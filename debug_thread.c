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
#include <pthread.h>
#include "ncurses/ncurses.h"

pthread_mutex_t mutex;
int end;

void *refresh_gameboard(void *arg){
    gameboard *g = (gameboard*)arg;

    while(end == 0){
        test(g);
        pthread_mutex_lock(&mutex);
        refresh_game(g->b, g->lw, g->lr);
        pthread_mutex_unlock(&mutex);
        usleep(3*100000);
    }

    pthread_exit(NULL);
}

void *controle_game(void *arg){
    gameboard *g = (gameboard*)arg;

    while(end == 0){
        ACTION a = control(g->lw);
        pthread_mutex_lock(&mutex);
        switch(perform_action(g->b, g->p, a)){
            case -1: // quit
                curs_set(1); // Set the cursor to visible again
                endwin(); /* End curses mode */
                free_gameboard(g);
                end = 1;
                pthread_exit(NULL);
            case 1:
                //if(g->lw->cursor < 0){
                    g->lw->cursor=0;
                    memset(g->lw->data, 0, SIZE_MAX_MESSAGE);
                //}
                break;
            case 2: //left
                // udp_message(pl, MOVE_WEST);
                break;
            case 3: //right
                // udp_message(pl, MOVE_EAST);
                break;
            case 4: //up
                // udp_message(pl, MOVE_NORTH);
                break;
            case 5: //down
                // udp_message(pl, MOVE_SOUTH);
                break;
            case 6: //bomb
            // TODO
            default: break;
        }
        pthread_mutex_unlock(&mutex);
    }

    pthread_exit(NULL);
}

int main(){

    gameboard *g;
    pthread_mutex_init(&mutex, 0);
    end = 0;

    pthread_t action;
    pthread_t refresh_party;
    g = create_board();

    
    if(pthread_create(&refresh_party, NULL, refresh_gameboard, (void *)g)){
        perror("thread refresh party");
        return 1;
    }

    if(pthread_create(&refresh_party, NULL, controle_game, (void *)g)){
        perror("thread refresh party");
        return 1;
    }

    pthread_join(refresh_party, NULL);
    pthread_join(action, NULL);

    curs_set(1); // Set the cursor to visible again
    endwin();

    return 0;
}