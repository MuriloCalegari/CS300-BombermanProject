#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "match.h"

int connect_to_server(int port, char* addr){
    //*** create socket ***
    int sockfd = socket(PF_INET6, SOCK_STREAM, 0);
    if(sockfd == -1){
        perror("socket creation");
        return -1;
    }

    //*** create server address ***
    struct sockaddr_in6 server_address;
    memset(&server_address, 0,sizeof(server_address));
    server_address.sin6_family = AF_INET6;
    server_address.sin6_port = htons(port);
    inet_pton(AF_INET6, addr, &server_address.sin6_addr);

    //*** connect to server ***
    int r = connect(sockfd, (struct sockaddr *) &server_address, sizeof(server_address));
    if(r == -1){
        perror("connection failed");
        close(sockfd);
        return -1;
    }

    return sockfd;
}


int main(int argc, char** args){
    int sockfd;
    int port = atoi(args[1]);
    char* addr = args[2];

    if(argc != 3){
        printf("usage: %s <port> <address>\n", args[0]);
        return 1;
    }

    if((sockfd = connect_to_server(port, addr)) < 0){
        perror("connection");
        return 1;
    }

    printf("connection successful\n");

    //*** send a message ***
    // uint16_t header;
    // generate_header(codereq, id, eq, &header);
    // int written = send(sockfd, &header, sizeof(uint16_t), 0);
    // if(written <= 0){
    //     perror("write error");
    //     exit(3);
    // }

    return 0;
}