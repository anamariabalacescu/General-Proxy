#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

bool connectServer(const char* ip, int port)
{

    //executes the server program so it receives the port
    char command[100];
    snprintf(command, sizeof(command), "/home/student/Desktop/Folder/accept-conn %d &", port);
    int status = system(command);

    if (status == -1) {
        perror("system");
        return 1;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        return 0;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        return 0;
    }

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr) ) == -1) {
        perror("connect");
        return 0;
    }

    char *message = (char*)malloc(sizeof(port));
    sprintf(message, "%d", port);

    if (send(sockfd, message, strlen(message), 0) == -1) {
        perror("send");
        exit(1);
    }

    printf("Connection to %s:%d successful!\n", ip, port);

    printf("Message sent: %s\n", message);

    char buffer[1024];
    ssize_t bytes_received = recv(sockfd, buffer, sizeof(buffer), 0);
    if (bytes_received == -1) {
        perror("recv");
        exit(1);
    }

    if (bytes_received == 0) {
        printf("Server closed the connection\n");
    } else {
        buffer[bytes_received] = '\0'; 
        printf("Received: %s\n", buffer);
    }

    close(sockfd);

    return 1;

}

int main(int argc, char *argv[]) {
    /*if (argc != 3) {
        fprintf(stderr, "Usage: %s <IP> <PORT>\n", argv[0]);
        return 1;
    }*/

    const char *ip = "192.168.84.112";//argv[1];
    int port = 444537;//atoi(argv[2]);

    if(connectServer(ip, port)==1)
        printf("We did it.");
    return 0;
}