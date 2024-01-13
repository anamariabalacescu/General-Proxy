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
#include <regex.h>

#define MAX_CLIENTS 1000

#define linechar 128
#define delim -

//black list and replacing rules
char** blockedIp;
int blckip = 0;

char** blockedMac;
int blckmac = 0;


char** protocols;
int prot_no =0;

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

//detectie protocoale
typedef struct{
    char* name;
    char* regex;
    char* hex;
}protocol;

protocol* prots;

void initialize_protocols() {
    prots = (protocol*)malloc(100 * sizeof(protocol));

    prots[0].name = (char*)malloc(5 * sizeof(char));
    memcpy(prots[0].name, "HTTP", 5);
    prots[0].regex = (char*)malloc(22 * sizeof(char));
    memcpy(prots[0].regex, "^GET http://www.+ HTTP.+", 22);
    prots[0].hex = (char*)malloc(strlen("485454502F") + 1);
    memcpy(prots[0].hex, "485454502F", strlen("485454502F")+1);
    prots[0].hex[strlen("485454502F")] = '\0';

    prots[1].name = (char*)malloc(5 * sizeof(char));
    memcpy(prots[1].name, "HTTP", 5);
    prots[1].regex = (char*)malloc(27 * sizeof(char));
    memcpy(prots[1].regex, "^POST /submit-form HTTP/", 27);
    prots[1].hex = (char*)malloc(strlen("504F5354202F7375626D69742D666F726D20485454502F") + 1);
    memcpy(prots[1].hex, "504F5354202F7375626D69742D666F726D20485454502F", strlen("504F5354202F7375626D69742D666F726D20485454502F")+1);
    prots[1].hex[strlen("504F5354202F7375626D69742D666F726D20485454502F")] = '\0';

    prots[2].name = (char*)malloc(4 * sizeof(char));
    memcpy(prots[2].name, "SSH", 4);
    prots[2].regex = (char*) malloc(29 * sizeof(char));
    memcpy(prots[2].regex, "^SSH-[0-9]+.[0-9]+-OpenSSH.*", 29);
    prots[2].hex = (char*)malloc(strlen("2D4F70656E535348") + 1);
    memcpy(prots[2].hex, "2D4F70656E535348", strlen("2D4F70656E535348")+1);

    prots[3].name = (char*)malloc(4 * sizeof(char));
    memcpy(prots[3].name, "FTP", 4);
    prots[3].regex = (char*) malloc(9 * sizeof(char));
    memcpy(prots[3].regex, "^USER .*", 9); 
    prots[3].hex = (char*)malloc(strlen("55534552") + 1);
    memcpy(prots[3].hex, "55534552", strlen("55534552")+1);

    prots[4].name = (char*)malloc(4 * sizeof(char));
    memcpy(prots[4].name, "FTP", 4);
    prots[4].regex = (char*) malloc(9 * sizeof(char));
    memcpy(prots[4].regex, "^LIST .*", 9);
    prots[4].hex = (char*)malloc(strlen("4C495354") + 1);
    memcpy(prots[4].hex, "4C495354", strlen("4C495354")+1);

    prots[5].name = (char*)malloc(4 * sizeof(char));
    memcpy(prots[5].name, "FTP", 4);
    prots[5].regex = (char*) malloc(14 * sizeof(char));
    memcpy(prots[5].regex, "^PORT [0-9,]+", 14);
    prots[5].hex = (char*)malloc(strlen("504F5254") + 1);
    memcpy(prots[5].hex, "504F5254", strlen("504F5254")+1);

    prots[6].name = (char*)malloc(4 * sizeof(char));
    memcpy(prots[6].name, "FTP", 4);
    prots[6].regex = (char*) malloc(9 * sizeof(char));
    memcpy(prots[6].regex, "^RETR .*", 9);
    prots[6].hex = (char*)malloc(strlen("52455452") + 1);
    memcpy(prots[6].hex, "52455452", strlen("52455452")+1);

    prots[7].name = (char*)malloc(4 * sizeof(char));
    memcpy(prots[7].name, "FTP", 4);
    prots[7].regex = (char*) malloc(9 * sizeof(char));
    memcpy(prots[7].regex, "^STOR .*", 9);
    prots[7].hex = (char*)malloc(strlen("53544F52") + 1);
    memcpy(prots[7].hex, "53544F52", strlen("53544F52")+1);
    
    prots[8].name = (char*)malloc(4 * sizeof(char));
    memcpy(prots[8].name, "FTP", 4);
    prots[8].regex = (char*) malloc(8 * sizeof(char));
    memcpy(prots[8].regex, "^CWD .*", 8);
    prots[8].hex = (char*)malloc(strlen("435744") + 1);
    memcpy(prots[8].hex, "435744", strlen("435744")+1);

    prots[9].name = (char*)malloc(4 * sizeof(char));
    memcpy(prots[9].name, "SSH", 4);
    prots[9].regex = (char*) malloc(29 * sizeof(char));
    memcpy(prots[9].regex, ".*ssh-userauth.*", 29);
    prots[9].hex = (char*)malloc(strlen("7373682d7573657261757468") + 1);
    memcpy(prots[9].hex, "7373682d7573657261757468", strlen("7373682d7573657261757468")+1);

    prots[10].name = (char*)malloc(4 * sizeof(char));
    memcpy(prots[10].name, "SSH", 4);
    prots[10].regex = (char*) malloc(29 * sizeof(char));
    memcpy(prots[10].regex, ".*ssh-connection.*", 29);
    prots[10].hex = (char*)malloc(strlen("7373682D636F6E6E656374696F6E") + 1);
    memcpy(prots[10].hex, "7373682D636F6E6E656374696F6E", strlen("7373682D636F6E6E656374696F6E")+1);
}

char* whatprotocol(char *message, Client *c) {
    if(ntohs(c->address.sin_port) == 22)
        return "SSH";
    else if(ntohs(c->address.sin_port) == 21) 
        return "FTP";

    for (int i = 0; i < 11; i++) {
        
        regex_t regex2;
        int aux;

        // Compile the regular expression
        printf("%s\t", prots[i].regex);
        printf("%s\n", prots[i].regex);
        aux = regcomp(&regex2, prots[i].regex, REG_EXTENDED);

        if (aux != 0) {
            fprintf(stderr, "Error compiling regex for protocol %d\n", i + 1);
            return "TCP protocol";  
        }

        // Execute the regular expression match
        aux = regexec(&regex2, message, 0, NULL, 0);

        regfree(&regex2);

        if (aux == 0) {
            // Match found
            return prots[i].name;
        }
    }
    return "TCP protocol";
}

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
    protocols = (char**)malloc(sizeof(char*));
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
            if(strstr(buf, "prot"))
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
                                    protocols = (char**)realloc(protocols, (prot_no + 1)*sizeof(char*));
                                    protocols[prot_no] = (char*)malloc(sizeof(char)*(strlen(buf)+1));
                                    strcpy(protocols[prot_no], buf);
                                    prot_no++;
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

    printf("Protocols for detection:\n");
    for(int i = 0; i < prot_no; i++)
        printf("%s\n", protocols[i]);

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

    printf("Replace ip %d:\n", protocols);
    for(int i = 0; i < prot_no; i++)
        printf("%s %s\n", protocols[i]);

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

    for (size_t i = 0; i < len; i++) {
        sprintf(hex + 2 * i, "%02X", input[i]);
    }

    hex[2 * len] = '\0';
    return hex;
}

char* replaceBytes(const char* message) {
    char* hexMessage = charToHex(message);

    size_t hexLen = strlen(hexMessage);

    for (size_t i = 0; i < hexLen; i += 2) {
        for (int j = 0; j < rplbytes; j++) {
            if (strncmp(hexMessage + i, rBytes.initialValue[j], 2) == 0) {
                strncpy(hexMessage + i, rBytes.replacedValue[j], 2);
                break;
            }
        }
    }

    return hexMessage;
}

char* replaceCustomBytes(const char* message, const char* bytes2Replace, const char* replacement, int* lenght) {
    if (message == NULL || bytes2Replace == NULL || replacement == NULL) {
        perror("Invalid arguments");
        exit(-1);
    }

    size_t messageLength = strlen(message);
    size_t bytes2ReplaceLength = strlen(bytes2Replace);
    size_t replacementLength = strlen(replacement);
    printf("Meslen:%ld\n",messageLength);
    printf("b2rlen:%ld\n",bytes2ReplaceLength);
    printf("rlen:%ld\n",replacementLength);
    //printNow("la replace\n");

    char* result = (char*)malloc((messageLength - bytes2ReplaceLength + replacementLength + 1) * sizeof(char));
    if (result == NULL) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }

    size_t i, j, k;
    k = 0;

    int found = 0;
    for (i = 0; i < messageLength; i++) {
        if (strncmp(&message[i], bytes2Replace, bytes2ReplaceLength) == 0 && found == 0) {
            //printNow("am intrat in comparatie\n");
            for (j = 0; j < replacementLength; j++) {
                result[k++] = replacement[j];
               
                found = 1;
            }
            i += bytes2ReplaceLength -1;
        } else {
            result[k++] = message[i];
       
        }
    }
    result[k] = '\0';
    //printf("\n");
    //printNow(result);

    //printNow("am iesit din for\n");
    *lenght = k;
    history("Proxy","has replaced bytes as dictated in CLI");

    return result;
}

char* printType(char* message) //client message in clear -- detect protocol function for bytes
{
    //printf("\nGot in type function\n");
    size_t len = strlen(message);
    char* hex = (char*)malloc(2 * len + 1);
    hex = charToHex(message); //message bytes
    //printf("\nGot hex of message\n");
    //printf("%s\n", hex);

    //we have the hex, we compare to known hex bytes of protocols
    for(int i = 0; i < 11; i++)
    {
        //printf("%s\n", prots[i].hex);
        if(strstr(hex, prots[i].hex)!=NULL)
        {
            //printf("\ngot prot name\n");
            return prots[i].name;
        }
            //printf("\nno prot name\n");
    }
    return "TCP"; //not a known protocol
}

int hex_to_int(char c){
        int first = c / 16 - 3;
        int second = c % 16;
        int result = first*10 + second;
        if(result > 9) result--;
        return result;
}

char* hexToAscii(char* hex, int hex_len) {
    //printNow(hex);
    //int hex_len=strlen(hex);
    
    if (hex_len % 2 != 0) {
        // Invalid hex string
        return NULL;
    }

    // for(int i = 0; i <= hex_len; i++)
    //     printf("%c ", hex[i]);

   //printNow("la hex to ascii\n");

    int ascii_len = hex_len / 2;

    char* ascii = (char*)malloc(ascii_len + 1);

    for (int i = 0; i < ascii_len; i++) {
        //printf("Se calculeaza traducerea in ascii\n");
        if(2*i < hex_len) {
            int c1 = hex_to_int(hex[2*i]);
            int c2 = hex_to_int(hex[2*i+1]);
            ascii[i] = c1*16 + c2;
        }
    }

    //printNow("gata");
    ascii[ascii_len] = '\0';
    return ascii;
}

int protIsValid(char* message) //forward directly
{
    for (int i = 0; i < prot_no; i++) {
        regex_t regex;
        int ret;

        // Compile the regular expression
        ret = regcomp(&regex, protocols[i], REG_EXTENDED);
        if (ret != 0) {
            fprintf(stderr, "Error compiling regex for protocol %d\n", i + 1);
            return -1;  // Error code
        }

        // Execute the regular expression match
        ret = regexec(&regex, message, 0, NULL, 0);
        regfree(&regex);

        if (ret == 0) {
            // Match found
            return 1;
        } else if (ret != REG_NOMATCH) {
            // Error in matching
            fprintf(stderr, "Error matching regex for protocol %d\n", i + 1);
            return -1;  // Error code
        }
    }

    // No match found
    return 0;
}

void *handle_client(void *arg) {
    Client *client = (Client *)arg;
    char buffer[2000];
    int valread;
    
    while (1) {
        valread = read(client->socket, buffer, sizeof(buffer));

        if(protIsValid(buffer) == 1)
        {//forward directly --the protocol is in the white
            printf("Client message: \n");
            hex_dump(buffer, client);
            pthread_mutex_lock(&mutex);
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
            } else {
                buffer[valread] = '\0';
                printf("\nServer message to client(%d):\n", client->clientNumber);
                hex_dump(buffer, client);

                // Send server message to client
                send(client->socket, buffer, strlen(buffer), 0);
                pthread_mutex_unlock(&mutex);
            }
        }else{
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
                hex_dump(buffer, client);
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
                            hex_dump(buffer, client);

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

                        int length = 0;
                        char* replacedHex = (char*)malloc(2 * len + strlen(replacement) - strlen(bytes2Replace) + 1);
                        replacedHex= replaceCustomBytes(hex, bytes2Replace, replacement, &length);
                        //printNow(replacedHex);

                        printNow(replacedHex);
                        printf("Length: %d\n", length);
                        char* ascii = (char*)malloc(length/2 + 1);
                        printf("Size of ascii: %ld\n", strlen(ascii));
                        printf("Size of replacedHex: %ld\n", strlen(replacedHex));

                        printf("Length: %d\n", length);
                        ascii = hexToAscii(replacedHex, length);
                        //printNow(ascii);

                        printf("Replaced message: %s\n");
                        hex_dump(ascii, client);

                        // Forward message to server
                        send(server_socket, ascii, strlen(ascii), 0);
                        history("Proxy", "forwarded the packet to the server");

                        int valread = recv(server_socket, buffer, sizeof(buffer), 0);
                        buffer[valread] = '\0';

                        printf("\nServer message to client(%d):\n", client->clientNumber);
                        hex_dump(buffer, client);
                        history("Server", "sent a response packet to the proxy");
                        // Send server message to client
                        send(client->socket, buffer, strlen(buffer), 0);
                        history("Proxy", "forwarded the packet to the client");

                        pthread_mutex_unlock(&mutex);
                    }else{
                        history("Proxy", "attempted harmful action.");
                        printf("Wrong choice. Try again.\n");
                        printf("Select (F) Forward, (D) Drop, (R) Replace Bytes, (C) Custom replace bytes\n");
                        pthread_mutex_unlock(&mutex);
                    }
                }while(ok == 0);
                // Forward the server's response to the client
                //send(client->socket, buffer, strlen(buffer), 0);
            }
        }
    }

    free(client);
    pthread_exit(NULL);
}

void hex_dump(const char* message, Client *c) {
    ssize_t length = strlen(message);
    size_t i, j;
    char *numeProtocol = (char*)malloc(20 * sizeof(char));
    //printf("Got in hexdump");
    numeProtocol = printType(message);
    //numeProtocol = whatprotocol(message, c);
    printf("Protocol: %s\n", numeProtocol);

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
    protocols = (char**)malloc(sizeof(char*));
    rBytes.initialValue = (char**)malloc(sizeof(char*));
    rBytes.replacedValue = (char**)malloc(sizeof(char*));

    getRules(argv[4]);
    initialize_protocols();

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