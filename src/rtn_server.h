#ifndef RTN_SERVER_H
#define RTN_SERVER_H

#include "rtn_base.h"

#define PORT "12345"   // Port for signaling
#define BACKLOG 5      // Maximum pending connections queue size
#define BUFFER_SIZE 256

static void 
handle_client(int client_fd) 
{
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));

    // Receive start message
    if (recv(client_fd, buffer, sizeof(buffer), 0) <= 0) {
        perror("recv");
        close(client_fd);
        return;
    }

    debug("Received from client: %s\n", buffer);

    if (strcmp(buffer, "START") == 0) {
        info("Test started!\n");
        // Acknowledge start
        send(client_fd, "START_ACK", 9, 0);
    } else {
        warn("Unexpected message: %s\n", buffer);
        close(client_fd);
        return;
    }

    // Simulating test phase...
    sleep(5);

    // Receive end message
    memset(buffer, 0, sizeof(buffer));
    if (recv(client_fd, buffer, sizeof(buffer), 0) <= 0) {
        perror("recv");
        close(client_fd);
        return;
    }

    debug("Received from client: %s\n", buffer);

    if (strcmp(buffer, "END") == 0) {
        info("Test ended!\n");
        // Acknowledge end
        send(client_fd, "END_ACK", 7, 0);
    } else {
        warn("Unexpected message: %s\n", buffer);
    }

    close(client_fd);
}

static int 
do_server()
{
    debug("Starting server...\n");

    struct addrinfo hints = {
        .ai_family   = AF_UNSPEC,       // IPv4 or IPv6
        .ai_socktype = SOCK_STREAM,     // TCP
        .ai_flags    = AI_PASSIVE,      // Use my IP
    };
    
    struct addrinfo *res;
    if (getaddrinfo(NULL, PORT, &hints, &res) != 0) {
        perror("getaddrinfo");
    }

    debug("Creating socket...\n");

    int server_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (server_fd == -1) {
        perror("socket");
        freeaddrinfo(res);
        exit(1);
    }

    if (bind(server_fd, res->ai_addr, res->ai_addrlen) == -1) {
        perror("bind");
        close(server_fd);
        freeaddrinfo(res);
        exit(1);
    }

    freeaddrinfo(res);

    if (listen(server_fd, BACKLOG) == -1) {
        perror("listen");
        close(server_fd);
        exit(1);
    }

    printf("Server is listening on port %s...\n", PORT);

    socklen_t addr_size;
    char client_ip[INET6_ADDRSTRLEN];

    int client_fd;
    struct sockaddr_storage client_addr;
    addr_size = sizeof(client_addr);
    while ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_size)) != -1) {
        inet_ntop(client_addr.ss_family,
                  (client_addr.ss_family == AF_INET)
                      ? (void *)&((struct sockaddr_in *)&client_addr)->sin_addr
                      : (void *)&((struct sockaddr_in6 *)&client_addr)->sin6_addr,
                  client_ip, sizeof(client_ip));
        printf("Connection from %s\n", client_ip);

        handle_client(client_fd);
    }

    close(server_fd);

    return 0;
}

#endif // RTN_SERVER_H