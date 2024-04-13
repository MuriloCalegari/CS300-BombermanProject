/*
These are all util structs to keep track of a match context.
Used by the server to save information on players,
socket information, current game status, etc.
*/

#include <stdint.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define FOUR_OPPONENTS_MODE 0
#define TEAM_MODE 1

#define MAX_PLAYERS_PER_MATCH 4

typedef struct Match {
    uint8_t mode; // FOUR_OPPONENTS or TEAM_MODE
    uint8_t players_count; // How many players are currently on this match
    
    /* The following are in order for 1st player, 2nd player, etc. */
    uint8_t players[MAX_PLAYERS_PER_MATCH]; // Players' IDs
    uint8_t players_team[MAX_PLAYERS_PER_MATCH]; // 0 or 1 for each player
    int sockets_tcp[MAX_PLAYERS_PER_MATCH]; // Players' sockets in TCP mode
    uint8_t players_ready_status[MAX_PLAYERS_PER_MATCH];

    struct sockaddr_storage multicast_addr;
    uint8_t multicast_port; // in host endianness

    /* Grid information, where grid is an array such that
        grid[i * width + j] corresponds to the state of the (i, j) cell */
    uint8_t height;
    uint8_t width;
    uint8_t *grid;

    pthread_mutex_t mutex;
} Match;

typedef struct PlayerHandlerThreadContext {
    int player_index;
    Match *match;
} PlayerHandlerThreadContext;