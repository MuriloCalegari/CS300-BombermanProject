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

void *refresh_gameboard(void *arg){
    gameboard *g = (gameboard*)g;

    while(1){
        //ACTION a = control(g->lw);
        //if(perform_action(g->b, g->p, a) == -1) break;
        test(g);
        refresh_game(g->b, g->lw, g->lr);
        usleep(3*100000);
    }

    pthread_exit(NULL);
}

int main(){

    gameboard *g;
    pthread_mutex_init(&mutex, 0);

    pthread_t refresh_party;
    g = create_board();

    
    if(pthread_create(&refresh_party, NULL, refresh_gameboard, (void *)g)){
        perror("thread refresh party");
        return 1;
    }

    pthread_join(refresh_party, NULL);

    curs_set(1); // Set the cursor to visible again
    endwin();

    return 0;
}