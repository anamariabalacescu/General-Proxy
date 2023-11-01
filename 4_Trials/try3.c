#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ifaddrs.h>

#define BUFFER_SIZE 1024

void *handle_client(void *arg);
char serverIp[INET_ADDRSTRLEN];
int serverPort = 8080;

bool setServerConn()
{
    struct ifaddrs *ifap, *ifa;
    struct sockaddr_in *sa;

    if (getifaddrs(&ifap) == -1) {
        perror("getifaddrs");
        return 0;
    }

    for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr != NULL && ifa->ifa_addr->sa_family == AF_INET) {
            sa = (struct sockaddr_in *)ifa->ifa_addr;
            const char *ipAddress = inet_ntoa(sa->sin_addr);
            if (ipAddress != NULL) {
                strncpy(serverIp, ipAddress, INET_ADDRSTRLEN);  // Store IP
                return 1;  //IP address is found
            }
        }
    }

    freeifaddrs(ifap);

    return 0;
}

int main(int argc, char *argv[]) {
    //we pass as arguments the ip and port of the server
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <proxy_port>\n", argv[0]);
        return 1;
    }

    if(setServerConn()==1)
        printf("Server is connected.\n");
    else
        printf("Failure to connect to sever.\n");

    int proxy_port = atoi(argv[1]);

    int proxy_socket, client_socket;
    struct sockaddr_in proxy_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    proxy_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (proxy_socket == -1) {
        perror("Proxy socket creation failed");
        exit(1);
    }

    proxy_addr.sin_family = AF_INET;
    proxy_addr.sin_addr.s_addr = INADDR_ANY;
    proxy_addr.sin_port = htons(proxy_port);

    if (bind(proxy_socket, (struct sockaddr *)&proxy_addr, sizeof(proxy_addr) < 0))
    {
        perror("Bind failed");
        exit(1);
    }

    if (listen(proxy_socket, 5) < 0) {
        perror("Listen failed");
        exit(1);
    }

    while (1) {
        client_socket = accept(proxy_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }

        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, handle_client, (void *)&client_socket) < 0) {
            perror("Thread creation failed");
            close(client_socket);
        }
    }

    close(proxy_socket);
    return 0;
}

void *handle_client(void *arg) {
    int client_socket = *((int *)arg);
    char buffer[BUFFER_SIZE];
    int bytes_read;

    const char *local_ip = "127.0.0.1";
    const int local_port = 8080;

    int local_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (local_socket < 0) {
        perror("Local socket creation failed");
        close(client_socket);
        pthread_exit(NULL);
    }

    struct sockaddr_in local_addr;
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = inet_addr(local_ip);
    local_addr.sin_port = htons(local_port);

    if (connect(local_socket, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        perror("Connection to local server failed");
        close(client_socket);
        close(local_socket);
        pthread_exit(NULL);
    }

    while ((bytes_read = read(client_socket, buffer, BUFFER_SIZE)) > 0) {
        send(local_socket, buffer, bytes_read, 0);
        bytes_read = read(local_socket, buffer, BUFFER_SIZE);
        send(client_socket, buffer, bytes_read, 0);
    }

    close(client_socket);
    close(local_socket);
    pthread_exit(NULL);
}