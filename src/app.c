#include "../include/app.h"

//protocol FTP (File Transfer Protocol) client

int parseURL(char *input_url, struct urlArguments *url){
    //input_url format - ftp://[<user>:<password>@]<host>/<url-path>
    //input_url format anonymous - ftp://<host>/<url-path>

    const char *ftp_prefix = "ftp://";
    if (strncmp(input_url, ftp_prefix, strlen(ftp_prefix)) != 0) {
        printf("Invalid URL format\n");
        return -1;  // Indicate failure
    }

    char *url_part = input_url + strlen(ftp_prefix);

    // Find the user and password
    char *user_end = strchr(url_part, ':');
    if (user_end == NULL) {

        printf("WARNING: Anonymous MODE\n");
        strncpy(url->user, "anonymous", sizeof(url->user) - 1);
        strncpy(url->password, "anonymous@", sizeof(url->password) - 1);

        url_part = url_part; 
    } else {
        // Parse the user and password
        strncpy(url->user, url_part, user_end - url_part);
        url->user[user_end - url_part] = '\0';

        char *password_end = strchr(user_end, '@');
        if (password_end == NULL) {
            printf("ERROR: No password\n");
            return -1;
        }

        strncpy(url->password, user_end + 1, password_end - user_end - 1);
        url->password[password_end - user_end - 1] = '\0'; //

        url_part = password_end + 1; 
    }

    // Find the host and path
    char *host_end = strchr(url_part, '/');
    if (host_end == NULL) {
        printf("ERROR: No path\n");
        return -1;
    }

    strncpy(url->host, url_part, host_end - url_part);
    url->host[host_end - url_part] = '\0';

    strncpy(url->path, host_end + 1, MAX_LEN - 1);

    // Find the filename
    char *filename_start = strrchr(input_url, '/');
    if (filename_start == NULL) {
        printf("Invalid URL format\n");
        return -1;
    }

    strncpy(url->filename, filename_start + 1, MAX_LEN - 1);

    if(getIP(url->host, url->ip) != 0){
        printf("ERROR: Could not get IP\n");
        return -1;
    }
    return 0;  

}

int createSocket(char *ip, int port){
    int sockfd;
    struct sockaddr_in server_addr;

    /*server address handling*/
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);    
    server_addr.sin_port = htons(port);        

    /*open a TCP socket*/
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        exit(-1);
    }
    /*connect to the server*/
    if (connect(sockfd,
                (struct sockaddr *) &server_addr,
                sizeof(server_addr)) < 0) {
        perror("connect()");
        exit(-1);
    }
    return sockfd;
}

int readResponse(int socket, char *buf){
    char byte = 0;
    int i = 0; 
    State state = INITIALIZING;
    memset(buf, 0, MAX_LEN);

    while (state != RESPONSE_COMPLETE) {
        if (read(socket, &byte, 1) < 0) {
            printf("ERROR: Could not read from socket\n");
            return -1;
        }

        switch (state) {
            case INITIALIZING:
                if (byte == ' ') {
                    state = RECEIVING_SINGLE;
                } else if (byte == '-') {
                    state = RECEIVING_MULTIPLE;
                } else if (byte == '\n') {
                    state = RESPONSE_COMPLETE;
                }
                else buf[i++] = byte;
                break;
            case RECEIVING_SINGLE:
                if (byte == '\n') {
                    state = RESPONSE_COMPLETE;
                }
                else buf[i++] = byte;
                break;
            case RECEIVING_MULTIPLE:
                if (byte == '\n') {
                    memset(buf, 0, MAX_LEN);
                    state = INITIALIZING;
                    i = 0;
                }
                else buf[i++] = byte;
                break;
            case RESPONSE_COMPLETE:
                break;
            default:
                printf("ERROR: Invalid state\n");
        }
    }

    printf("Buffer: %s\n", buf);
    int responseCode = atoi(buf);
    printf("Code: %d\n", responseCode);

    return responseCode;
}



int getIP(char *host, char *ip) {
    struct hostent *h;

    if ((h = gethostbyname(host)) == NULL) {
        herror("gethostbyname");
        return -1;
    }

    strcpy(ip, inet_ntoa(*((struct in_addr *) h->h_addr)));

    return 0;
}   

int authentication(int socket, char *user, char *password) {
    char userCommand[6 + strlen(user) + 1];  // "USER " + user + "\n"
    char passCommand[6 + strlen(password) + 1];  // "PASS " + password + "\n"
    char result[MAX_LEN];

    snprintf(userCommand, sizeof(userCommand), "USER %s\n", user);

    write(socket, userCommand, strlen(userCommand));

    // Check server response
    if (readResponse(socket, result) != READY_TO_PASS) {
        printf("ERROR: Server not ready to receive password\n");
        return -1;
    }

    snprintf(passCommand, sizeof(passCommand), "PASS %s\n", password);

    write(socket, passCommand, strlen(passCommand));

    return readResponse(socket, result);
}


int changePassiveMode(int socket, char *ip_address, int *port_socketB) {
    char pasvCommand[] = "pasv\n";
    char result[MAX_LEN];
    int ip1, ip2, ip3, ip4, port1, port2;

    //Send passive mode command
    write(socket, pasvCommand, strlen(pasvCommand)); 

    if (readResponse(socket, result) != PASSIVE_MODE) {
        printf("ERROR: Server not ready to enter passive mode\n");
        return -1;
    }
    sscanf(result, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)", &ip1, &ip2, &ip3, &ip4, &port1, &port2);
   
    *port_socketB = port1 * 256 + port2; // Calculate port number
  
    sprintf(ip_address, "%d.%d.%d.%d", ip1, ip2, ip3, ip4);
 
    return 0;
}

int requestPath(int socket, char *path) {
    char retrCommand[5+strlen(path)+1];
    char result[MAX_LEN];

    sprintf(retrCommand, "retr %s\n", path);
    write(socket, retrCommand, strlen(retrCommand));

    if (readResponse(socket, result) != FILE_OK) {
        printf("ERROR: Server not ready to receive file\n");
        return -1;
    }

    return 0;
}

int getFile(int socketB, int socketA, char *filename) {

    FILE *file = fopen(filename, "w");

    if (file == NULL) {
        printf("ERROR: Could not open file\n");
        return -1;
    }

    char buf[MAX_LEN];
    int bytes;

    // Read from socket B and write to file
    while ((bytes = read(socketB, buf, MAX_LEN)) > 0) {
        if (fwrite(buf, bytes, 1, file) < 0) {
        fclose(file);
        return -1;
    }
    }

    fclose(file);

    if (readResponse(socketA, buf) != TRANSFER_COMPLETE) {
        printf("ERROR: File transfer not complete\n");
        return -1;
    }

    return 0;
}

int endConnection(const int socketA, const int socketB) {
    
    char result[MAX_LEN];
    char quitCommand[] = "quit\n";

    write(socketA, quitCommand, strlen(quitCommand));

    if(readResponse(socketA, result) != QUIT) {
        printf("ERROR: Could not quit\n");
        return -1;
    }
    
    return close(socketA) || close(socketB);
}

int main(int argc, char **argv)
{

    if (argc != 2)
    {
        fprintf(stderr,"Wrong number of arguments. Usage: download ftp://[<user>:<password>@]<host>/<url-path>\n");
        exit(1);
    }

    struct urlArguments url;
    memset(&url,0,sizeof(url)); // Initialize struct to 0

    if (parseURL(argv[1],&url) != 0)
    {
        fprintf(stderr,"Error parsing URL\n");
        exit(1);
    }

    // Print all the URL arguments 
    printf("Host name: %s\n", url.host);
    printf("Path: %s\n", url.path);
    printf("User: %s\n", url.user);
    printf("Password: %s\n", url.password);
    printf("File name: %s\n", url.filename);
    printf("IP Address: %s\n", url.ip);

    int socketA = createSocket(url.ip, SERVER_PORT);
    if (socketA < 0) {
        printf("ERROR: Could not create socket A\n");
        exit(-1);
    }

    char buf[MAX_LEN];

    if (readResponse(socketA, buf) != READY_TO_AUTH) {
        printf("ERROR: Server not ready to authenticate\n");
        exit(-1);
    }

    // Authentication
    if (authentication(socketA, url.user, url.password) != LOGIN_SUCCESS) {
        printf("ERROR: Could not authenticate\n");
        exit(-1);
    }

    // Passive mode
    int port_socketB;
    char ip_address[MAX_LEN];
    if (changePassiveMode(socketA, ip_address, &port_socketB) != 0) {
        printf("ERROR: Could not enter passive mode\n");
        exit(-1);
    }

    // Create socket for data transfer
    int socketB = createSocket(ip_address, port_socketB);
    if (socketB < 0) {
        printf("ERROR: Could not create socket B\n");
        exit(-1);
    }
        
    if (requestPath(socketA, url.path) != 0) {
        printf("ERROR: Could not request path\n");
        exit(-1);
    }

    if (getFile(socketB, socketA, url.filename) != 0) {
        printf("ERROR: Could not get file\n");
        exit(-1);
    }

    if (endConnection(socketA, socketB) != 0) {
        printf("ERROR: Could not close sockets\n");
        exit(-1);
    } 

    return 0;

}