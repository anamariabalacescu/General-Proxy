#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define MAX_BUFFER_SIZE 1024

void error(const char *msg) {
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[]) {
    /*if (argc != 4) {
        fprintf(stderr, "Usage: %s <local_port> <remote_host> <remote_port>\n", argv[0]);
        exit(1);
    }*/

    int local_port = 445367;//atoi(argv[1]);
    const char *remote_host = "192.168.84.112";//argv[2];
    int remote_port = 80;//atoi(argv[3]);

    // socket for local port
    int local_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (local_socket < 0) {
        error("Error opening socket");
    }

    struct sockaddr_in local_addr, remote_addr;
    bzero((char *) &local_addr, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = htons(local_port);

    if (bind(local_socket, (struct sockaddr *) &local_addr, sizeof(local_addr)) < 0) {
        error("Error on binding");
    }

    listen(local_socket, 5);

    int remote_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (remote_socket < 0) {
        error("Error opening remote socket");
    }

    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(remote_port);
    if (inet_pton(AF_INET, remote_host, &remote_addr.sin_addr) <= 0) {
        error("Error resolving remote host");
    }

    if (connect(remote_socket, (struct sockaddr *) &remote_addr, sizeof(remote_addr)) < 0) {
        perror("Error connecting to remote server");
        exit(1);
    }

    char buffer[MAX_BUFFER_SIZE];
    int n;
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_socket = accept(local_socket, (struct sockaddr *) &client_addr, &client_len);
        if (client_socket < 0) {
            error("Error accepting connection");
        }

        while ((n = read(client_socket, buffer, MAX_BUFFER_SIZE)) > 0) {
            write(remote_socket, buffer, n);
        }

        while ((n = read(remote_socket, buffer, MAX_BUFFER_SIZE)) > 0) {
            write(client_socket, buffer, n);
        }

        close(client_socket);
    }

    close(local_socket);
    close(remote_socket);

    return 0;
}