#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>  // Include this header for getaddrinfo and struct addrinfo
#include "mfs.h"

#define TIMEOUT 5 // seconds

int sockfd;
struct sockaddr_in server_addr;

int send_receive(char *send_buffer, char *recv_buffer) {
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);
    fd_set read_fds;
    struct timeval tv;

    int retries = 5; // Number of retries
    while (retries > 0) {
        if (sendto(sockfd, send_buffer, strlen(send_buffer), 0, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("sendto failed");
            return -1;
        }

        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);

        tv.tv_sec = TIMEOUT;
        tv.tv_usec = 0;

        int rv = select(sockfd + 1, &read_fds, NULL, NULL, &tv);
        if (rv == -1) {
            perror("select failed");
            return -1;
        } else if (rv == 0) {
            printf("Timeout, retrying...\n");
            retries--;
            continue;
        } else {
            int len = recvfrom(sockfd, recv_buffer, MFS_BLOCK_SIZE, 0, (struct sockaddr *)&from, &fromlen);
            if (len < 0) {
                perror("recvfrom failed");
                return -1;
            }
            recv_buffer[len] = '\0';
            return 0;
        }
    }

    return -1; // Exceeded retries
}

int MFS_Init(char *hostname, int port) {
    printf("Initializing with hostname: %s, port: %d\n", hostname, port);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        return -1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    int err = getaddrinfo(hostname, NULL, &hints, &res);
    if (err != 0) {
        fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(err));
        return -1;
    }

    struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;
    server_addr.sin_addr = ipv4->sin_addr;
    freeaddrinfo(res);

    return 0;
}

int MFS_Lookup(int pinum, char *name) {
    char send_buffer[BUFFER_SIZE];
    snprintf(send_buffer, BUFFER_SIZE, "LOOKUP %d %s", pinum, name);

    char recv_buffer[BUFFER_SIZE];
    struct sockaddr_in response_addr;
    socklen_t addr_len = sizeof(response_addr);

    sendto(sockfd, send_buffer, strlen(send_buffer), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    recvfrom(sockfd, recv_buffer, BUFFER_SIZE, 0, (struct sockaddr *)&response_addr, &addr_len);

    int inum;
    sscanf(recv_buffer, "%d", &inum);
    return inum;
}

int MFS_Stat(int inum, MFS_Stat_t *m) {
    char send_buffer[BUFFER_SIZE];
    snprintf(send_buffer, BUFFER_SIZE, "STAT %d", inum);

    char recv_buffer[BUFFER_SIZE];
    struct sockaddr_in response_addr;
    socklen_t addr_len = sizeof(response_addr);

    sendto(sockfd, send_buffer, strlen(send_buffer), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    recvfrom(sockfd, recv_buffer, BUFFER_SIZE, 0, (struct sockaddr *)&response_addr, &addr_len);

    int type, size;
    sscanf(recv_buffer, "%d %d", &type, &size);
    m->type = type;
    m->size = size;
    return 0;
}

int MFS_Write(int inum, char *buffer, int block) {
    char send_buffer[BUFFER_SIZE + MFS_BLOCK_SIZE];
    snprintf(send_buffer, BUFFER_SIZE, "WRITE %d %d", inum, block);
    memcpy(send_buffer + strlen(send_buffer) + 1, buffer, MFS_BLOCK_SIZE);

    char recv_buffer[BUFFER_SIZE];
    struct sockaddr_in response_addr;
    socklen_t addr_len = sizeof(response_addr);

    sendto(sockfd, send_buffer, BUFFER_SIZE + MFS_BLOCK_SIZE, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    recvfrom(sockfd, recv_buffer, BUFFER_SIZE, 0, (struct sockaddr *)&response_addr, &addr_len);

    int result;
    sscanf(recv_buffer, "%d", &result);
    return result;
}

int MFS_Read(int inum, char *buffer, int block) {
    char send_buffer[BUFFER_SIZE];
    snprintf(send_buffer, BUFFER_SIZE, "READ %d %d", inum, block);

    char recv_buffer[BUFFER_SIZE + MFS_BLOCK_SIZE];
    struct sockaddr_in response_addr;
    socklen_t addr_len = sizeof(response_addr);

    sendto(sockfd, send_buffer, strlen(send_buffer), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    recvfrom(sockfd, recv_buffer, BUFFER_SIZE + MFS_BLOCK_SIZE, 0, (struct sockaddr *)&response_addr, &addr_len);

    int result;
    sscanf(recv_buffer, "%d", &result);
    if (result == 0) {
        memcpy(buffer, recv_buffer + sizeof(int), MFS_BLOCK_SIZE);
    }
    return result;
}

int MFS_Creat(int pinum, int type, char *name) {
    char send_buffer[BUFFER_SIZE];
    snprintf(send_buffer, BUFFER_SIZE, "CREAT %d %d %s", pinum, type, name);

    char recv_buffer[BUFFER_SIZE];
    struct sockaddr_in response_addr;
    socklen_t addr_len = sizeof(response_addr);

    sendto(sockfd, send_buffer, strlen(send_buffer), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    recvfrom(sockfd, recv_buffer, BUFFER_SIZE, 0, (struct sockaddr *)&response_addr, &addr_len);

    int result;
    sscanf(recv_buffer, "%d", &result);
    return result;
}

int MFS_Unlink(int pinum, char *name) {
    char send_buffer[BUFFER_SIZE];
    snprintf(send_buffer, BUFFER_SIZE, "UNLINK %d %s", pinum, name);

    char recv_buffer[BUFFER_SIZE];
    struct sockaddr_in response_addr;
    socklen_t addr_len = sizeof(response_addr);

    sendto(sockfd, send_buffer, strlen(send_buffer), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    recvfrom(sockfd, recv_buffer, BUFFER_SIZE, 0, (struct sockaddr *)&response_addr, &addr_len);

    int result;
    sscanf(recv_buffer, "%d", &result);
    return result;
}

int MFS_Shutdown() {
    char send_buffer[BUFFER_SIZE];
    snprintf(send_buffer, BUFFER_SIZE, "SHUTDOWN");

    char recv_buffer[BUFFER_SIZE];
    struct sockaddr_in response_addr;
    socklen_t addr_len = sizeof(response_addr);

    sendto(sockfd, send_buffer, strlen(send_buffer), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    recvfrom(sockfd, recv_buffer, BUFFER_SIZE, 0, (struct sockaddr *)&response_addr, &addr_len);

    int result;
    sscanf(recv_buffer, "%d", &result);
    return result;
}