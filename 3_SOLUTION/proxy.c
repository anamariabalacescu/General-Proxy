#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>


#define MAX_CLIENTS 3

int proxy_socket;
int server_socket;
struct sockaddr_in proxy_addr;
struct sockaddr_in server_addr;

char* server_ip;
char* server_port;
char* client_port;

typedef struct {
    int socket;
    struct sockaddr_in address;
    int clientNumber;
} Client;

typedef struct {
    Client client;
    char* message;
}Message;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


int client_count = 0;
int max_client = 0;

Client* aux;
char buffaux[2000];
int siqnalRequired=0;

void *handle_client(void *arg) {
    Client *client = (Client *)arg;
    char buffer[2000];
    int valread;
    
    while (1) {
        valread = read(client->socket, buffer, sizeof(buffer));
        if (valread <= 0) {
            // Client disconnected
            close(client->socket);
            printf("\nClient disconnected\n");
            client_count--;
            max_client = 0;
            break;
        }

        buffer[valread] = '\0';
        printf("\nClient(%d) message:\n",client->clientNumber);
        hex_dump(buffer);
        aux = client;
        pthread_mutex_lock(&mutex);
        printf("\nSelect (F) Forward, (D) Drop, (R) Replace Bytes\n");
        printf("Enter your choice for client(%d): ",client->clientNumber);
        char choice;
        scanf("%c",&choice);
        getchar();
        
        if (choice == 'F') {
            // Forward message to server
            send(server_socket, buffer, strlen(buffer), 0);

            // Receive the server's response
            valread = recv(server_socket, buffer, sizeof(buffer), 0);

            if (valread <= 0) {
                // Server disconnected
                close(client->socket);
                printf("Server disconnected\n");
                
                pthread_mutex_unlock(&mutex);
                break;
            } else {
                buffer[valread] = '\0';
                printf("\nServer message to client(%d):\n", client->clientNumber);
                hex_dump(buffer);

                // Send server message to client
                send(client->socket, buffer, strlen(buffer), 0);
                pthread_mutex_unlock(&mutex);
            }
        } else if (choice == 'D') {
            // Nothing to be done, server won't get the message
            printf("Packet dropped\n");
            memcpy(buffer, "Packet dropped \n", strlen("Packet dropped \n"));
            send(client->socket, buffer, strlen(buffer), 0);
            pthread_mutex_unlock(&mutex);
        } else if (choice == 'R') {
            //TO DO
            pthread_mutex_unlock(&mutex);
        }
        // Forward the server's response to the client
        //send(client->socket, buffer, strlen(buffer), 0);
    }

    free(client);
    pthread_exit(NULL);
}

void hex_dump(const char* message) {
    ssize_t length = strlen(message);
    size_t i, j;

    for (i = 0; i < length; i += 16) {
        // address offset
        printf("%08lX: ", i);

        // hexdump
        for (j = 0; j < 16 && i + j < length; j++) {
            printf("%02X", (unsigned char)message[i + j]);
            if (j % 2 == 1) {
                printf(" ");
            }
        }

        // padding
        for (; j < 16; j++) {
            printf("   ");
            if (j % 2 == 1) {
                printf(" ");
            }
        }

        // ASCII
        for (j = 0; j < 16 && i + j < length; j++) {
            char ch = message[i + j];
            if (ch >= 32 && ch <= 126) {
                printf("%c", ch);
            } else {
                printf(".");
            }
        }

        printf("\n");
    }
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        perror("Missing parameter");
        exit(-1);
    }

    server_ip = argv[1];
    server_port = argv[2];
    client_port = argv[3];

    // Set up proxy address structure
    proxy_addr.sin_family = AF_INET;
    proxy_addr.sin_port = htons(atoi(client_port));
    proxy_addr.sin_addr.s_addr = INADDR_ANY;

    // Set up server address structure
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(server_port));
    server_addr.sin_addr.s_addr = inet_addr(server_ip);

    // Create proxy socket
    proxy_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (proxy_socket < 0) {
        printf("Unable to create proxy socket\n");
        exit(-1);
    }

    // Bind proxy socket to the specified port
    if (bind(proxy_socket, (struct sockaddr*)&proxy_addr, sizeof(proxy_addr)) < 0) {
        printf("Couldn't bind to the port\n");
        exit(-1);
    }

    // Listen for incoming connections
    if (listen(proxy_socket, 1000) < 0) {
        printf("Error while listening\n");
        exit(-1);
    }

    // Server socket for communication
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        printf("Unable to create server socket\n");
        exit(-1);
    }

    // Connect to the server
    if (connect(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("Unable to connect to the server\n");
        exit(-1);
    }

    printf("Proxy listening on htons(port %s), forwarding to %s:%s\n", client_port, server_ip, server_port);

    
    pthread_t threads[MAX_CLIENTS];

    while (1) {
        if(max_client == 0) {
            int client_socket;
            struct sockaddr_in client_addr;
            int client_size = sizeof(client_addr);

            // Accept an incoming connection from a client
            client_socket = accept(proxy_socket, (struct sockaddr*)&client_addr, &client_size);

            if (client_socket < 0) {
                printf("Can't accept\n");
                exit(-1);
            }


            // Create a new thread to handle the client
            Client *client = (Client *)malloc(sizeof(Client));
            client->socket = client_socket;
            client->address = client_addr;
            client->clientNumber = client_count;

            printf("Client(%d) connected at IP: %s and port: %i\n",client->clientNumber ,inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

            if (pthread_create(&threads[client_count], NULL, handle_client, (void *)client) != 0) {
                perror("Thread creation failed");
                exit(EXIT_FAILURE);
            }
    }
        client_count++;

        // Limit the number of clients for this example
        if (client_count == MAX_CLIENTS) {
            printf("Maximum number of clients reached. No more connections will be accepted.\n");
            max_client = 1;
        }
    }

    // Wait for all threads to finish
    for (int i = 0; i < client_count; i++) {
        pthread_join(threads[i], NULL);
    }

    // Close sockets
    close(proxy_socket);
    close(server_socket);

    return 0;
}