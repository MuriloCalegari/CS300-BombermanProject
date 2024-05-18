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
#include <net/if.h>
#include <sys/poll.h>
#include "ncurses/ncurses.h"
#include "client/context.h"
#include "client/network.h"
#include "common/util.h"

char* addr;

#define SIZE_MAX_GRID 2000

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

void print_header(LOG_LEVEL level, MessageHeader* header){
    print_log(level, "MessageHeader: CODEREQ(%d) ID(%d) EQ(%d)\n",
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

    print_log(LOG_INFO, "Starting match with server. Sending header:\n");
    print_header(LOG_INFO, &header);

    if(send(pl->socket_tcp, &header, sizeof(header), 0) == -1){
        perror("start_match, send");
        return -1;
    }

    // wait server response
    NewMatchMessage resp;
    memset(&resp, 0, sizeof(resp));

    print_log(LOG_INFO, "Waiting for server response...\n");
    if(read_loop(pl->socket_tcp, &resp, sizeof(resp), 0) <= 0){
        perror("start_match, recv");
        return -1;
    }

    // codereq check
    resp.header.header_line = ntohs(resp.header.header_line); // convert
    int test = GET_CODEREQ(&resp.header);
    print_log(LOG_DEBUG, "Received codereq %d from server\n", test);
    if(((GET_CODEREQ(&resp.header)) != SERVER_RESPONSE_MATCH_START_4_OPPONENTS && mode == MODE_NO_TEAM) ||
    ((GET_CODEREQ(&resp.header)) != SERVER_RESPONSE_MATCH_START_2_TEAMS && mode == MODE_2_TEAM)) {
        print_log(LOG_ERROR, "start_match, error recv codereq");
        return -1;
    }

    pl->eq = GET_EQ(&resp.header);
    pl->id = GET_ID(&resp.header);
    memset(&pl->adr_udp, 0, sizeof(pl->adr_udp));
    memcpy(&pl->adr_udp, resp.adr_mdiff, sizeof(pl->adr_udp));

    pl->port_multidiff = ntohs(resp.port_mdiff);
    pl->port_udp = ntohs(resp.port_udp);

    //config sock_abonnement
    int ok = 1;
    if(setsockopt(pl->socket_multidiff, SOL_SOCKET, SO_REUSEADDR, &ok, sizeof(ok)) < 0) {
        perror("echec de SO_REUSEADDR");
        close(pl->socket_udp);
        return 1;
    }
    if(setsockopt(pl->socket_multidiff, SOL_SOCKET, SO_REUSEPORT, &ok, sizeof(ok)) < 0) {
        perror("echec de SO_REUSEADDR");
        close(pl->socket_udp);
        return 1;
    }

    /* Initialization of the reception address */
    struct sockaddr_in6 adr;
    memset(&adr, 0, sizeof(adr));
    adr.sin6_family = AF_INET6;
    adr.sin6_addr = in6addr_any;
    adr.sin6_port = htons(pl->port_multidiff);

    print_log(LOG_INFO, "Binding UDP socket to port %d\n", pl->port_multidiff);
    if(bind(pl->socket_multidiff, (struct sockaddr*) &adr, sizeof(adr))) {
        perror("echec de bind");
        close(pl->socket_udp);
        return 1;
    }

    // If address equals localhost and this is a mac then...
    
    int ifindex = -1;
    
    if(strcmp(addr, "localhost") == 0 || strcmp(addr, "::1") == 0) {
        #ifdef TARGET_OS_OSX
        print_log(LOG_INFO, "Running on MacOS with localhost, using if_nametoindex for loopback interface\n");
        ifindex = if_nametoindex ("lo0");
        if(ifindex == 0)
            perror("if_nametoindex");
        #endif
        #ifdef __linux
        print_log(LOG_INFO, "Running on Linux with localhost, using eth0 interface\n");
        ifindex = if_nametoindex ("eth0");
        if(ifindex == 0)
            perror("if_nametoindex");
        #endif
    }

    /* Subscribe to the multicast group */

    struct ipv6_mreq group;
    memcpy(&group.ipv6mr_multiaddr.s6_addr, pl->adr_udp, sizeof(pl->adr_udp));
    if(ifindex == -1)
        group.ipv6mr_interface = 0; // all interfaces
    else
        group.ipv6mr_interface = ifindex; // override

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

int tchat_message(player *pl){
    TChatHeader message;
    memset(&message, 0, sizeof(message));
    char *data = pl->g->lw->data;
    // start message with "/t" for team tchat
    if(data[0] == '/' && data[1] == 't') {
        print_log(LOG_VERBOSE, "Sending team tchat message\n");
        SET_CODEREQ(&message.header, T_CHAT_TEAM);
        data += 2;
    } else {
        print_log(LOG_VERBOSE, "Sending all players tchat message\n");
        SET_CODEREQ(&message.header, T_CHAT_ALL_PLAYERS);
    }
    SET_ID(&message.header, pl->id);
    SET_EQ(&message.header, pl->eq);
    message.header.header_line = htons(message.header.header_line);

    message.data_len = strlen(data);
    char res[message.data_len + 1];
    int i;
    for(i=0; i<message.data_len; i++){
        res[i] = data[i];
    }
    res[i] = 0;
    print_log(LOG_DEBUG, "Sending tchat message: %s\n", res);
    if(write_loop(pl->socket_tcp, &message, sizeof(message), 0) <= 0){
        perror("tchat_message, send");
        return -1;
    }
    if(write_loop(pl->socket_tcp, res, message.data_len, 0) <= 0){
        perror("tchat_message, send");
        return -1;
    }

    pl->g->lw->cursor=0;
    memset(pl->g->lw->data, 0, SIZE_MAX_MESSAGE);
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
    pl->num = (pl->num+1) % NUM_MAX;
    SET_ACTION(&buffer, action);

    buffer.message_header.header_line = htons(buffer.message_header.header_line);
    buffer.action_identifier = htons(buffer.action_identifier);

    struct sockaddr_in6 adr;
    memset(&adr, 0, sizeof(adr));
    adr.sin6_family = AF_INET6;
    inet_pton(AF_INET6, pl->server_adr, &adr.sin6_addr); 
    adr.sin6_port = htons(pl->port_udp);

    int send = sendto(pl->socket_udp, &buffer, sizeof(buffer), 0, (struct sockaddr *)&adr, sizeof(adr));
    if(send < 0){
        perror("sendto fail");
        return -1;
    }

    return 0;
}

void end_game(player *pl) {
    print_log(LOG_INFO, "Ending game...\n");
    pthread_mutex_lock(&pl->mutex);
    if(pl->end == 0) {
        curs_set(1); // Set the cursor to visible again
        endwin(); /* End curses mode */
        pl->end = 1;
        print_log(LOG_DEBUG, "Setting end flag to 1\n");
    }
    pthread_mutex_unlock(&pl->mutex);
}

void *game_control(void *arg){
    player *pl = (player *)arg;
    while(pl->end == 0){
        pthread_mutex_lock(&pl->mutex);
        ACTION a = control(pl->g->lw, pl->tchat_mode);
        pthread_mutex_unlock(&pl->mutex);

        if(a == ENTER){
            if(pl->tchat_mode == 0){
                pl->tchat_mode = 1;
            }else{
                pl->tchat_mode = 0;
            }
            if(pl->g->lw->cursor > 0){
                pthread_mutex_lock(&pl->mutex);
                tchat_message(pl);
                pthread_mutex_unlock(&pl->mutex);
            }
        }
        if(pl->tchat_mode == 0){
            switch(a){
                case QUIT: // quit
                    end_game(pl);
                    pthread_exit(NULL);
                case LEFT: //left
                    udp_message(pl, MOVE_WEST);
                    break;
                case RIGHT: //right
                    udp_message(pl, MOVE_EAST);
                    break;
                case UP: //up
                    udp_message(pl, MOVE_NORTH);
                    break;
                case DOWN: //down
                    udp_message(pl, MOVE_SOUTH);
                    break;
                case BOMB_ACTION:
                    udp_message(pl, DROP_BOMB);
                    break;
                default: break;
            }
        }
    }
    pthread_exit(NULL);
}


void process_tchat_message(player *pl, MessageHeader *header) {
    TChatHeader *resp = malloc(sizeof(TChatHeader));
    memcpy(resp, header, sizeof(TChatHeader));
    read_loop(pl->socket_tcp, &resp->data_len, sizeof(resp->data_len), 0);

    char data[resp->data_len + 1];

    if(read_loop(pl->socket_tcp, &data, resp->data_len, 0) <= 0){
        perror("read_tcp, recv");
    }

    data[resp->data_len] = '\0';

    print_log(LOG_DEBUG, "Received tchat message of length %d: %s\n", resp->data_len, data);

    pthread_mutex_lock(&pl->mutex);
    //update tchat
    switch(pl->g->lr->nb_line){
        case 0:
            memcpy(&pl->g->lr->data[0], data, resp->data_len);
            pl->g->lr->len[0] = resp->data_len;
            pl->g->lr->nb_line++;
            break;
        case 1:
            memcpy(&pl->g->lr->data[1], data, resp->data_len);
            pl->g->lr->len[1] = resp->data_len;
            pl->g->lr->nb_line++;
            break;
        case 2:
            memcpy(&pl->g->lr->data[2], data, resp->data_len);
            pl->g->lr->len[2] = resp->data_len;
            pl->g->lr->nb_line++;
            break;
        default:
            memset(&pl->g->lr->data[0], 0, SIZE_MAX_MESSAGE); // clear source to get a new data
            memmove(&pl->g->lr->data[0], pl->g->lr->data[1], pl->g->lr->len[1]);
            pl->g->lr->len[0] = pl->g->lr->len[1];

            memset(&pl->g->lr->data[1], 0, SIZE_MAX_MESSAGE);
            memmove(&pl->g->lr->data[1], pl->g->lr->data[2], pl->g->lr->len[2]);
            pl->g->lr->len[1] = pl->g->lr->len[2];

            memset(&pl->g->lr->data[2], 0, SIZE_MAX_MESSAGE);
            memcpy(&pl->g->lr->data[2], data, sizeof(data));
            pl->g->lr->len[2] = resp->data_len;
            break;
    }
    pthread_mutex_unlock(&pl->mutex);
    free(resp);
}

void *read_tcp(void* arg){
    player *pl = (player *) arg;

    while(1){
        // Read header first, then process message accordingly

        MessageHeader header;
        memset(&header, 0, sizeof(header));

        if(read_loop(pl->socket_tcp, &header, sizeof(header), 0) <= 0){
            perror("read_tcp, recv");
        }

        header.header_line = ntohs(header.header_line);

        print_log(LOG_DEBUG, "Received header on TCP:\n");
        print_header(LOG_DEBUG, &header);

        switch((GET_CODEREQ(&header))) {
            case SERVER_TCHAT_SENT_ALL_PLAYERS:
            case SERVER_TCHAT_SENT_TEAM:
                print_log(LOG_VERBOSE, "Header is a tchat message\n");
                process_tchat_message(pl, &header);
                break;
            case SERVER_RESPONSE_MATCH_END_4_OPPONENTS:
            case SERVER_RESPONSE_MATCH_END_2_TEAMS:
                print_log(LOG_VERBOSE, "Header is a match end message\n");
                end_game(pl);
                break;
            default:
                print_log(LOG_WARNING, "Received unknown message with codereq %d\n", GET_CODEREQ(&header));
        }
    }

    pthread_exit(NULL);
}

void print_board(LOG_LEVEL level, board* b) {
    // Print board to stderr
    for(int i = 0; i < b->h; i++) {
        for(int j = 0; j < b->w; j++) {
            print_log_prefixed(level, 0, "%d ", get_grid(b, j, i));
        }
        print_log_prefixed(level, 0, "\n");
    }
}

void refresh_gameboard_implementation(player *pl) {
    int buf_size = sizeof(MatchFullUpdateHeader) + SIZE_MAX_GRID*3;
    print_log(LOG_DEBUG, "Mallocing buffer of size %d for UDP gameboard updates\n", buf_size);
    char *buf = malloc(buf_size);
    MessageHeader *header;
    while(pl->end == 0){

        // Use poll() to poll socket_multidiff with a timeout of UDP_TIMEOUT
        struct pollfd fds[1];
        fds[0].fd = pl->socket_multidiff;
        fds[0].events = POLLIN;
        int ret = poll_loop(fds, 1, UDP_TIMEOUT_SECONDS * 1000);

        if (ret == -1) {
            perror("poll");
            break;
        } else if (ret == 0) {
            continue;
        }

        recvfrom(pl->socket_multidiff, buf, buf_size, 0, NULL, 0);
        header = (MessageHeader *) buf;
        header->header_line = ntohs(header->header_line);
        print_log(LOG_VERBOSE, "Received header on UDP:\n");
        print_header(LOG_VERBOSE, header);

        if((GET_CODEREQ(header)) == SERVER_FULL_MATCH_STATUS){

            MatchFullUpdateHeader *mfuh = (MatchFullUpdateHeader *) buf;
            int columns = mfuh->height;
            int lines = mfuh->width;

            if(pl->g->init == 0){ //init grid
                setup_board(pl->g->b, lines, columns);
                pl->g->init = 1;
            }

            mfuh->num = ntohs(mfuh->num);
            print_log(LOG_VERBOSE, "Received full match status with height %d and width %d\n", mfuh->height, mfuh->width);
            memcpy(pl->g->b->grid, buf + sizeof(MatchFullUpdateHeader), columns * lines * sizeof(uint8_t));
            print_board(LOG_VERBOSE, pl->g->b);

            pthread_mutex_lock(&pl->mutex);
            refresh_game(pl->g->b, pl->g->lw, pl->g->lr);
            pthread_mutex_unlock(&pl->mutex);
        } else if((GET_CODEREQ(header)) == SERVER_PARTIAL_MATCH_UPDATE){

            // Ignore partial updates before we get the first full grid update
            if(pl->g->init == 0) break;

            MatchUpdateHeader *muh = (MatchUpdateHeader *) buf;
            muh->num = ntohs(muh->num);
            int count = muh->count;

            print_log(LOG_VERBOSE, "Received a partial update with %d cells\n", count);

            CellStatusUpdate *current_cell = (CellStatusUpdate *)(buf + sizeof(MatchUpdateHeader));

            for(int i=0; i<count; i++){
                set_grid(pl->g->b, current_cell->col, current_cell->row, current_cell->status);
                current_cell++;
            }

            pthread_mutex_lock(&pl->mutex);
            refresh_game(pl->g->b, pl->g->lw, pl->g->lr);
            pthread_mutex_unlock(&pl->mutex);
        }
    }

    free(buf);
}

int main(int argc, char** args){
    player *pl = malloc(sizeof(player));
    pl->num = 0; // number of action
    pl->end = 0;
    pl->tchat_mode = 0;
    pthread_mutex_init(&pl->mutex, 0);

    if(argc < 4){
        print_log(LOG_ERROR, "usage: %s <port> <address> <1:no team, 2:2team> [OPTIONS]\n", args[0]);
        print_log(LOG_ERROR, "OPTIONS:\n");
        print_log(LOG_ERROR, "--debug: print debug messages to stderr.\n");
        return 1;
    }

    if(argc == 4 || (argc >= 5 && strcmp(args[4], "--debug") != 0)) {
        freopen("/dev/null", "w", stderr);
    } else if (argc >= 5 && strcmp(args[4], "--debug") == 0) {
        // Open (or create) new file on current directory named arg[0]_debug:
        // This will be used to store debug messages.
        connect_stderr_to_debug_file(args[0]);
    }

    int port = atoi(args[1]);
    addr = args[2];
    pl->server_adr = args[2];
    if((pl->socket_tcp = connect_to_server(port, addr)) < 0){
        print_log(LOG_ERROR, "Connecting to server failed. Exiting...");
        return 1;
    }

    print_log(LOG_INFO, "connection successful\n");

    if((pl->socket_multidiff = socket(AF_INET6, SOCK_DGRAM, 0)) < 0){
        perror("socket abonnement");
        return 1;
    }

    if((pl->socket_udp = socket(AF_INET6, SOCK_DGRAM, 0)) < 0){
        perror("socket udp");
        return 1;
    }

    if(start_match(pl, atoi(args[3])) < 0){
        print_log(LOG_ERROR, "start_match failed\n");
        return 1;
    }

    pl->g = create_board();
    pl->read_tcp_thread = malloc(sizeof(pthread_t));
    pl->game_control_thread = malloc(sizeof(pthread_t));

    if(pthread_create(pl->read_tcp_thread, NULL, read_tcp, pl)){
        perror("thread read tchat");
        return 1;
    }
    if(pthread_create(pl->game_control_thread, NULL, game_control, pl)){
        perror("action thread");
        return 1;
    }

    refresh_gameboard_implementation(pl);

    printf("Game has ended. Waiting on other threads to finish...\n");
    pthread_cancel(*pl->read_tcp_thread);

    pthread_join(*pl->read_tcp_thread, NULL);
    pthread_join(*pl->game_control_thread, NULL);
    
    close(pl->socket_tcp);
    close(pl->socket_multidiff);
    close(pl->socket_udp);
    free_gameboard(pl->g);
    free(pl);

    return 0;
}
