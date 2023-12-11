#pragma once

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <regex.h>

#include <string.h>
#include <strings.h>


#define MAX_LEN 256
#define SERVER_PORT 21


#define READY_TO_AUTH 220
#define READY_TO_PASS 331
#define LOGIN_SUCCESS 230
#define PASSIVE_MODE 227
#define FILE_OK 150
#define TRANSFER_COMPLETE 226
#define QUIT 221

struct urlArguments {
    char host[MAX_LEN]; // 'ftp.up.pt'
    char path[MAX_LEN]; 
    char user[MAX_LEN];
    char password[MAX_LEN];
    char filename[MAX_LEN];
    char ip[MAX_LEN];
};

typedef enum{
    INITIALIZING,  
    RECEIVING_SINGLE,  //state that receives a single response
    RECEIVING_MULTIPLE,  // state that receives a multiple response
    RESPONSE_COMPLETE 
} State;

//Functions

int parseURL(char *url, struct urlArguments *arguments);
int createSocket(char *ip, int port);
int readResponse(int socket, char *buf);
int getIP(char *host, char *ip);
int authentication(int socket, char *user, char *password);
int changePassiveMode(int socket, char *ip, int *port);
int requestPath(int socket, char *path);
int getFile(int socketA, int socketB, char *filename);
int endConnection(const int socketA, const int socketB);
