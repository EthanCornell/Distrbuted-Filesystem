
#include "ufs.h"        // Custom header file for file system structures and definitions
#include <assert.h>     // Assert function for error checking
#include <errno.h>      // Error number definitions
#include <fcntl.h>      // File control options
#include <netinet/in.h> // Internet address family structures
#include <stdio.h>      // Standard input/output library
#include <stdlib.h>     // Standard library for memory allocation and process control
#include <string.h>     // String handling functions
#include <sys/socket.h> // Socket functions
#include <unistd.h>     // Standard symbolic constants and types

#define PORT 12345
#define BUFFER_SIZE 4096 // Match BUFFER_SIZE with UFS_BLOCK_SIZE

typedef struct
{
    super_t superblock;   // Superblock of the file system
    inode_t inodes[4096]; // In-memory inode table
} fs_state_t;

fs_state_t fs_state; // Global file system state
int fd;              // File descriptor for the file system image

// Function to initialize or load the file system
void init_or_load_fs(const char *fs_image);

// Function to process incoming requests
void process_request(char *buffer, int sockfd, struct sockaddr_in client_addr, socklen_t addr_size);

// Helper functions for different file operations
int handle_lookup(int pinum, char *name);
int handle_stat(int inum, inode_t *inode);
int handle_write(int inum, char *buffer, int block);
int handle_read(int inum, char *buffer, int block);
int handle_creat(int pinum, int type, char *name);
int handle_unlink(int pinum, char *name);

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s [portnum] [file-system-image]\n", argv[0]);
        exit(1);
    }

    int port = atoi(argv[1]); // Convert port number from string to integer
    char *fs_image = argv[2]; // Get file system image file name from command line arguments

    // Initialize or load the file system image
    init_or_load_fs(fs_image);

    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    char buffer[BUFFER_SIZE];
    socklen_t addr_size;

    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr)); // Clear server address structure
    memset(&client_addr, 0, sizeof(client_addr)); // Clear client address structure

    // Fill server information
    server_addr.sin_family = AF_INET;         // IPv4
    server_addr.sin_addr.s_addr = INADDR_ANY; // Any incoming interface
    server_addr.sin_port = htons(port);       // Port number in network byte order

    // Bind the socket with the server address
    if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    printf("UDP Server listening on port %d\n", port);

    while (1)
    {
        addr_size = sizeof(client_addr);
        int len = recvfrom(sockfd, (char *)buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &addr_size);
        buffer[len] = '\0'; // Null-terminate the received message
        printf("Received: %s\n", buffer);

        // Process received message and prepare a response
        process_request(buffer, sockfd, client_addr, addr_size);
    }

    return 0;
}

void init_or_load_fs(const char *fs_image)
{
    fd = open(fs_image, O_RDWR | O_CREAT, 0666); // Open or create the file system image with read-write permissions
    if (fd < 0)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }

    off_t size = lseek(fd, 0, SEEK_END); // Seek to the end of the file to check its size
    if (size == 0)
    {
        // Initialization of the File System Image:
        printf("Initializing file system image...\n");

        super_t *s = &fs_state.superblock;
        int num_inodes = 32; // Number of inodes
        int num_data = 32;   // Number of data blocks

        unsigned char *empty_buffer = calloc(UFS_BLOCK_SIZE, 1); // Allocate zeroed buffer for initialization
        if (empty_buffer == NULL)
        {
            perror("calloc");
            exit(1);
        }

        int bits_per_block = (8 * UFS_BLOCK_SIZE); // Bits per block (8 bits per byte)

        // Set up superblock fields
        s->num_inodes = num_inodes;
        s->num_data = num_data;
        s->inode_bitmap_addr = 1;                          // Inode bitmap starts at block 1
        s->inode_bitmap_len = num_inodes / bits_per_block; // Length of inode bitmap in blocks
        if (num_inodes % bits_per_block != 0)
            s->inode_bitmap_len++;

        s->data_bitmap_addr = s->inode_bitmap_addr + s->inode_bitmap_len;
        s->data_bitmap_len = num_data / bits_per_block;
        if (num_data % bits_per_block != 0)
            s->data_bitmap_len++;

        s->inode_region_addr = s->data_bitmap_addr + s->data_bitmap_len;
        int total_inode_bytes = num_inodes * sizeof(inode_t);
        s->inode_region_len = total_inode_bytes / UFS_BLOCK_SIZE;
        if (total_inode_bytes % UFS_BLOCK_SIZE != 0)
            s->inode_region_len++;

        s->data_region_addr = s->inode_region_addr + s->inode_region_len;
        s->data_region_len = num_data;

        int total_blocks = 1 + s->inode_bitmap_len + s->data_bitmap_len + s->inode_region_len + s->data_region_len;

        // Write superblock to disk
        int rc = pwrite(fd, s, sizeof(super_t), 0);
        if (rc != sizeof(super_t))
        {
            perror("write");
            exit(1);
        }

        // Zero out all blocks
        for (int i = 1; i < total_blocks; i++)
        {
            rc = pwrite(fd, empty_buffer, UFS_BLOCK_SIZE, i * UFS_BLOCK_SIZE);
            if (rc != UFS_BLOCK_SIZE)
            {
                perror("write");
                exit(1);
            }
        }

        // Bitmap initialization
        typedef struct
        {
            unsigned int bits[UFS_BLOCK_SIZE / sizeof(unsigned int)];
        } bitmap_t;
        bitmap_t b;
        for (int i = 0; i < 1024; i++)
            b.bits[i] = 0;
        b.bits[0] = 0x1 << 31; // First entry is allocated

        rc = pwrite(fd, &b, UFS_BLOCK_SIZE, s->inode_bitmap_addr * UFS_BLOCK_SIZE);
        assert(rc == UFS_BLOCK_SIZE);

        rc = pwrite(fd, &b, UFS_BLOCK_SIZE, s->data_bitmap_addr * UFS_BLOCK_SIZE);
        assert(rc == UFS_BLOCK_SIZE);

        // Inode table initialization
        typedef struct
        {
            inode_t inodes[UFS_BLOCK_SIZE / sizeof(inode_t)];
        } inode_block;

        inode_block itable;
        itable.inodes[0].type = UFS_DIRECTORY;
        itable.inodes[0].size = 2 * sizeof(dir_ent_t);
        itable.inodes[0].direct[0] = s->data_region_addr;
        for (int i = 1; i < DIRECT_PTRS; i++)
            itable.inodes[0].direct[i] = -1;

        rc = pwrite(fd, &itable, UFS_BLOCK_SIZE, s->inode_region_addr * UFS_BLOCK_SIZE);
        assert(rc == UFS_BLOCK_SIZE);

        // Root directory initialization
        typedef struct
        {
            dir_ent_t entries[128];
        } dir_block_t;
        dir_block_t parent;
        strcpy(parent.entries[0].name, ".");
        parent.entries[0].inum = 0;

        strcpy(parent.entries[1].name, "..");
        parent.entries[1].inum = 0;

        for (int i = 2; i < 128; i++)
            parent.entries[i].inum = -1;

        rc = pwrite(fd, &parent, UFS_BLOCK_SIZE, s->data_region_addr * UFS_BLOCK_SIZE);
        assert(rc == UFS_BLOCK_SIZE);

        free(empty_buffer);
        (void)fsync(fd);
    }
    else
    {
        // Load Existing File System Image:
        printf("Loading file system image...\n");

        lseek(fd, 0, SEEK_SET);                                   // Seek to the beginning of the file
        int rc = read(fd, &fs_state.superblock, sizeof(super_t)); // Read the superblock
        if (rc != sizeof(super_t))
        {
            perror("read");
            exit(1);
        }

        // Read the inode table
        for (int i = 0; i < fs_state.superblock.num_inodes; i++)
        {
            lseek(fd, (fs_state.superblock.inode_region_addr * UFS_BLOCK_SIZE) + (i * sizeof(inode_t)), SEEK_SET);
            rc = read(fd, &fs_state.inodes[i], sizeof(inode_t));
            if (rc != sizeof(inode_t))
            {
                perror("read");
                exit(1);
            }
        }
    }
}

// Helper function to handle LOOKUP request
int handle_lookup(int pinum, char *name)
{
    if (pinum < 0 || pinum >= (int)fs_state.superblock.num_inodes)
    {
        printf("Invalid pinum: %d\n", pinum);
        return -1; // Invalid pinum
    }

    inode_t *dir_inode = &fs_state.inodes[pinum];
    if (dir_inode->type != UFS_DIRECTORY)
    {
        printf("Not a directory: %d\n", pinum);
        return -1; // Not a directory
    }

    // Iterate over the directory entries to find the matching name
    for (int i = 0; i < DIRECT_PTRS && (int)dir_inode->direct[i] != -1; i++)
    {
        typedef struct
        {
            dir_ent_t entries[128];
        } dir_block_t;
        dir_block_t dir_block;
        lseek(fd, dir_inode->direct[i] * UFS_BLOCK_SIZE, SEEK_SET); // Seek to the data block
        read(fd, &dir_block, sizeof(dir_block_t));                  // Read the directory block

        for (int j = 0; j < 128; j++)
        {
            if (dir_block.entries[j].inum != -1 && strcmp(dir_block.entries[j].name, name) == 0)
            {
                return dir_block.entries[j].inum; // Return the inode number
            }
        }
    }

    printf("Name not found: %s\n", name);
    return -1; // Name not found
}

// Helper function to handle STAT request
int handle_stat(int inum, inode_t *inode)
{
    if (inum < 0 || inum >= (int)fs_state.superblock.num_inodes)
    {
        return -1; // Invalid inum
    }

    *inode = fs_state.inodes[inum];
    return 0;
}

// Helper function to handle WRITE request
int handle_write(int inum, char *buffer, int block)
{
    if (inum < 0 || inum >= (int)fs_state.superblock.num_inodes)
    {
        return -1; // Invalid inum
    }

    inode_t *inode = &fs_state.inodes[inum];
    if (inode->type != UFS_REGULAR_FILE)
    {
        return -1; // Not a regular file
    }

    if (block < 0 || (unsigned int)block >= DIRECT_PTRS)
    {
        return -1; // Invalid block number
    }

    // Allocate a new block if necessary
    if ((int)inode->direct[block] == -1)
    {
        inode->direct[block] = fs_state.superblock.data_region_addr + inode->size / UFS_BLOCK_SIZE;
        inode->size += UFS_BLOCK_SIZE;
    }

    // Write the data to the allocated block
    pwrite(fd, buffer, UFS_BLOCK_SIZE, inode->direct[block] * UFS_BLOCK_SIZE);
    fsync(fd); // Force the data to be written to disk

    return 0;
}

// Helper function to handle READ request
int handle_read(int inum, char *buffer, int block)
{
    if (inum < 0 || inum >= (int)fs_state.superblock.num_inodes)
    {
        return -1; // Invalid inum
    }

    inode_t *inode = &fs_state.inodes[inum];
    if (block < 0 || (unsigned int)block >= DIRECT_PTRS || (int)inode->direct[block] == -1)
    {
        return -1; // Invalid block number or unallocated block
    }

    // Read the data from the specified block
    pread(fd, buffer, UFS_BLOCK_SIZE, inode->direct[block] * UFS_BLOCK_SIZE);
    return 0;
}

// Helper function to handle CREAT request
int handle_creat(int pinum, int type, char *name)
{
    if (pinum < 0 || pinum >= (int)fs_state.superblock.num_inodes)
    {
        printf("Invalid pinum: %d\n", pinum);
        return -1; // Invalid pinum
    }

    inode_t *dir_inode = &fs_state.inodes[pinum];
    if (dir_inode->type != UFS_DIRECTORY)
    {
        printf("Not a directory: %d\n", pinum);
        return -1; // Not a directory
    }

    // Check if the name already exists
    int existing_inum = handle_lookup(pinum, name);
    if (existing_inum != -1)
    {
        printf("Name already exists: %s\n", name);
        return 0; // Name already exists
    }

    // Find an empty inode
    int new_inum = -1;
    for (int i = 0; i < (int)fs_state.superblock.num_inodes; i++)
    {
        if (fs_state.inodes[i].type == -1)
        {
            new_inum = i;
            break;
        }
    }

    if (new_inum == -1)
    {
        printf("No empty inode available\n");
        return -1; // No empty inode available
    }

    // Initialize the new inode
    inode_t *new_inode = &fs_state.inodes[new_inum];
    new_inode->type = type;
    new_inode->size = 0;
    memset(new_inode->direct, -1, sizeof(new_inode->direct));

    // Add the new entry to the parent directory
    for (int i = 0; i < DIRECT_PTRS && (int)dir_inode->direct[i] != -1; i++)
    {
        typedef struct
        {
            dir_ent_t entries[128];
        } dir_block_t;
        dir_block_t dir_block;
        lseek(fd, dir_inode->direct[i] * UFS_BLOCK_SIZE, SEEK_SET);
        read(fd, &dir_block, sizeof(dir_block_t));

        for (int j = 0; j < 128; j++)
        {
            if (dir_block.entries[j].inum == -1)
            {
                strcpy(dir_block.entries[j].name, name);
                dir_block.entries[j].inum = new_inum;
                pwrite(fd, &dir_block, sizeof(dir_block_t), dir_inode->direct[i] * UFS_BLOCK_SIZE);
                fsync(fd); // Force the data to be written to disk
                return 0;
            }
        }
    }

    printf("Directory is full\n");
    return -1; // Directory is full
}

// Helper function to handle UNLINK request
int handle_unlink(int pinum, char *name)
{
    if (pinum < 0 || pinum >= (int)fs_state.superblock.num_inodes)
    {
        return -1; // Invalid pinum
    }

    inode_t *dir_inode = &fs_state.inodes[pinum];
    if (dir_inode->type != UFS_DIRECTORY)
    {
        return -1; // Not a directory
    }

    // Find the directory entry
    for (int i = 0; i < DIRECT_PTRS && (int)dir_inode->direct[i] != -1; i++)
    {
        typedef struct
        {
            dir_ent_t entries[128];
        } dir_block_t;
        dir_block_t dir_block;
        lseek(fd, dir_inode->direct[i] * UFS_BLOCK_SIZE, SEEK_SET);
        read(fd, &dir_block, sizeof(dir_block_t));

        for (int j = 0; j < 128; j++)
        {
            if (dir_block.entries[j].inum != -1 && strcmp(dir_block.entries[j].name, name) == 0)
            {
                int inum = dir_block.entries[j].inum;
                dir_block.entries[j].inum = -1;
                pwrite(fd, &dir_block, sizeof(dir_block_t), dir_inode->direct[i] * UFS_BLOCK_SIZE);
                fsync(fd); // Force the data to be written to disk

                // Mark the inode as free
                fs_state.inodes[inum].type = -1;
                return 0;
            }
        }
    }

    return -1; // Name not found
}

// Function to process incoming requests
void process_request(char *buffer, int sockfd, struct sockaddr_in client_addr, socklen_t addr_size)
{
    char command[BUFFER_SIZE];
    sscanf(buffer, "%s", command); // Extract the command from the buffer

    char response[BUFFER_SIZE];
    memset(response, 0, BUFFER_SIZE); // Clear the response buffer

    if (strcmp(command, "LOOKUP") == 0)
    {
        int pinum;
        char name[28];
        sscanf(buffer + strlen(command) + 1, "%d %s", &pinum, name);
        int inum = handle_lookup(pinum, name);
        snprintf(response, BUFFER_SIZE, "%d", inum);
    }
    else if (strcmp(command, "STAT") == 0)
    {
        int inum;
        sscanf(buffer + strlen(command) + 1, "%d", &inum);
        inode_t inode;
        int rc = handle_stat(inum, &inode);
        if (rc == 0)
        {
            snprintf(response, BUFFER_SIZE, "%d %d %u", inode.type, inode.size, inode.direct[0]);
        }
        else
        {
            snprintf(response, BUFFER_SIZE, "%d", rc);
        }
    }
    else if (strcmp(command, "WRITE") == 0)
    {
        int inum, block;
        char write_buffer[UFS_BLOCK_SIZE];
        sscanf(buffer + strlen(command) + 1, "%d %d", &inum, &block);
        memcpy(write_buffer, buffer + strlen(command) + 1 + sizeof(int) * 2, UFS_BLOCK_SIZE);
        int rc = handle_write(inum, write_buffer, block);
        snprintf(response, BUFFER_SIZE, "%d", rc);
    }
    else if (strcmp(command, "READ") == 0)
    {
        int inum, block;
        char read_buffer[UFS_BLOCK_SIZE];
        sscanf(buffer + strlen(command) + 1, "%d %d", &inum, &block);
        int rc = handle_read(inum, read_buffer, block);
        if (rc == 0)
        {
            memcpy(response, read_buffer, UFS_BLOCK_SIZE);
        }
        else
        {
            snprintf(response, BUFFER_SIZE, "%d", rc);
        }
    }
    else if (strcmp(command, "CREAT") == 0)
    {
        int pinum, type;
        char name[28];
        sscanf(buffer + strlen(command) + 1, "%d %d %s", &pinum, &type, name);
        int rc = handle_creat(pinum, type, name);
        snprintf(response, BUFFER_SIZE, "%d", rc);
    }
    else if (strcmp(command, "UNLINK") == 0)
    {
        int pinum;
        char name[28];
        sscanf(buffer + strlen(command) + 1, "%d %s", &pinum, name);
        int rc = handle_unlink(pinum, name);
        snprintf(response, BUFFER_SIZE, "%d", rc);
    }
    else if (strcmp(command, "SHUTDOWN") == 0)
    {
        fsync(fd); // Force all data to be written to disk
        snprintf(response, BUFFER_SIZE, "0");
        sendto(sockfd, response, strlen(response), 0, (const struct sockaddr *)&client_addr, addr_size);
        exit(0); // Shutdown the server
    }
    else
    {
        // Unknown command
        snprintf(response, BUFFER_SIZE, "Unknown command");
    }

    sendto(sockfd, response, strlen(response), 0, (const struct sockaddr *)&client_addr, addr_size);
}
