#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>


pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int max_client = 0;

void *handle_client(void *socket_desc) {
    int client_sock = *((int*)socket_desc);
    char client_message[2000], server_message[2000];

    // Clean buffers:
    memset(client_message, '\0', sizeof(client_message));
    memset(server_message, '\0', sizeof(server_message));

    // Receive client's message:
    while (1) {
        if (recv(client_sock, client_message, sizeof(client_message), 0) < 0) {
            printf("Couldn't receive\n");
            break; // Exit the loop on receive error
        }
        printf("Msg from client: %s\n", client_message);
        pthread_mutex_lock(&mutex);
        // Respond to client:
        strcpy(server_message, "Packet has reached its destination\n");

        if (send(client_sock, server_message, strlen(server_message), 0) < 0) {
            printf("Can't send\n");
            break; // Exit the loop on send error
        }
        pthread_mutex_unlock(&mutex);

        sleep(2);
        memset(client_message, '\0', sizeof(client_message));
    }

    // Closing the socket:
    close(client_sock);

    pthread_exit(NULL);
}

int main(int argc, char* argv[]) {
    int socket_desc, client_sock, client_size;
    struct sockaddr_in server_addr, client_addr;
    char server_message[2000], client_message[2000];
    
    // Clean buffers:
    memset(server_message, '\0', sizeof(server_message));
    memset(client_message, '\0', sizeof(client_message));

    // Create socket:
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);

    if(socket_desc < 0) {
        printf("Error while creating socket\n");
        return -1;
    }
    printf("Socket created successfully\n");

    // Set port and IP that we'll be listening for, any other IP_SRC or port will be dropped:
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[1]));
    server_addr.sin_addr.s_addr = inet_addr("192.168.1.137");

    // Bind to the set port and IP:
    if(bind(socket_desc, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("Couldn't bind to the port\n");
        return -1;
    }
    printf("Done with binding\n");

    // Listen for clients:
    if(listen(socket_desc, 5) < 0) { // Increased the backlog to 5
        printf("Error while listening\n");
        return -1;
    }
    printf("\nListening for incoming connections.....\n");

    // Accept incoming connections and handle them in separate threads:
    while(1) {
        client_size = sizeof(client_addr);
        client_sock = accept(socket_desc, (struct sockaddr*)&client_addr, &client_size);

        if (client_sock < 0) {
            printf("Can't accept\n");
            continue; // Continue to the next iteration
        }

        printf("Client connected at IP: %s and port: %i\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, (void*)&client_sock) < 0) {
            printf("Couldn't create thread\n");
            return -1;
        }

        pthread_detach(thread_id); // Detach the thread to clean up resources automatically
    }

    // Closing the socket:
    close(socket_desc);

    return 0;
}
