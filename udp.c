#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 12345
#define BUFFER_SIZE 1024

int main() {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    char buffer[BUFFER_SIZE];
    socklen_t addr_size;

    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    memset(&client_addr, 0, sizeof(client_addr));

    // Fill server information
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind the socket with the server address
    if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    printf("UDP Server listening on port %d\n", PORT);

    while (1) {
        addr_size = sizeof(client_addr);
        int len = recvfrom(sockfd, (char *)buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &addr_size);
        buffer[len] = '\0';
        printf("Received: %s\n", buffer);

        // Process received message and prepare a response
        // For now, just echo back the received message
        sendto(sockfd, (const char *)buffer, strlen(buffer), 0, (const struct sockaddr *)&client_addr, addr_size);
    }

    return 0;
}
