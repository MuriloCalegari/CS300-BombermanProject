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

#define LONG_FREQ_MS 1000
#define MIN_SHORT_FREQ 100

#define BOMB_TIME_TO_EXPLODE_SECONDS 3

// For each LONG_FREQ_MS, a player could but as much as
// LONG_FREQ_MS / MIN_SHORT_FREQ bombs, so a maximum
// of MAX_BOMBS_PER_PLAYER bombs per player can be at the game
// at a given time
#define MAX_BOMBS_PER_PLAYER \
    ((LONG_FREQ_MS / MIN_SHORT_FREQ) * (BOMB_TIME_TO_EXPLODE_SECONDS + 1))

typedef enum PLAYER_STATUS { CONNECTED, READY_TO_PLAY, DEAD } PLAYER_STATUS;

typedef struct ActionBuf {
    uint16_t num;
    uint8_t action;
    uint8_t is_pending;
} ActionBuf;

typedef struct Bomb {
    uint8_t row;
    uint8_t col;
    uint8_t player;
    uint8_t seconds_counter;
    struct Bomb *prev;
    struct Bomb *next;
} Bomb;

typedef struct Match {
    uint8_t mode; // FOUR_OPPONENTS or TEAM_MODE
    uint8_t players_count; // How many players are currently on this match
    
    /* The following are in order for 1st player, 2nd player, etc. */
    uint8_t players[MAX_PLAYERS_PER_MATCH]; // Players' IDs
    uint8_t players_team[MAX_PLAYERS_PER_MATCH]; // 0 or 1 for each player
    int sockets_tcp[MAX_PLAYERS_PER_MATCH]; // Players' sockets in TCP mode
    uint8_t player_status[MAX_PLAYERS_PER_MATCH];

    int players_current_position[MAX_PLAYERS_PER_MATCH]; // Players' current position in the grid

    struct sockaddr_storage multicast_addr;
    uint16_t multicast_port; // in host endianness
    uint16_t udp_server_port; // in host endianness
    int inbound_socket_udp;
    int outbound_socket_udp;

    uint16_t full_update_current_num;
    uint16_t partial_update_current_num;

    /* Grid information, where grid is an array such that
        grid[i * width + j] corresponds to the state of the (i, j) cell */
    uint8_t height;
    uint8_t width;
    uint8_t *grid;

    /* Buffer storing the latest actions that we have received from the players so far */
    ActionBuf latest_movements[MAX_PLAYERS_PER_MATCH];
    ActionBuf latest_bombs[MAX_PLAYERS_PER_MATCH];
    int freq;

    Bomb *bombs_head;
    Bomb *bombs_tail;

    uint8_t is_match_finished;

    pthread_mutex_t mutex;

    pthread_t *match_handler_thread;
    pthread_t *match_updater_thread;
    pthread_t *tcp_player_handler_threads[MAX_PLAYERS_PER_MATCH];
} Match;

typedef struct PlayerHandlerThreadContext {
    int player_index;
    Match *match;
} PlayerHandlerThreadContext;

typedef struct MatchHandlerThreadContext {
    Match *match;
} MatchHandlerThreadContext;