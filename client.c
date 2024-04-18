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
#include "common/messages.h"
#include "ncurses/ncurses.h"
#include "client/context.h"

int modulo_2_13(int n);

typedef struct player {
    int socket_tcp;
    int socket_udp;
    int id;
    int eq;
    int num;
    int mode;
    uint8_t adr_udp[16];
    int port_udp;
} player;

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

int start_match(player pl, int mode) {
    MessageHeader header;
    memset(&header, 0, sizeof(header));
    if(mode == 1){
        pl.mode = NEW_MATCH_4_OPPONENTS;
    }else{
        pl.mode = NEW_MATCH_2_TEAMS;
    }
    SET_CODEREQ(&header, pl.mode);
    header.header_line = htons(header.header_line);

    printf("\nStarting match with server. Sending header:\n");
    print_header(&header);

    if(send(pl.socket_tcp, &header, sizeof(header), 0) == -1){
        perror("start_match, send");
        return -1;
    }

    // wait server response
    NewMatchMessage resp;
    memset(&resp, 0, sizeof(resp));

    if(recv(pl.socket_tcp, &resp, sizeof(resp), 0) <= 0){
        perror("start_match, recv");
    }
    // codereq check
    if(((GET_CODEREQ(&resp.header)) != SERVER_RESPONSE_MATCH_START_4_OPPONENTS && mode == MODE_NO_TEAM) ||
    ((GET_CODEREQ(&resp.header)) != SERVER_RESPONSE_MATCH_START_2_TEAMS && mode == MODE_2_TEAM)) {
        perror("start_match, error recv codereq");
        return -1;
    }

    pl.eq = GET_EQ(&resp.header);
    pl.id = GET_ID(&resp.header);

    memcpy(&pl.adr_udp, resp.adr_mdiff, sizeof(resp.adr_mdiff));
    pl.port_udp = resp.port_mdiff;

    //config sock_abonnement
    int ok = 1;
    if(setsockopt(pl.socket_udp, SOL_SOCKET, SO_REUSEADDR, &ok, sizeof(ok)) < 0) {
        perror("echec de SO_REUSEADDR");
        close(pl.socket_udp);
        return 1;
    }

    /* Initialization of the reception address */
    struct sockaddr_in6 adr;
    memset(&adr, 0, sizeof(adr));
    adr.sin6_family = AF_INET6;
    adr.sin6_addr = in6addr_any;
    adr.sin6_port = htons(resp.port_udp);

    if(bind(pl.socket_udp, (struct sockaddr*) &adr, sizeof(adr))) {
        perror("echec de bind");
        close(pl.socket_udp);
        return 1;
    }

    int ifindex = if_nametoindex ("eth0");
    if(ifindex == 0)
        perror("if_nametoindex");

    /* subscribe to the multicast group */
    struct ipv6_mreq group;
    memcpy(&group.ipv6mr_multiaddr.s6_addr, resp.adr_mdiff, sizeof(resp.adr_mdiff));
    group.ipv6mr_interface = ifindex;

    if(setsockopt(pl.socket_udp, IPPROTO_IPV6, IPV6_JOIN_GROUP, &group, sizeof group) < 0) {
        perror("echec de abonnement groupe");
        close(pl.socket_udp);
        return 1;
    }

    /* ready */
    memset(&header, 0, sizeof(MessageHeader));
    if(pl.mode == MODE_NO_TEAM){
        SET_EQ(&header, CLIENT_READY_TO_PLAY_4_OPPONENTS);
    }else{
        SET_EQ(&header, CLIENT_READY_TO_PLAY_2_TEAMS);
    }
    SET_ID(&header, pl.id);
    SET_EQ(&header, pl.eq);

    header.header_line = htons(header.header_line);

    if(send(pl.socket_tcp, &header, sizeof(header), 0) == -1){
        perror("start_match, send");
        return -1;
    }

    return 0;
}

int tchat_message(player pl, char *data){
    TChatHeader message;

    if(strcmp(&data[0],"/") == 0 && strcmp(&data[0],"t") == 0 ) { // start message with "/t" for team tchat
        SET_CODEREQ(&message.header, T_CHAT_TEAM);
    }else{
        SET_CODEREQ(&message.header, T_CHAT_ALL_PLAYERS);
    }
    SET_ID(&message.header, pl.id);
    SET_EQ(&message.header, pl.eq);
    message.header.header_line = htons(message.header.header_line);

    message.data_len = strlen(data);
//    message.data = data; TODO FIX

    if(send(pl.socket_tcp, &message, sizeof(message), 0) == -1){
        perror("tchat_message, send");
        return -1;
    }
    return 0;
}

int udp_message(player pl, int action){

    ActionMessage buffer;
    if(pl.mode == MODE_NO_TEAM){
        SET_CODEREQ(&buffer.message_header, ACTION_MESSAGE_4_OPPONENTS);
    }else{
        SET_CODEREQ(&buffer.message_header, ACTION_MESSAGE_2_TEAMS);
    }
    SET_EQ(&buffer.message_header, pl.eq);
    SET_ID(&buffer.message_header, pl.id);
    SET_NUM(&buffer, modulo_2_13(pl.num));
    pl.num = pl.num+1;
    SET_ACTION(&buffer, action);

    buffer.message_header.header_line = htons(buffer.message_header.header_line);

    struct sockaddr_in6 adr;
    memset(&adr, 0, sizeof(adr));
    adr.sin6_family = AF_INET6;
    memcpy(&adr.sin6_addr, pl.adr_udp, sizeof(pl.adr_udp));
    adr.sin6_port = pl.port_udp;

    int send = sendto(pl.socket_udp, &buffer, sizeof(buffer), 0, (struct sockaddr *)&adr, sizeof(adr));
    if(send < 0){
        perror("sendto fail");
        return -1;
    }

    return 0;
}

int tchat(player pl){
    board* b = malloc(sizeof(board));;
    line_r* lr = malloc(sizeof(line_r));
    line_w* lw = malloc(sizeof(line_w));
    lw->cursor = 0;
    pos* p = malloc(sizeof(pos));
    p->x = 0; p->y = 0;

    // NOTE: All ncurses operations (getch, mvaddch, refresh, etc.) must be done on the same thread.
    initscr(); /* Start curses mode */
    raw(); /* Disable line buffering */
    intrflush(stdscr, FALSE); /* No need to flush when intr key is pressed */
    keypad(stdscr, TRUE); /* Required in order to get events from keyboard */
    nodelay(stdscr, TRUE); /* Make getch non-blocking */
    noecho(); /* Don't echo() while we do getch (we will manually print characters when relevant) */
    curs_set(0); // Set the cursor to invisible
    start_color(); // Enable colors
    init_pair(1, COLOR_YELLOW, COLOR_BLACK); // Define a new color style (text is yellow, background is black)

    setup_board(b);
    while (true) {
        ACTION a = control(lw);
        switch(perform_action(b, p, a)){
            case -1: // quit
                free_board(b);
                curs_set(1); // Set the cursor to visible again
                endwin(); /* End curses mode */
                free(p); free(lw); free(lr); free(b);
                return 1;
            case 1:
                if(lw->cursor > 0){
                    tchat_message(pl, lw->data);
                    lw->cursor=0;
                    memset(lw->data, 0, TEXT_SIZE);
                }
                break;
            case 2: //left
                udp_message(pl, 3);
                break;
            case 3: //right
                udp_message(pl, 1);
                break;
            case 4: //up
                udp_message(pl, 0);
                break;
            case 5: //down
                udp_message(pl, 2);
                break;
            case 6: //bomb
            // TODO
            default: break;
        }
        refresh_game(b, lw, lr);
        usleep(30*1000);
    }
    free_board(b);

    curs_set(1); // Set the cursor to visible again
    endwin(); /* End curses mode */

    free(p); free(lw); free(lr); free(b);

    return 0;
}


int main(int argc, char** args){
    player pl;
    pl.num = 0;

    if(argc != 4){
        printf("usage: %s <port> <address> <1:no team, 2:2team>\n", args[0]);
        return 1;
    }

    int port = atoi(args[1]);
    char* addr = args[2];

    if((pl.socket_tcp = connect_to_server(port, addr)) < 0){
        printf("Connecting to server failed. Exiting...");
        return 1;
    }

    printf("connection successful\n");

    if((pl.socket_udp = socket(AF_INET6, SOCK_DGRAM, 0)) < 0){
        perror("socket abonnement");
        return 1;
    }

    if(start_match(pl, atoi(args[3])) < 0){
        printf("start_match failed\n");
        return 1;
    }

    tchat(pl);

    close(pl.socket_tcp);

    return 0;
}

int modulo_2_13(int n) {
    while (n >= (1 << 13)) {
        n -= (1 << 13);
    }
    return n;
}
