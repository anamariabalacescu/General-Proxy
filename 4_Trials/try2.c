#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int main(int argc, char *argv[]) {
    // if (argc != 2) {
    //     fprintf(stderr, "Usage: %s <PORT>\n", argv[0]);
    //     return 1;
    // }

    //int port = atoi(argv[1]);
    
    //int server_port = atoi(argv[1]);
    int port = atoi(argv[1]);//444531;

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        return 1;
    }

    if (listen(server_socket, 5) == -1) {
        perror("listen");
        return 1;
    }

    //printf("Server listening on port %d...\n", port);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket == -1) {
            perror("accept");
            continue;
        }

        char buffer[1024];
        ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytes_received == -1) {
            perror("recv");
            close(client_socket);
            continue;
        }

        if (bytes_received == 0) {
            printf("Client closed the connection\n");
        } else {
            buffer[bytes_received] = '\0';
            printf("Received: %s\n", buffer);

            const char *response = "Server received your message";
            ssize_t bytes_sent = send(client_socket, response, strlen(response), 0);
            if (bytes_sent == -1) {
                perror("send");
            }
        }

        close(client_socket);
    }

    close(server_socket);

    return 0;
}