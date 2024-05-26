#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mfs.h"

int main() {
    // Initialize the client library with server hostname and port
    if (MFS_Init("localhost", 12345) != 0) {
        fprintf(stderr, "Failed to initialize MFS client\n");
        return 1;
    }

    // Create a new directory in the root directory
    if (MFS_Creat(0, MFS_DIRECTORY, "newdir") != 0) {
        fprintf(stderr, "Failed to create directory\n");
    } else {
        printf("Directory 'newdir' created successfully\n");
    }

    // Lookup the inode number of the newly created directory
    int inum = MFS_Lookup(0, "newdir");
    if (inum < 0) {
        fprintf(stderr, "Failed to lookup 'newdir'\n");
    } else {
        printf("Inode number of 'newdir' is %d\n", inum);
    }

    // Stat the newly created directory
    MFS_Stat_t m;
    if (MFS_Stat(inum, &m) != 0) {
        fprintf(stderr, "Failed to stat 'newdir'\n");
    } else {
        printf("Directory 'newdir' has size %d and type %d\n", m.size, m.type);
    }

    // Create a new file in the newly created directory
    if (MFS_Creat(inum, MFS_REGULAR_FILE, "newfile") != 0) {
        fprintf(stderr, "Failed to create file\n");
    } else {
        printf("File 'newfile' created successfully\n");
    }

    // Write data to the new file
    char buffer[MFS_BLOCK_SIZE];
    strcpy(buffer, "Hello, world!");
    if (MFS_Write(inum, buffer, 0) != 0) {
        fprintf(stderr, "Failed to write to 'newfile'\n");
    } else {
        printf("Data written to 'newfile' successfully\n");
    }

    // Read data from the new file
    char read_buffer[MFS_BLOCK_SIZE];
    if (MFS_Read(inum, read_buffer, 0) != 0) {
        fprintf(stderr, "Failed to read from 'newfile'\n");
    } else {
        printf("Data read from 'newfile': %s\n", read_buffer);
    }

    // Shutdown the server
    if (MFS_Shutdown() != 0) {
        fprintf(stderr, "Failed to shutdown MFS server\n");
    } else {
        printf("MFS server shutdown successfully\n");
    }

    return 0;
}
