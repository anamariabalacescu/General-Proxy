#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>

int socket_desc2;
struct sockaddr_in server_addr2;

char* server_ip;
char* server_port;

void chat2server(char * client_message)
{

    int server_size2 = sizeof(server_addr2);
    if(send(socket_desc2, client_message, strlen(client_message), 0) < 0){
        printf("Unable to send message\n");
        exit(-1);
    }

    char server_message[2000];
    if(recv(socket_desc2, server_message, sizeof(server_message), 0) < 0){
        printf("Error while receiving server's msg\n");
        exit(-1);
    }

    //return server_message;
}

//parse arguments server_ip server_port client_port
int main(int argc, char* argv[])
{
    if(argc < 4)
    {
        perror("Missing parameter");
        exit(-1);
    }

    printf("s_ip:%s\n s_port:%s\n client_port:%s", argv[1], argv[2], argv[3]);

    //strcpy(server_ip, argv[1]);
    server_ip = argv[1];
    server_port = argv[2];

    int socket_desc, client_sock, client_size;
    struct sockaddr_in server_addr, client_addr;
    char server_message[2000], client_message[2000];
 
    // Clean buffers:
    memset(server_message, '\0', sizeof(server_message));
    memset(client_message, '\0', sizeof(client_message));

    // Create socket:
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);

    if(socket_desc < 0){
        printf("Error while creating socket\n");
        return -1;
    }
    printf("Socket created successfully\n");
    
     // Set port and IP that we'll be listening for, any other IP_SRC or port will be dropped: => for proxy - listening on client_port
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[3]));
    server_addr.sin_addr.s_addr = inet_addr("10.0.2.15");
    
    // Bind to the set port and IP:
    if(bind(socket_desc, (struct sockaddr*)&server_addr, sizeof(server_addr))<0){
        printf("Couldn't bind to the port\n");
        return -1;
    }
    printf("Done with binding\n");
    
        // Listen for clients:
    if(listen(socket_desc, 1) < 0){
        printf("Error while listening\n");
        return -1;
    }
    printf("\nListening for incoming connections.....\n");
 
        // Accept an incoming connection from one of the clients:
    client_size = sizeof(client_addr);
    client_sock = accept(socket_desc, (struct sockaddr*)&client_addr, &client_size);
    
    if (client_sock < 0){
        printf("Can't accept\n");
        return -1;
    }
    printf("Client connected at IP: %s and port: %i\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));



    //Server set-up
    if(socket_desc2 < 0){
        printf("Unable to create socket\n");
        exit(-1);
    }
 
    printf("Socket created successfully\n");
 
    // Set port and IP the same as server-side:
    server_addr2.sin_family = AF_INET;
    server_addr2.sin_port = htons(atoi(argv[2]));
    server_addr2.sin_addr.s_addr = inet_addr(server_ip);
 

    socket_desc2 = socket(AF_INET, SOCK_STREAM, 0);
    // Send connection request to server:
    if(connect(socket_desc2, (struct sockaddr*)&server_addr2, sizeof(server_addr2)) < 0){
        printf("Unable to connect\n");
        exit(-1);
    }
    printf("Connected with server successfully\n");
    ///end server set up

        // Receive client's message:
        // We now use client_sock, not socket_desc
        while(1)
        {
            if (recv(client_sock, client_message, sizeof(client_message), 0) < 0){
                printf("Couldn't receive\n");
                return -1;
            }
            printf("Msg from client: %s\n", client_message);
        
            chat2server(client_message);
            // Respond to client:
            strcpy(server_message, "This is the server's message.");
            //strcpy(server_message, chat2server(client_message));

            if (send(client_sock, server_message, strlen(server_message), 0) < 0){
                printf("Can't send\n");
                return -1;
            }

            memset(client_message, '\0', sizeof(client_message));
            memset(server_message, '\0', sizeof(client_message));
        }

        // Closing the socket:
        close(client_sock);

    close(socket_desc);
 
    return 0;
}