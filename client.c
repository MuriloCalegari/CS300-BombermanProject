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
        perror("connect_to_server");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

void print_header(MessageHeader* header){
    printf("MessageHeader: CODEREQ(%d) ID(%d) EQ(%d)\n",
        GET_CODEREQ(header), GET_ID(header), GET_EQ(header));
}

int start_match(int sockfd, uint16_t mode) {
    MessageHeader header;
    memset(&header, 0, sizeof(header));
    SET_CODEREQ(&header, mode);

    printf("\nStarting match with server. Sending header:\n");
    print_header(&header);

    if(send(sockfd, &header, sizeof(header), 0) == -1){
        perror("start_match, send");
        return -1;
    }

    return 0;
}


int main(int argc, char** args){
    int sockfd;

    if(argc != 3){
        printf("usage: %s <port> <address>\n", args[0]);
        return 1;
    }

    int port = atoi(args[1]);
    char* addr = args[2];

    if((sockfd = connect_to_server(port, addr)) < 0){
        printf("Connecting to server failed. Exiting...");
        return 1;
    }

    printf("connection successful\n");

    if(start_match(sockfd, 1) < 0){
        printf("start_match failed\n");
        return 1;
    }

    return 0;
}