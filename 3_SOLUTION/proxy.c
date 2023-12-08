#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>

#define MAX_CLIENTS 1000

#define linechar 128
#define delim -

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
void getRules(const char* filepath) {
    blockedIp = (char**)malloc(sizeof(char*));
    blockedMac = (char**)malloc(sizeof(char*));
    ips.initialValue = (char**)malloc(sizeof(char*));
    ips.replacedValue = (char**)malloc(sizeof(char*));
    rBytes.initialValue = (char**)malloc(sizeof(char*));
    rBytes.replacedValue = (char**)malloc(sizeof(char*));

    int f = open(filepath, O_RDONLY);
    int rc;
    if(f < 0)
    {
        perror("File descriptor not received");
        exit(-1);
    }    

    char* buf = (char*)malloc(sizeof(char)*linechar);
    ssize_t bytes_read;

    int caz = -1;
    //cases: 1 - ip blocked, 2 - mac blocked, 3 - replace ip, 4 - replace bytes

    char c;
    ssize_t countch = read (f, &c, 1);

    if(countch < 1)
    {
        perror("File is empty");
        exit(-1);
    }

    while(countch)
    {
        int nrch = 0;
        int ok = 0;
        while(c!= '\n')
        {
            ok = 1;
            buf[nrch++] = c;
            read(f, &c, 1);
        }

        buf[nrch] = '\0';
        
        if(ok == 1)
        {
            if(strstr(buf, "ipr"))
                caz = 3;
            else {
                if(strstr(buf, "mac"))
                    caz = 2;
                else {
                    if(strstr(buf, "ip"))
                        caz = 1;
                    else {
                        if(strstr(buf, "bytesr"))
                            caz = 4;
                        else{
                            if(strlen(buf)>0)
                            //nu suntem pe linie pentru determinare a cazului deci trebuie sa preluam valorile
                            switch(caz)
                            {
                                case 1:
                                    blockedIp = (char**)realloc(blockedIp, (blckip + 1)*sizeof(char*));
                                    blockedIp[blckip] = (char*)malloc(sizeof(char)*(strlen(buf)+1)); //+1 pentru '\0' de la sfarsitul lui buf
                                    strcpy(blockedIp[blckip], buf);
                                    blckip++;
                                    break;
                                case 2:
                                    blockedMac = (char**)realloc(blockedMac, (blckmac + 1)*sizeof(char*));
                                    blockedMac[blckmac] = (char*)malloc(sizeof(char)*(strlen(buf)+1));
                                    strcpy(blockedMac[blckmac], buf);
                                    blckmac++;
                                    break;
                                case 3:
                                    ips.initialValue = (char**)realloc(ips.initialValue, sizeof(char) * (rplip + 1));
                                    ips.replacedValue = (char**)realloc(ips.replacedValue, sizeof(char) * (rplip + 1));
                                    ips.initialValue[rplip] = (char*)malloc(sizeof(char)*(strlen(buf)+1));
                                    ips.replacedValue[rplip] = (char*)malloc(sizeof(char)*(strlen(buf)+1));
                                    char* ip1 = strtok(buf, "-");
                                    char* ip2 = strtok(NULL, "\n");
                                    strcpy(ips.initialValue[rplip], ip1);
                                    strcpy(ips.replacedValue[rplip], ip2);
                                    rplip++;
                                    break;
                                case 4:
                                    rBytes.initialValue = (char**)realloc(rBytes.initialValue, sizeof(char) * (rplbytes + 1));
                                    rBytes.replacedValue = (char**)realloc(rBytes.replacedValue, sizeof(char) * (rplbytes + 1));
                                    rBytes.initialValue[rplbytes] = (char*) malloc(sizeof(char) * (strlen(buf) + 1));
                                    rBytes.replacedValue[rplbytes] = (char*) malloc(sizeof(char) * (strlen(buf) + 1));
                                    char* mac1 = strtok(buf, "-");
                                    char* mac2 = strtok(NULL, "\n");
                                    strcpy(rBytes.initialValue[rplbytes], mac1);
                                    strcpy(rBytes.replacedValue[rplbytes], mac2);
                                    rplbytes++;
                                    break;
                                default:
                                    perror("Invalid case");
                                    exit(-1);
                            }
                        }
                    }
                }
            }
        }
        countch = read(f, &c, 1);
        memset(buf, '\0', sizeof(buf));
    }

    //pentru verificare afisam rezultatele:
    printf("Blocked ip:\n");
    for(int i = 0; i < blckip; i++)
        printf("%s\n", blockedIp[i]);
    
    printf("Blocked mac:\n");
    for(int i = 0; i < blckmac; i++)
        printf("%s\n", blockedMac[i]);

    printf("Replace ip:\n");
    for(int i = 0; i < rplip; i++)
        printf("%s %s\n", ips.initialValue[i], ips.replacedValue[i]);

    printf("Replace bytes:\n");
    for(int i = 0; i < rplbytes; i++)
        printf("%s cu %s\n", rBytes.initialValue[i], rBytes.replacedValue[i]);

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


char* charToHex(const char* input) {
    size_t len = strlen(input);
    char* hex = (char*)malloc(2 * len + 1);

    for (size_t i = 0; i < len; ++i) {
        sprintf(hex + 2 * i, "%02X", input[i]);
    }

    hex[2 * len] = '\0';
    return hex;
}

char* replaceBytes(const char* message) {
    char* hexMessage = charToHex(message);

    size_t hexLen = strlen(hexMessage);

    for (size_t i = 0; i < hexLen; i += 2) {
        for (int j = 0; j < rplbytes; ++j) {
            if (strncmp(hexMessage + i, rBytes.initialValue[j], 2) == 0) {
                strncpy(hexMessage + i, rBytes.replacedValue[j], 2);
                break;
            }
        }
    }

    return hexMessage;
}

char* replaceCustomBytes(const char* message, const char* bytes2Replace, const char* replacement) {
    if (message == NULL || bytes2Replace == NULL || replacement == NULL) {
        perror("Invalid arguments");
        exit(-1);
    }

    size_t messageLength = strlen(message);
    size_t bytes2ReplaceLength = strlen(bytes2Replace);
    size_t replacementLength = strlen(replacement);
    
    //printNow("la replace\n");

    char* result = (char*)malloc((messageLength + 1) * sizeof(char));
    if (result == NULL) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }

    size_t i, j, k;

    for (i = 0; i < messageLength; ++i) {
        if (strncmp(&message[i], bytes2Replace, bytes2ReplaceLength) == 0) {
            //printNow("am intrat in comparatie\n");
            for (j = 0; j < replacementLength; ++j) {
                result[i + j] = replacement[j];
            }
            i += bytes2ReplaceLength - 1;
        } else {
            result[i] = message[i];
        }
    }
    result[i] = '\0';

    //printNow("am iesit din for\n");

    history("Proxy","has replaced bytes as dictated in CLI");

    return result;
}

char* hexToAscii(const char* hex) {
    size_t hex_len = strlen(hex);
    if (hex_len % 2 != 0) {
        // Invalid hex string
        return NULL;
    }

    //printNow("la hex to ascii\n");

    size_t ascii_len = hex_len / 2;
    char* ascii = (char*)malloc(ascii_len + 1);

    for (size_t i = 0; i < ascii_len; ++i) {
        //printf("Se calculeaza traducerea in ascii\n");
        sscanf(hex + 2 * i, "%2hhX", &ascii[i]);
    }

    //printNow("gata");
    ascii[ascii_len] = '\0';
    return ascii;
}

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
            printf("\nSelect (F) Forward, (D) Drop, (R) Replace Bytes, (C) Custom replace bytes\n");
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
                    //history("Proxy", "replaces bytes");
                    ok =1;
                    //TO DO
                    pthread_mutex_unlock(&mutex);
                } else if (choice == 'C') {
                    ok =1;
                    size_t len = strlen(buffer);
                    char* hex = (char*)malloc(2 * len + 1);
                    hex = charToHex(buffer);
                    
                    printf("Enter the bytes to be replaced: ");
                    char c;
                    char* bytes2Replace = (char*)malloc(sizeof(char));
                    int chn = 0;
                    while((c = getchar()) != '\n') {
                        bytes2Replace[chn++] = c;
                        bytes2Replace = (char*)realloc(bytes2Replace, (chn + 1) * sizeof(char));
                    }
                    bytes2Replace[chn] = '\0';

                    printf("Enter the bytes for replacement: ");
                    char* replacement = (char*)malloc(sizeof(char));
                    chn = 0;
                    while((c = getchar()) != '\n') {
                        replacement[chn++] = c;
                        replacement = (char*)realloc(replacement, (chn + 1) * sizeof(char));
                    }
                    replacement[chn] = '\0';
                    
                    //printNow(hex);

                    char* replacedHex = (char*)malloc(2 * len + 1 + strlen(replacement) - strlen(bytes2Replace) + 1);
                    replacedHex= replaceCustomBytes(hex, bytes2Replace, replacement);
                    //printNow(replacedHex);

                    char* ascii = (char*)malloc(len + 1);
                    ascii = hexToAscii(replacedHex);
                    //printNow(ascii);

                    printf("Replaced message: %s\n");
                    hex_dump(ascii);

                    // Forward message to server
                    send(server_socket, ascii, strlen(ascii), 0);
                    history("Proxy", "forwarded the packet to the server");

                    int valread = recv(server_socket, buffer, sizeof(buffer), 0);
                    buffer[valread] = '\0';

                    printf("\nServer message to client(%d):\n", client->clientNumber);
                    hex_dump(buffer);
                    history("Server", "sent a response packet to the proxy");
                    // Send server message to client
                    send(client->socket, buffer, strlen(buffer), 0);
                    history("Proxy", "forwarded the packet to the client");

                    pthread_mutex_unlock(&mutex);
                }else{
                    history("Proxy", "attempted harmful action.");
                    printf("Wrong choice. Try again.\n");
                    printf("Select (F) Forward, (D) Drop, (R) Replace Bytes, (C) Custom replace bytes\n");
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
    // if (argc < 4) {
    //     perror("Missing parameter");
    //     exit(-1);
    // }

    // Initialize arrays
    blockedIp = (char**)malloc(sizeof(char*));
    blockedMac = (char**)malloc(sizeof(char*));
    ips.initialValue = (char**)malloc(sizeof(char*));
    ips.replacedValue = (char**)malloc(sizeof(char*));
    rBytes.initialValue = (char**)malloc(sizeof(char*));
    rBytes.replacedValue = (char**)malloc(sizeof(char*));

    getRules(argv[4]);

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
    // debug(); // verificam

    printf("Proxy listening on htons(port %s), forwarding to %s:%s\n", client_port, server_ip, server_port);

    while (1) {
        int client_socket;
        struct sockaddr_in client_addr;
        int client_size = sizeof(client_addr);
        // printf("aici 1\n");
        // Accept an incoming connection from a client
        client_socket = accept(proxy_socket, (struct sockaddr*)&client_addr, &client_size);
        
        if (client_socket < 0) {
            printf("Can't accept\n");
            exit(-1);
        }

        if(searchBlocked(inet_ntoa(client_addr.sin_addr), 1) == 0) {
        // Create a new thread to handle the client
            // printf("aici 3\n");
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