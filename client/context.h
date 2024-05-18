#include <stdint.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <pthread.h>

#define MODE_NO_TEAM 1 
#define MODE_2_TEAM 2

#define UDP_TIMEOUT_SECONDS 2

// When to timeout the read of a message from the TCP socket
// so we can check if
#define TCP_SOCKET_REFRESH_INTERVAL_SECONDS 5

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
    int tchat_mode;
    int end;

    pthread_t *read_tcp_thread;
    pthread_t *game_control_thread;
} player;