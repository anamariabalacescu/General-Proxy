#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#define MAX_BUFFER_SIZE 8192

void forward_data(int client_socket, int server_socket) {
    char buffer[MAX_BUFFER_SIZE];
    int bytes_received;

    while (1) {
        bytes_received = recv(client_socket, buffer, MAX_BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            break;
        }

        send(server_socket, buffer, bytes_received, 0);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }

    int proxy_port = atoi(argv[1]);
    int proxy_socket, client_socket, server_socket;
    struct sockaddr_in proxy_addr, client_addr, server_addr;
    socklen_t client_len = sizeof(client_addr);

    proxy_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (proxy_socket < 0) {
        perror("Error creating proxy socket");
        exit(1);
    }

    memset(&proxy_addr, 0, sizeof(proxy_addr));
    proxy_addr.sin_family = AF_INET;
    proxy_addr.sin_addr.s_addr = INADDR_ANY;
    proxy_addr.sin_port = htons(proxy_port);

    if (bind(proxy_socket, (struct sockaddr *)&proxy_addr, sizeof(proxy_addr)) < 0) {
        perror("Error binding proxy socket");
        exit(1);
    }

    if (listen(proxy_socket, 10) < 0) {
        perror("Error listening for connections");
        exit(1);
    }

    printf("Proxy server listening on port %d...\n", proxy_port);

    while (1) {
        client_socket = accept(proxy_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0) {
            perror("Error accepting client connection");
            continue;
        }

        server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket < 0) {
            perror("Error creating server socket");
            close(client_socket);
            continue;
        }

        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = inet_addr("TARGET_SERVER_IP");
        server_addr.sin_port = htons(8081);//(TARGET_SERVER_PORT);

        if (connect(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("Error connecting to the target server");
            close(client_socket);
            close(server_socket);
            continue;
        }

        forward_data(client_socket, server_socket);

        close(client_socket);
        close(server_socket);
    }

    close(proxy_socket);

    return 0;
}