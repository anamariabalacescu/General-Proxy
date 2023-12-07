#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>


#define MAX_CLIENTS 1000

//black list and replacing rules
char** blockedIp;
int blckip = 0;

char** blockedMac;
int blckmac = 0;

struct replaceIp{
    char** initialValue;
    char** replacedValue;
}ips; 
int rplip =0;

struct replaceBytes{
    char** initialValue;
    char** replacedValue;
}rBytes; 
int rplbytes=0;
//

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

#define WAITING_LIST_SIZE 100

typedef struct {
    Client** waiting_clients;
    int front;
    int rear;
} WaitingList;

WaitingList* waiting_list;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_t threads[MAX_CLIENTS];


int client_count = 0;
int max_client = 0;

Client* aux;
Client* queue;
char buffaux[2000];
int siqnalRequired=0;

void initialize_waiting_list() {
    waiting_list = (WaitingList*)malloc(sizeof(WaitingList));
    waiting_list->waiting_clients = (Client**)malloc(WAITING_LIST_SIZE * sizeof(Client*));
    waiting_list->front = -1;
    waiting_list->rear = -1;
}

int is_waiting_list_empty() {
    //debug("is_waiting_list_empty");
    return (waiting_list->front == -1 && waiting_list->rear == -1);
}

void enqueue_waiting_client(Client* client) {
    if (is_waiting_list_empty()) {
        waiting_list->front = 0;
    }

    waiting_list->rear = (waiting_list->rear + 1) % WAITING_LIST_SIZE;
    waiting_list->waiting_clients[waiting_list->rear] = client;
}

Client* dequeue_waiting_client() {
    if (is_waiting_list_empty()) {
        return NULL;
    }

    Client* client = waiting_list->waiting_clients[waiting_list->front];

    if (waiting_list->front == waiting_list->rear) {
        // Last element in the queue
        initialize_waiting_list();
    } else {
        waiting_list->front = (waiting_list->front + 1) % WAITING_LIST_SIZE;
    }

    return client;
}

void printNow(char *Message) {
    printf("GOT: %s\n", Message);
}
//set-up for server

void getRules(int server_sock) {
    // Receiving blocked ip addresses
    int nr = 0;
    int len;
    char* server_message;
    recv(server_sock, &nr, sizeof(int), 0);
    printf("%d \n", nr);
    //printNow(number);
    if (nr > 0) {
        for (int i = 0; i < nr; i++) {
            recv(server_sock, &len, sizeof(int), 0);
            printf("GOT: %d\t", len);
            server_message = (char*) malloc((len+1) * sizeof(char));
            recv(server_sock, server_message, len, 0);
            printNow(server_message);
            blockedIp = (char **)realloc(blockedIp, (blckip + 1) * sizeof(char *));
            blockedIp[blckip] = strdup(server_message);
            blckip++;

            len = 0;
            memset(server_message, '\0', sizeof(server_message));
        }
    }

    //printf("aici 1\n");
    // Receiving blocked mac addresses
    nr = 0;
    //memset(number, '\0', sizeof(number));
    recv(server_sock, &nr, sizeof(nr), 0);

    if (nr > 0) {
        for (int i = 0; i < nr; i++) {
            recv(server_sock, &len, sizeof(len), 0);
            printf("GOT: %d\t", len);
            server_message = (char*) malloc((len + 1) * sizeof(char));
            recv(server_sock, server_message, len, 0);
            printNow(server_message);
            blockedMac = (char **)realloc(blockedMac, (blckmac + 1) * sizeof(char *));
            blockedMac[blckmac] = strdup(server_message);
            blckmac++;

            len = 0;
            memset(server_message, '\0', sizeof(server_message));
        }
    }

    //printf("aici 2\n");

    // Receiving bytes to replace
    recv(server_sock, &nr, sizeof(int), 0);
    if (nr > 0) {
        for (int i = 0; i < nr; i++) {

            recv(server_sock, &len, sizeof(len), 0);
            
            char *server_message = (char *)malloc((len + 1) * sizeof(char));
            recv(server_sock, server_message, len, 0);
                printNow(server_message);
            server_message[len] = '\0';
            rBytes.initialValue = (char **)realloc(rBytes.initialValue, (rplbytes + 1) * sizeof(char *));
            rBytes.replacedValue = (char **)realloc(rBytes.replacedValue, (rplbytes + 1) * sizeof(char *));
            //char *aux = strtok(server_message, "-");
            
            rBytes.initialValue[rplbytes] = strdup(strtok(server_message, "-"));
            //printf("SECV1 : %s\n", rBytes.initialValue[rplbytes]);

            // allocate memory + copy value
            rBytes.replacedValue[rplbytes] = strdup(strtok(NULL, "\0"));
            //printf("SECV2 : %s\n", rBytes.replacedValue[rplbytes]);
            //printf("SECV1 : %s\n", rBytes.initialValue[rplbytes]);
            
            rplbytes++;
            
            len = 0;
            memset(server_message, '\0', sizeof(server_message));
        }
    }

    //printf("aici 3\n");
}

//end set-up

//verify set up
void debug()
{
    printf("Blocked ip %d:\n", blckip);
    for(int i = 0; i < blckip; i++)
        printf("%s\n", blockedIp[i]);
    
    printf("Blocked mac %d:\n", blckmac);
    for(int i = 0; i < blckmac; i++)
        printf("%s\n", blockedMac[i]);

    printf("Replace ip %d:\n", rplip);
    for(int i = 0; i < rplip; i++)
        printf("%s %s\n", ips.initialValue[i], ips.replacedValue[i]);

    printf("Replace bytes %d:\n", rplbytes);
    for(int i = 0; i < rplbytes; i++) {
        printf("%s ", rBytes.initialValue[i]);
        printf("%s\n", rBytes.replacedValue[i]);
    }
}
//end verify set up

void *handle_client(void *arg) {
    Client *client = (Client *)arg;
    char buffer[2000];
    int valread;
    
    while (1) {
        valread = read(client->socket, buffer, sizeof(buffer));
        if (valread <= 0) {
            // Client disconnected
            printf("\nClient disconnected\n");
            client_count--;

            history(inet_ntoa(client->address.sin_addr), "disconnected from the proxy");

            // Try to dequeue a waiting client and handle it
            Client* waiting_client = dequeue_waiting_client();
            
            if (waiting_client != NULL) {
                printf("Handling waiting client(%d)\n", waiting_client->clientNumber);

                client->socket = waiting_client->socket;
                client->address = waiting_client->address;
                client->clientNumber = waiting_client->clientNumber;

                free(waiting_client);
                client_count++;         
            } else{
                printf("No waiting clients\n");
            }

            break;
        } else{
            pthread_mutex_lock(&mutex);
            buffer[valread] = '\0';
            printf("\nClient(%d) message:\n",client->clientNumber);
            history(inet_ntoa(client->address.sin_addr), "sent a packet to the proxy");
            hex_dump(buffer);
            aux = client;
            printf("\nSelect (F) Forward, (D) Drop, (R) Replace Bytes\n");
            printf("Enter your choice for client(%d): ",client->clientNumber);
            char choice;
            int ok = 0;
            do{
                scanf("%c",&choice);
                getchar();
                
                if (choice == 'F') {
                    ok = 1;
                    // Forward message to server
                    send(server_socket, buffer, strlen(buffer), 0);

                    history("Proxy", "forwarded the packet to the server");
                    // Receive the server's response
                    valread = recv(server_socket, buffer, sizeof(buffer), 0);

                    if (valread <= 0) {
                        // Server disconnected

                        history(inet_ntoa(client->address.sin_addr), "disconnected from the proxy");
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
                    history("Proxy", "dropped the packet.");
                    ok = 1;
                    // Nothing to be done, server won't get the message
                    printf("Packet dropped\n");
                    memcpy(buffer, "Packet dropped \n", strlen("Packet dropped \n"));
                    send(client->socket, buffer, strlen(buffer), 0);
                    pthread_mutex_unlock(&mutex);
                } else if (choice == 'R') {
                    history("Proxy", "replaces bytes");
                    ok =1;
                    //TO DO
                    pthread_mutex_unlock(&mutex);
                } else{
                    history("Proxy", "attempted harmful action.");
                    printf("Wrong choice. Try again.\n");
                    printf("Select (F) Forward, (D) Drop, (R) Replace Bytes\n");
                }
            }while(ok == 0);
            // Forward the server's response to the client
            //send(client->socket, buffer, strlen(buffer), 0);
        }
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

int searchBlocked(char *add, int type){
    if(type == 1){
        //printf("aici\n");
        //search ip
        for(int i = 0; i < blckip; i++)
            if(strstr(blockedIp[i], add) != NULL){
                //printf("aici\n");
                return 1;
            }
        return 0;
    }
}

void history(const char* maker, const char* action) {
    FILE *file = fopen("historylog.txt", "a");
    if (file == NULL) {
        perror("Error opening historylog.txt");
        exit(EXIT_FAILURE);
    }

    // Get current timestamp
    time_t t = time(NULL);
    struct tm* tm_info = localtime(&t);

    // Format timestamp as string
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%d/%m/%Y %H:%M", tm_info);

    // Write log entry to file
    fprintf(file, "%s %s.\t\t%s\n", maker, action, timestamp);

    fclose(file);
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        perror("Missing parameter");
        exit(-1);
    }
    // Initialize arrays
    blockedIp = (char**)malloc(sizeof(char*));
    blockedMac = (char**)malloc(sizeof(char*));
    ips.initialValue = (char**)malloc(sizeof(char*));
    ips.replacedValue = (char**)malloc(sizeof(char*));
    rBytes.initialValue = (char**)malloc(sizeof(char*));
    rBytes.replacedValue = (char**)malloc(sizeof(char*));

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

    history("Proxy", "connected to the server");
    getRules(server_socket); //get rules and blacklist from server
    history("Proxy", "received rules from the server");
    debug(); // verificam

    printf("Proxy listening on htons(port %s), forwarding to %s:%s\n", client_port, server_ip, server_port);

    while (1) {
        int client_socket;
        struct sockaddr_in client_addr;
        int client_size = sizeof(client_addr);

        // Accept an incoming connection from a client
        client_socket = accept(proxy_socket, (struct sockaddr*)&client_addr, &client_size);
        
        if (client_socket < 0) {
            printf("Can't accept\n");
            exit(-1);
        }

        if(searchBlocked(inet_ntoa(client_addr.sin_addr), 1) == 0) {
        // Create a new thread to handle the client
            //printf("aici 3\n");
            history(inet_ntoa(client_addr.sin_addr), "connected to the proxy");
            //history("Client", "connected to the proxy");
            Client *client = (Client *)malloc(sizeof(Client));
            client->socket = client_socket;
            client->address = client_addr;
            client->clientNumber = client_count;
            client_count++;

            if(client_count <= MAX_CLIENTS) {
                printf("Client(%d) connected at IP: %s and port: %i\n",client->clientNumber ,inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

                if (pthread_create(&threads[client_count - 1], NULL, handle_client, (void *)client) != 0) {
                    perror("Thread creation failed");
                    exit(EXIT_FAILURE);
                }
                        // Limit the number of clients for this example
                if (client_count == MAX_CLIENTS) {
                    printf("Maximum number of clients reached. No more connections will be accepted.\n");
                    max_client = 1;
                }
            } else {
                // Enqueue in waiting list
                printf("Client(%d) enqueued in waiting list\n", client->clientNumber);
                enqueue_waiting_client(client);
            }
        } else{ 
            //printf("aici 2\n");
            history(inet_ntoa(client_addr.sin_addr), "blocked by the proxy");
            printf("Blocked ip: %s\n", inet_ntoa(client_addr.sin_addr));
            close(client_socket);
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