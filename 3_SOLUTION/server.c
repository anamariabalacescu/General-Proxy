#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <fcntl.h>

#define linechar 128
#define delim -

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int max_client = 0;

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
 
    //buf = malloc((linechar + 1) * sizeof(char));
    /* read the last 100 characthers */
    //bytes_read = read(f, buf, linechar);

    // if(bytes_read == 0)
    // {
    //     perror("File is empty");
    //     exit(-1);
    // }

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

char *makeOneString(char *s1, char *s2) {
    char *string = malloc(strlen(s1) + strlen(s2) + 2); // +2 for the '-' and '\0'
    if (string == NULL) {
        perror("malloc failed");
        exit(-1);
    }

    strcpy(string, s1);
    strcat(string, "-");
    strcat(string, s2);

    return string;
}

void sendRules(int client_sock)
{
    //need to send blckips, blckmacs, rplips, rplbytes => no of strings sent <= the sum of those - 4 + 4 strings
    // the  4 strings will be the number needed for each category; if one is 0 then we don't need to send anything; the response will be implemented in the proxy 
    // Sending blocked ip addresses

    int len = 0;
    if (blckip == 0) {
        send(client_sock, 0, sizeof(int), 0);
    } else {
        printf("%d\n", blckip);
        send(client_sock, &blckip, sizeof(blckip), 0);
        for (int i = 0; i < blckip; i++) {
            len = strlen(blockedIp[i]);
            send(client_sock, &len, sizeof(int), 0);
            send(client_sock, blockedIp[i], strlen(blockedIp[i]), 0);
            len = 0;
        }
    }

    printf("aici 1\n");
    // Sending blocked mac addresses
    if (blckmac == 0) {
        send(client_sock, 0, sizeof(int), 0);
    } else {
        char number[100];
        sprintf(number, "%d", blckmac);
        send(client_sock, &blckmac, sizeof(blckmac), 0);
        for (int i = 0; i < blckmac; i++) {
            len = strlen(blockedMac[i]);
            send(client_sock, &len, sizeof(int), 0);
            send(client_sock, blockedMac[i], strlen(blockedMac[i]), 0);
        }
    }

    printf("aici 2\n");

    // Sending bytes to replace
    if (rplbytes == 0) {
        send(client_sock, 0, sizeof(int), 0);
    } else {
        char number[100];
        sprintf(number, "%d", rplbytes);
        send(client_sock, &rplbytes, sizeof(rplbytes), 0);

    printf("aici 3\n");
        for (int i = 0; i < rplbytes; i++) {
            char *message = makeOneString(rBytes.initialValue[i], rBytes.replacedValue[i]);
            printf("%s\n", message);
            len = strlen(message);
            send(client_sock, &len, sizeof(int), 0);
            send(client_sock, message, strlen(message), 0);
            free(message);
        }

    printf("aici 4\n");
    }
}


/// argv[1] = port number ....... argv[2] = rules filepath
int main(int argc, char* argv[]) {

    if(argc < 3) {
        perror("Missing parameters.\n");
        exit(-1);
    }

    getRules(argv[2]);

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
    server_addr.sin_addr.s_addr = inet_addr("192.168.1.112");

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

        sendRules(client_sock); //trimitem proxy-ului regulile si blacklistul

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
