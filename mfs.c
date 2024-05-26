#include "mfs.h"        // Header file for MFS functions and definitions
#include <arpa/inet.h>  // Definitions for internet operations
#include <netdb.h>      // Definitions for network database operations like getaddrinfo
#include <netinet/in.h> // Internet address family
#include <stdio.h>      // Standard I/O library
#include <stdlib.h>     // Standard library for memory allocation and process control
#include <string.h>     // String manipulation functions
#include <sys/socket.h> // Socket functions and data structures
#include <unistd.h>     // Standard symbolic constants and types

#define TIMEOUT 5 // Timeout for socket operations in seconds

int sockfd;                     // Socket file descriptor
struct sockaddr_in server_addr; // Server address structure

// Function to send a request to the server and receive a response
int send_receive(char *send_buffer, char *recv_buffer)
{
    struct sockaddr_in from;          // Address structure for the source of the response
    socklen_t fromlen = sizeof(from); // Length of the address structure
    fd_set read_fds;                  // File descriptor set for select
    struct timeval tv;                // Timeout structure

    int retries = 5; // Number of retries
    while (retries > 0)
    {
        // Send the request to the server
        if (sendto(sockfd, send_buffer, strlen(send_buffer), 0, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        {
            perror("sendto failed");
            return -1;
        }

        FD_ZERO(&read_fds);        // Clear the file descriptor set
        FD_SET(sockfd, &read_fds); // Add the socket to the set

        tv.tv_sec = TIMEOUT; // Set the timeout duration
        tv.tv_usec = 0;

        // Wait for a response with a timeout
        int rv = select(sockfd + 1, &read_fds, NULL, NULL, &tv);
        if (rv == -1)
        {
            perror("select failed");
            return -1;
        }
        else if (rv == 0)
        {
            // Timeout occurred, retry
            printf("Timeout, retrying...\n");
            retries--;
            continue;
        }
        else
        {
            // Receive the response from the server
            int len = recvfrom(sockfd, recv_buffer, MFS_BLOCK_SIZE, 0, (struct sockaddr *)&from, &fromlen);
            if (len < 0)
            {
                perror("recvfrom failed");
                return -1;
            }
            recv_buffer[len] = '\0'; // Null-terminate the received data
            return 0;
        }
    }

    return -1; // Exceeded retries
}

// Function to initialize the MFS client
int MFS_Init(char *hostname, int port)
{
    printf("Initializing with hostname: %s, port: %d\n", hostname, port);

    // Create a socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("socket creation failed");
        return -1;
    }

    // Initialize the server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;      // IPv4
    hints.ai_socktype = SOCK_DGRAM; // Datagram socket

    // Resolve the hostname to an IP address
    int err = getaddrinfo(hostname, NULL, &hints, &res);
    if (err != 0)
    {
        fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(err));
        return -1;
    }

    // Copy the resolved IP address to the server address structure
    struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;
    server_addr.sin_addr = ipv4->sin_addr;
    freeaddrinfo(res); // Free the address info structure

    return 0;
}

// Function to lookup a directory entry
int MFS_Lookup(int pinum, char *name)
{
    char send_buffer[BUFFER_SIZE];
    snprintf(send_buffer, BUFFER_SIZE, "LOOKUP %d %s", pinum, name); // Format the request

    char recv_buffer[BUFFER_SIZE];
    struct sockaddr_in response_addr;
    socklen_t addr_len = sizeof(response_addr);

    // Send the lookup request to the server
    sendto(sockfd, send_buffer, strlen(send_buffer), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    // Receive the response from the server
    recvfrom(sockfd, recv_buffer, BUFFER_SIZE, 0, (struct sockaddr *)&response_addr, &addr_len);

    int inum;
    sscanf(recv_buffer, "%d", &inum); // Parse the response to get the inode number
    return inum;
}

// Function to get the status of an inode
int MFS_Stat(int inum, MFS_Stat_t *m)
{
    char send_buffer[BUFFER_SIZE];
    snprintf(send_buffer, BUFFER_SIZE, "STAT %d", inum); // Format the request

    char recv_buffer[BUFFER_SIZE];
    struct sockaddr_in response_addr;
    socklen_t addr_len = sizeof(response_addr);

    // Send the stat request to the server
    sendto(sockfd, send_buffer, strlen(send_buffer), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    // Receive the response from the server
    recvfrom(sockfd, recv_buffer, BUFFER_SIZE, 0, (struct sockaddr *)&response_addr, &addr_len);

    int type, size;
    sscanf(recv_buffer, "%d %d", &type, &size); // Parse the response to get the inode type and size
    m->type = type;
    m->size = size;
    return 0;
}

// Function to write data to a file
int MFS_Write(int inum, char *buffer, int block)
{
    char send_buffer[BUFFER_SIZE + MFS_BLOCK_SIZE];
    snprintf(send_buffer, BUFFER_SIZE, "WRITE %d %d", inum, block);        // Format the request
    memcpy(send_buffer + strlen(send_buffer) + 1, buffer, MFS_BLOCK_SIZE); // Copy the data to be written

    char recv_buffer[BUFFER_SIZE];
    struct sockaddr_in response_addr;
    socklen_t addr_len = sizeof(response_addr);

    // Send the write request to the server
    sendto(sockfd, send_buffer, BUFFER_SIZE + MFS_BLOCK_SIZE, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    // Receive the response from the server
    recvfrom(sockfd, recv_buffer, BUFFER_SIZE, 0, (struct sockaddr *)&response_addr, &addr_len);

    int result;
    sscanf(recv_buffer, "%d", &result); // Parse the response to get the result
    return result;
}

// Function to read data from a file
int MFS_Read(int inum, char *buffer, int block)
{
    char send_buffer[BUFFER_SIZE];
    snprintf(send_buffer, BUFFER_SIZE, "READ %d %d", inum, block); // Format the request

    char recv_buffer[BUFFER_SIZE + MFS_BLOCK_SIZE];
    struct sockaddr_in response_addr;
    socklen_t addr_len = sizeof(response_addr);

    // Send the read request to the server
    sendto(sockfd, send_buffer, strlen(send_buffer), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    // Receive the response from the server
    recvfrom(sockfd, recv_buffer, BUFFER_SIZE + MFS_BLOCK_SIZE, 0, (struct sockaddr *)&response_addr, &addr_len);

    int result;
    sscanf(recv_buffer, "%d", &result); // Parse the response to get the result
    if (result == 0)
    {
        memcpy(buffer, recv_buffer + sizeof(int), MFS_BLOCK_SIZE); // Copy the data to the buffer if successful
    }
    return result;
}

// Function to create a new file or directory
int MFS_Creat(int pinum, int type, char *name)
{
    char send_buffer[BUFFER_SIZE];
    snprintf(send_buffer, BUFFER_SIZE, "CREAT %d %d %s", pinum, type, name); // Format the request

    char recv_buffer[BUFFER_SIZE];
    struct sockaddr_in response_addr;
    socklen_t addr_len = sizeof(response_addr);

    // Send the create request to the server
    sendto(sockfd, send_buffer, strlen(send_buffer), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    // Receive the response from the server
    recvfrom(sockfd, recv_buffer, BUFFER_SIZE, 0, (struct sockaddr *)&response_addr, &addr_len);

    int result;
    sscanf(recv_buffer, "%d", &result); // Parse the response to get the result
    return result;
}

// Function to unlink (delete) a file or directory
int MFS_Unlink(int pinum, char *name)
{
    char send_buffer[BUFFER_SIZE];
    snprintf(send_buffer, BUFFER_SIZE, "UNLINK %d %s", pinum, name); // Format the request

    char recv_buffer[BUFFER_SIZE];
    struct sockaddr_in response_addr;
    socklen_t addr_len = sizeof(response_addr);

    // Send the unlink request to the server
    sendto(sockfd, send_buffer, strlen(send_buffer), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    // Receive the response from the server
    recvfrom(sockfd, recv_buffer, BUFFER_SIZE, 0, (struct sockaddr *)&response_addr, &addr_len);

    int result;
    sscanf(recv_buffer, "%d", &result); // Parse the response to get the result
    return result;
}

// Function to shutdown the server
int MFS_Shutdown()
{
    char send_buffer[BUFFER_SIZE];
    snprintf(send_buffer, BUFFER_SIZE, "SHUTDOWN"); // Format the request

    char recv_buffer[BUFFER_SIZE];
    struct sockaddr_in response_addr;
    socklen_t addr_len = sizeof(response_addr);

    // Send the shutdown request to the server
    sendto(sockfd, send_buffer, strlen(send_buffer), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    // Receive the response from the server
    recvfrom(sockfd, recv_buffer, BUFFER_SIZE, 0, (struct sockaddr *)&response_addr, &addr_len);

    int result;
    sscanf(recv_buffer, "%d", &result); // Parse the response to get the result
    return result;
}
