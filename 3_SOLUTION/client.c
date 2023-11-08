#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define delim -
/*ip: \n endlrows=1;
x.x.x.x \n
y.y.y.y \n
z.z.z.z \n
\n endlrows=2;
mac:
xx:xx:xx:xx:xx:xx
--------block

------replace
ipr: //replace ip
x.x.x.x - y.y.y.y

bytesr: //octeti
OA OB - OD OC



char*** blocked -ip si mac
char*** replace - ipr si replace*/
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

void getPreferences(char** filepath)
{
        // Initialize arrays
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

    char *buf;
    ssize_t bytes_read;
 
    buf = malloc(4001 * sizeof(char));
    /* read the last 100 characthers */
    bytes_read = read(f, buf, 4000);
 
    /* add '\0' at end of buffer to terminate the string properly */
    buf[bytes_read] = '\0';
 
    rc = close(f);

    //avem in buffer continutul fisierului de reguli

    int endlrows = 0; //consecutive
    int section = 0;
    char* gottenIp = malloc(sizeof(char)*16);
    char* gottenMac = malloc(sizeof(char)*18);
    char* replBytes = malloc(sizeof(char)*32);
    int p = 0;
    int ok = 0;
    int rowInSection = 0;
    for(int i = 0; i < bytes_read; i++)
    {   
        if(buf[i] == '\n' || buf == EOF)
        {
            if(gottenIp != NULL && section == 0 && rowInSection > 1 && endlrows == 1)
            {
                gottenIp[p] = NULL;
                printf("%s", gottenIp);
                blockedIp = (char**)realloc(blockedIp, (blckip + 1)*sizeof(char*));
                blockedIp[blckip] = (char*)malloc(sizeof(char)*(strlen(gottenIp)+1));
                strcpy(blockedIp[blckip++], gottenIp);
                p=0;
                memset(gottenIp, '\0', sizeof(gottenIp));
            }

            if(gottenMac != NULL && section == 1 && rowInSection > 1 && endlrows == 1)
            {
                gottenMac[p] = NULL;
                printf("%s", gottenMac);
                blockedMac = (char**)realloc(blockedMac, (blckmac + 1)*sizeof(char*));
                blockedMac[blckmac] = (char*)malloc(sizeof(char)*(strlen(gottenMac)+1));
                strcpy(blockedMac[blckmac++], gottenMac);
                p=0;
                memset(gottenMac, '\0', sizeof(gottenMac));
            }

            if(gottenIp != NULL && section == 2 && rowInSection > 1 && endlrows == 1)
            {
                ok = 0;
                gottenIp[p] = NULL;
                ips.replacedValue = (char**)realloc(ips.replacedValue, (rplip + 1)*sizeof(char*));
                ips.replacedValue[rplip] = (char*)malloc(sizeof(char)*(strlen(gottenIp)+1));
                strcpy(ips.replacedValue[rplip++],gottenIp);

                int x = rplip -1;
                printf("%s ",ips.initialValue[x]);
                printf( "se schimba cu %s",ips.replacedValue[x]);

                p=0;
                memset(gottenIp, '\0', sizeof(gottenIp));
            }

            if(replBytes != NULL && rowInSection > 1 && section == 3 && ok == 1)
            {
                ok = 0;
                replBytes[p] = NULL;
                rBytes.replacedValue = (char**)realloc(rBytes.replacedValue, (rplbytes+ 1)*sizeof(char*));
                rBytes.replacedValue[rplbytes] = (char*)malloc(sizeof(char)*(strlen(replBytes)+1));
                strcpy(rBytes.replacedValue[rplbytes++],replBytes);
                
                printf("%s se schimba cu %s",rBytes.initialValue[rplbytes-1],rBytes.replacedValue[rplbytes-1]);
                
                p=0;
                memset(replBytes, '\0', sizeof(replBytes));
            }

            endlrows++;
            if(endlrows==2)
            {
                section++;
                rowInSection = 0;
            }
        }
        else{
            endlrows = 0;
        }
        if(section == 0)//|| section == 1) //suntem in sector blocare ip sau mac
        {
            if(buf[i]=='i') //buf[i]=i buf[]
            {
                i=i+3;
                rowInSection++;
            }
            else{
                rowInSection = 2;
                gottenIp[p++] = buf[i];
            }
        }
        else{
            if(section==1)
            {
                if(buf[i]=='m')
                {
                    i=i+3;
                    rowInSection++;
                }
                else{
                    rowInSection = 2;
                    gottenMac[p++] = buf[i];
                }
            }
            else{
                if(section==2)
                {
                    if(buf[i]=='i')
                    {
                        i=i+3;
                        rowInSection++;
                    }
                    else{
                        rowInSection = 2;
                        if(buf[i] != '-')
                        {
                            gottenIp[p++] = buf[i];
                        }
                        else
                        {
                            gottenIp[p] = NULL;
                            ips.initialValue = (char**)realloc(ips.initialValue, (rplip + 1)*sizeof(char*));
                            ips.initialValue[rplip] = (char*)malloc(sizeof(char)*(strlen(gottenIp)+1));
                            strcpy(ips.initialValue[rplip], gottenIp);
                            //printf("%s primul ip", ips.initialValue[rplip]);
                            if(ok == 0)
                            {
                                memset(gottenIp, '\0', sizeof(gottenIp));
                                p = 0;
                                ok = 1;
                            }
                        }
                    }
                }
                else{
                    if(buf[i]=='b')
                    {
                        i=i+5;
                        rowInSection++;
                    }
                    else
                    {
                        rowInSection = 2;
                        if(buf[i] != '-')
                        {
                            replBytes[p++] = buf[i];
                        }
                        else
                        {
                            replBytes[p] = NULL;
                            rBytes.initialValue = (char**)realloc(rBytes.initialValue, (rplbytes+ 1)*sizeof(char*));
                            rBytes.initialValue[rplbytes] = (char*)malloc(sizeof(char)*(strlen(replBytes)+1));
                            rBytes.initialValue[rplbytes] = replBytes;
                            if(ok == 0)
                            {
                                memset(replBytes, '\0', sizeof(replBytes));
                                p=0;
                                ok = 1;
                            }
                        }
                    }
                }
            }
        }
    }
}

//parse proxy_ip client_port filepath
int main(int argc, char *argv[])
{
    printf("%s\t%s\t%s\n", argv[1], argv[2], argv[3]);
    getPreferences(argv[3]);
    int socket_desc;
    struct sockaddr_in server_addr;
    char server_message[2000], client_message[2000];

    // Clean buffers:
    memset(server_message, '\0', sizeof(server_message));
    memset(client_message, '\0', sizeof(client_message));

    // Create socket, we use SOCK_STREAM for TCP
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);

    if (socket_desc < 0)
    {
        printf("Unable to create socket\n");
        return -1;
    }

    printf("Socket created successfully\n");

    // Set port and IP the same as server-side:
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[2])); //client port
    server_addr.sin_addr.s_addr = inet_addr(argv[1]); /// server ip

    // Send connection request to server:
    if (connect(socket_desc, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        printf("Unable to connect\n");
        return -1;
    }
    printf("Connected with server successfully\n");

    while (1)
    {
        memset(client_message, '\0', sizeof(client_message));
        // Get input from the user:
        printf("Enter message: ");
        gets(client_message);

        // Send the message to server:
        if (send(socket_desc, client_message, strlen(client_message), 0) < 0)
        {
            printf("Unable to send message\n");
            return -1;
        }

        // Receive the server's response:
        if (recv(socket_desc, server_message, sizeof(server_message), 0) < 0)
        {
            printf("Error while receiving server's msg\n");
            return -1;
        }

        printf("Server's response: %s\n", server_message);
    }

    // Close the socket:
    close(socket_desc);
    


    return 0;
}