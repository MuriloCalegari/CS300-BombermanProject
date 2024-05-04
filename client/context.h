#include <stdint.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <pthread.h>

#define MODE_NO_TEAM 1 
#define MODE_2_TEAM 2

typedef struct player {
    int socket_tcp;
    int socket_udp;
    int socket_multidiff;
    int id;
    int eq;
    int num;
    int mode;
    uint8_t adr_udp[16];
    char *server_adr;
    int port_udp;
    int port_multidiff;
    pthread_mutex_t mutex;
    gameboard* g;
    int freq;
    int ready;
    int end;
} player;