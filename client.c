#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <ncurses.h>
#include <string.h>
#include <net/if.h>
#include "ncurses/ncurses.h"
#include "client/context.h"
#include "client/network.h"
#include "common/util.h"

char* addr;

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

int start_match(player *pl, int mode) {
    MessageHeader header;
    memset(&header, 0, sizeof(header));
    if(mode == 1){
        pl->mode = NEW_MATCH_4_OPPONENTS;
    }else{
        pl->mode = NEW_MATCH_2_TEAMS;
    }
    SET_CODEREQ(&header, pl->mode);
    header.header_line = htons(header.header_line);

    printf("\nStarting match with server. Sending header:\n");
    print_header(&header);

    if(send(pl->socket_tcp, &header, sizeof(header), 0) == -1){
        perror("start_match, send");
        return -1;
    }

    // wait server response
    NewMatchMessage resp;
    memset(&resp, 0, sizeof(resp));

    printf("Waiting for server response...\n");
    if(read_loop(pl->socket_tcp, &resp, sizeof(resp), 0) <= 0){
        perror("start_match, recv");
        return -1;
    }

    // codereq check
    resp.header.header_line = ntohs(resp.header.header_line); // convert
    int test = GET_CODEREQ(&resp.header);
    printf("codereq %d\n", test);
    if(((GET_CODEREQ(&resp.header)) != SERVER_RESPONSE_MATCH_START_4_OPPONENTS && mode == MODE_NO_TEAM) ||
    ((GET_CODEREQ(&resp.header)) != SERVER_RESPONSE_MATCH_START_2_TEAMS && mode == MODE_2_TEAM)) {
        perror("start_match, error recv codereq");
        return -1;
    }

    pl->eq = GET_EQ(&resp.header);
    pl->id = GET_ID(&resp.header);
    memset(&pl->adr_udp, 0, sizeof(pl->adr_udp));
    memcpy(&pl->adr_udp, resp.adr_mdiff, sizeof(pl->adr_udp));
    //convertEndian(pl.adr_udp);

    pl->port_multidiff = ntohs(resp.port_mdiff);
    pl->port_udp = ntohs(resp.port_udp);

    //config sock_abonnement
    int ok = 1;
    if(setsockopt(pl->socket_multidiff, SOL_SOCKET, SO_REUSEADDR, &ok, sizeof(ok)) < 0) {
        perror("echec de SO_REUSEADDR");
        close(pl->socket_udp);
        return 1;
    }

    /* Initialization of the reception address */
    struct sockaddr_in6 adr;
    memset(&adr, 0, sizeof(adr));
    adr.sin6_family = AF_INET6;
    //memcpy(&adr.sin6_addr, &pl.adr_udp, sizeof(pl.adr_udp));
    adr.sin6_addr = in6addr_any;
    adr.sin6_port = htons(pl->port_multidiff);

    printf("Binding UDP socket to port %d\n", pl->port_multidiff);
    if(bind(pl->socket_multidiff, (struct sockaddr*) &adr, sizeof(adr))) {
        perror("echec de bind");
        close(pl->socket_udp);
        return 1;
    }

    // If address equals localhost and this is a mac then...
    
    // int ifindex = -1;
    
    // if(strcmp(addr, "localhost") == 0 || strcmp(addr, "::1") == 0) {
    //     #ifdef TARGET_OS_OSX
    //     printf("Running on MacOS with localhost, using if_nametoindex for loopback interface\n");
    //     ifindex = if_nametoindex ("lo0");
    //     if(ifindex == 0)
    //         perror("if_nametoindex");
    //     #endif
    // }

    // /* subscribe to the multicast group */
    // struct ipv6_mreq group;
    // memcpy(&group.ipv6mr_multiaddr.s6_addr, pl->adr_udp, sizeof(pl->adr_udp));
    // if(ifindex == -1)
    //     group.ipv6mr_interface = 0; // all interfaces
    // else
    //     group.ipv6mr_interface = ifindex; // override

    // Use inet_ntop to convert the address to a human-readable string
    // char addr_str[INET6_ADDRSTRLEN];
    // inet_ntop(AF_INET6, &group.ipv6mr_multiaddr, addr_str, INET6_ADDRSTRLEN);
    // printf("Joining multicast group address: %s\n", addr_str);

    int ifindex = if_nametoindex ("eth0");
    if(ifindex == 0)
        perror("if_nametoindex");

    struct ipv6_mreq group;
    memcpy(&group.ipv6mr_multiaddr.s6_addr, pl->adr_udp, sizeof(pl->adr_udp));
    group.ipv6mr_interface = ifindex;

    if(setsockopt(pl->socket_multidiff, IPPROTO_IPV6, IPV6_JOIN_GROUP, &group, sizeof group) < 0) {
        perror("echec de abonnement groupe");
        close(pl->socket_udp);
        return 1;
    }

    /* ready */
    memset(&header, 0, sizeof(MessageHeader));
    if(pl->mode == MODE_NO_TEAM){
        SET_CODEREQ(&header, CLIENT_READY_TO_PLAY_4_OPPONENTS);
    }else{
        SET_CODEREQ(&header, CLIENT_READY_TO_PLAY_2_TEAMS);
    }
    SET_ID(&header, pl->id);
    SET_EQ(&header, pl->eq);

    header.header_line = htons(header.header_line);

    if(write_loop(pl->socket_tcp, &header, sizeof(header), 0) <= 0){
        perror("start_match, send");
        return -1;
    }

    return 0;
}

int tchat_message(player *pl, char *data){
    TChatHeader message;
    memset(&message, 0, sizeof(message));

    if(strcmp(&data[0],"/") == 0 && strcmp(&data[0],"t") == 0 ) { // start message with "/t" for team tchat
        SET_CODEREQ(&message.header, T_CHAT_TEAM);
    }else{
        SET_CODEREQ(&message.header, T_CHAT_ALL_PLAYERS);
    }
    SET_ID(&message.header, pl->id);
    SET_EQ(&message.header, pl->eq);
    message.header.header_line = htons(message.header.header_line);

    message.data_len = strlen(data);
    int i;
    for(i=0; i<message.data_len-1; i++){
        message.data[i] = data[i];
    }
    message.data[i+1] = 0;

    if(write_loop(pl->socket_tcp, &message, sizeof(message), 0) <= 0){
        perror("tchat_message, send");
        return -1;
    }
    return 0;
}

int udp_message(player *pl, int action){

    ActionMessage buffer;
    memset(&buffer, 0, sizeof(buffer));
    if(pl->mode == MODE_NO_TEAM){
        SET_CODEREQ(&buffer.message_header, ACTION_MESSAGE_4_OPPONENTS);
    }else{
        SET_CODEREQ(&buffer.message_header, ACTION_MESSAGE_2_TEAMS);
    }
    SET_EQ(&buffer.message_header, pl->eq);
    SET_ID(&buffer.message_header, pl->id);
    SET_NUM(&buffer, (pl->num % NUM_MAX));
    pl->num = pl->num+1;
    SET_ACTION(&buffer, action);

    buffer.message_header.header_line = htons(buffer.message_header.header_line);

    struct sockaddr_in6 adr;
    memset(&adr, 0, sizeof(adr));
    adr.sin6_family = AF_INET6;
    // memcpy(&adr.sin6_addr, pl->adr_udp, sizeof(pl->adr_udp));
    inet_pton(AF_INET6, "::1", &adr.sin6_addr);
    adr.sin6_port = htons(pl->port_udp);

    int send = sendto(pl->socket_udp, &buffer, sizeof(buffer), 0, (struct sockaddr *)&adr, sizeof(adr));
    if(send < 0){
        perror("sendto fail");
        return -1;
    }

    return 0;
}

void *game_control(void *arg){
    player *pl = (player *)arg;
    while(pl->ready == 1){
        ACTION a = control(pl->g->lw);
        switch(perform_action(pl->g->b, pl->g->p, a)){
            case -1: // quit
                free_board(pl->g->b);
                curs_set(1); // Set the cursor to visible again
                endwin(); /* End curses mode */
                free_gameboard(pl->g);
                break;
            case 1:
                if(pl->g->lw->cursor > 0){
                    tchat_message(pl, pl->g->lw->data);
                    pl->g->lw->cursor=0;
                    memset(pl->g->lw->data, 0, SIZE_MAX_MESSAGE);
                }
                break;
            case 2: //left
                udp_message(pl, MOVE_WEST);
                break;
            case 3: //right
                udp_message(pl, MOVE_EAST);
                break;
            case 4: //up
                udp_message(pl, MOVE_NORTH);
                break;
            case 5: //down
                udp_message(pl, MOVE_SOUTH);
                break;
            case 6: //bomb
            // TODO
            default: break;
        }
    }
    pthread_exit(NULL);
}


void *before_game_control(void *arg){
    player *pl = (player *)arg;
    while(pl->ready == 0){
        ACTION a = control(pl->g->lw);
        switch(perform_action(pl->g->b, pl->g->p, a)){
            case -1: // quit
                free_board(pl->g->b);
                curs_set(1); // Set the cursor to visible again
                endwin(); /* End curses mode */
                free_gameboard(pl->g);
                break;
            case 1:
                if(pl->g->lw->cursor > 0){
                    tchat_message(pl, pl->g->lw->data);
                    pl->g->lw->cursor=0;
                    memset(pl->g->lw->data, 0, SIZE_MAX_MESSAGE);
                }
                break;
            default: break;
        }
        usleep(30*1000);
        refresh_game(pl->g->b, pl->g->lw, pl->g->lr);
    }
    pthread_exit(NULL);
}


void *read_tcp_tchat(void* arg){
    player *pl = (player *) arg;

    while(1){
        TChatHeader resp;
        memset(&resp, 0, sizeof(resp));

        if(read_loop(pl->socket_tcp, &resp, sizeof(resp), 0) <= 0)
            perror("read_tcp, recv");

        pthread_mutex_lock(&pl->mutex);

        char data[SIZE_MAX_MESSAGE];
        memcpy(&resp.data, data, sizeof(resp.data));

        //update tchat
        switch(pl->g->lr->nb_line){
            case 0:
                memcpy(&pl->g->lr->data[0], data, sizeof(data));
                pl->g->lr->nb_line++;
                break;
            case 1:
                memcpy(&pl->g->lr->data[1], data, sizeof(data));
                pl->g->lr->nb_line++;
                break;
            case 2:
                memcpy(&pl->g->lr->data[2], data, sizeof(data));
                pl->g->lr->nb_line++;
                break;
            default:
                memset(&pl->g->lr->data[0], 0, SIZE_MAX_MESSAGE); // clear source to get a new data
                memmove(&pl->g->lr->data[0], pl->g->lr->data[1], sizeof(pl->g->lr->data[0]));
                memset(&pl->g->lr->data[1], 0, SIZE_MAX_MESSAGE);
                memmove(&pl->g->lr->data[1], pl->g->lr->data[2], sizeof(pl->g->lr->data[0]));
                memset(&pl->g->lr->data[2], 0, SIZE_MAX_MESSAGE);
                memcpy(&pl->g->lr->data[2], data, sizeof(data));
                break;
        }
        pthread_mutex_unlock(&pl->mutex);
    }
    pthread_exit(NULL);
}

void *refresh_gameboard(void *arg){ // multicast
    player *pl = (player *) arg;

    while(1){
        socklen_t difflen = sizeof(pl->adr_udp);
        MessageHeader header;
        recvfrom(pl->socket_multidiff, &header, sizeof(header), 0, (struct sockaddr *) &pl->adr_udp, &difflen);
        header.header_line = ntohs(header.header_line);
        pl->ready = 1; // unlock control player

        if((GET_CODEREQ(&header)) == SERVER_FULL_MATCH_STATUS){
            uint16_t num;
            recvfrom(pl->socket_multidiff, &num, sizeof(num), 0, (struct sockaddr *) &pl->adr_udp, &difflen);
            u_int8_t height;
            recvfrom(pl->socket_multidiff, &height, sizeof(height), 0, (struct sockaddr *) &pl->adr_udp, &difflen);
            uint8_t width;
            recvfrom(pl->socket_multidiff, &width, sizeof(width), 0, (struct sockaddr *) &pl->adr_udp, &difflen);
            int len = height * width * sizeof(uint8_t);
            char data[len];
            recvfrom(pl->socket_multidiff, &data, sizeof(data), 0, (struct sockaddr *) &pl->adr_udp, &difflen);
            update_grid(pl->g->b, data);

            pthread_mutex_lock(&pl->mutex);
            refresh_game(pl->g->b, pl->g->lw, pl->g->lr);
            pthread_mutex_unlock(&pl->mutex);
        }else if((GET_CODEREQ(&header)) == SERVER_PARTIAL_MATCH_UPDATE){
            uint16_t num;
            recvfrom(pl->socket_multidiff, &num, sizeof(num), 0, (struct sockaddr *) &pl->adr_udp, &difflen);
            uint8_t count;
            recvfrom(pl->socket_multidiff, &count, sizeof(count), 0, (struct sockaddr *) &pl->adr_udp, &difflen);
            pl->freq = count;
        }

    }
    pthread_exit(NULL);
}


int main(int argc, char** args){
    player *pl = malloc(sizeof(player));
    pl->num = 0; // number of action
    pl->ready = 0;
    pthread_mutex_init(&pl->mutex, 0);

    if(argc != 4){
        printf("usage: %s <port> <address> <1:no team, 2:2team>\n", args[0]);
        return 1;
    }

    int port = atoi(args[1]);
    addr = args[2];
    pl->server_adr = args[2];
    if((pl->socket_tcp = connect_to_server(port, addr)) < 0){
        printf("Connecting to server failed. Exiting...");
        return 1;
    }

    printf("connection successful\n");

    if((pl->socket_multidiff = socket(AF_INET6, SOCK_DGRAM, 0)) < 0){
        perror("socket abonnement");
        return 1;
    }

    if((pl->socket_udp = socket(AF_INET6, SOCK_DGRAM, 0)) < 0){
        perror("socket udp");
        return 1;
    }

    if(start_match(pl, atoi(args[3])) < 0){
        printf("start_match failed\n");
        return 1;
    }

    pthread_t thread_tchat_read;
    pthread_t refresh_party;
    pthread_t action;
    pthread_t lobby;

    pl->g = create_board();
    refresh_game(pl->g->b, pl->g->lw, pl->g->lr);

    if(pthread_create(&lobby, NULL, before_game_control, pl)){
        perror("thread refresh party");
        return 1;
    }

    if(pthread_create(&refresh_party, NULL, refresh_gameboard, pl)){
        perror("thread refresh party");
        return 1;
    }
    if(pthread_create(&thread_tchat_read, NULL, read_tcp_tchat, pl)){
        perror("thread read tchat");
        return 1;
    }
    if(pthread_create(&action, NULL, game_control, pl)){
        perror("action thread");
        return 1;
    }

    //printf("Waiting on all threads to finish...\n");
    udp_message(pl, MOVE_WEST);

    pthread_join(thread_tchat_read, NULL);
    pthread_join(refresh_party, NULL);
    pthread_join(action, NULL);
    pthread_join(lobby, NULL);

    while(1){
        socklen_t difflen = sizeof(pl->adr_udp);
        MessageHeader header;
        recvfrom(pl->socket_multidiff, &header, sizeof(header), 0, (struct sockaddr *) &pl->adr_udp, &difflen);
        header.header_line = ntohs(header.header_line);
        printf("hedaer : %d\n", GET_CODEREQ(&header));
    }

    
    close(pl->socket_tcp);
    close(pl->socket_multidiff);
    close(pl->socket_udp);
    free(pl);

    return 0;
}
