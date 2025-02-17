#ifndef RTN_CLIENT_H
#define RTN_CLIENT_H

#include "rtn_base.h"

#define PORT "12345"   // Port for signaling
#define BUFFER_SIZE 256

void
do_client()
{
    struct addrinfo hints, *res;
    int client_fd;
    char buffer[BUFFER_SIZE];

    // Set up hints for getaddrinfo
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;       // IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;  // TCP

    char *host = "localhost";
    if (getaddrinfo(host, PORT, &hints, &res) != 0) {
        perror("getaddrinfo");
        exit(1);
    }

    // Create socket
    client_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (client_fd == -1) {
        perror("socket");
        freeaddrinfo(res);
        exit(1);
    }

    // Connect to server
    if (connect(client_fd, res->ai_addr, res->ai_addrlen) == -1) {
        perror("connect");
        close(client_fd);
        freeaddrinfo(res);
        exit(1);
    }

    freeaddrinfo(res);

    // Send start message
    strcpy(buffer, "START");
    send(client_fd, buffer, strlen(buffer), 0);

    // Receive acknowledgment
    memset(buffer, 0, sizeof(buffer));
    recv(client_fd, buffer, sizeof(buffer), 0);
    printf("Received from server: %s\n", buffer);

    // Simulating test phase...
    sleep(5);

    // Send end message
    strcpy(buffer, "END");
    send(client_fd, buffer, strlen(buffer), 0);

    // Receive acknowledgment
    memset(buffer, 0, sizeof(buffer));
    recv(client_fd, buffer, sizeof(buffer), 0);
    printf("Received from server: %s\n", buffer);

    close(client_fd);
}

#endif // RTN_CLIENT_H