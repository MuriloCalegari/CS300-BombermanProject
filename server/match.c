#include "match.h"

#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../common/util.h"
#include "network.h"
#include <sys/socket.h>
#include <net/if.h>

void fill_address(struct sockaddr_storage *dst, int current_udp_port,
                  char *multicast_address) {
    struct sockaddr_in6 addr_ipv6;
    memset(&addr_ipv6, 0, sizeof(addr_ipv6));
    addr_ipv6.sin6_family = AF_INET6;
    addr_ipv6.sin6_port = htons(current_udp_port);

    int s = inet_pton(AF_INET6, multicast_address, &addr_ipv6.sin6_addr);

#ifdef TARGET_OS_OSX
    printf("Running on MacOS with localhost, using loopback interface\n");
        addr_ipv6.sin6_scope_id = if_nametoindex("lo0");
#endif

    if (s <= 0) {
        perror("inet_pton");
        exit(1);
    }

    // Copy the address to the sockaddr_storage
    memcpy(dst, &addr_ipv6, sizeof(addr_ipv6));
}

int get_player_initial_position(int id, int width, int height) {
    switch (id) {
        case 0:
            return 0;  // Top left corner
        case 1:
            return width - 1;  // Top right corner
        case 2:
            return height * width - 1;  // Bottom right corner
        case 3:
            return height * width - width;  // Bottom left corner
        default:
            printf("Invalid player id: %d\n", id);
            exit(-1);
    }
}

int get_player_team(int initial_position, int width, int height) {
    if (initial_position == 0 || initial_position == height * width - 1) {
        return 0;
    } else {
        return 1;
    }
}

Match *create_new_match(int client_socket, int udp_port, int height, int width,
                        char *multicast_address, int freq, int mode) {
    Match *new_match = malloc(sizeof(Match));
    memset(new_match, 0, sizeof(Match));

    int player_id = 0;

    new_match->mode = mode;
    new_match->players_count = 1;
    new_match->players[player_id] = 0;
    new_match->sockets_tcp[player_id] = client_socket;

    new_match->multicast_port = udp_port;
    new_match->udp_server_port = udp_port + 1;
    new_match->inbound_socket_udp =
            setup_udp_listening_socket(new_match->udp_server_port);
    new_match->outbound_socket_udp = socket(AF_INET6, SOCK_DGRAM, 0);

    fill_address(&new_match->multicast_addr, udp_port, multicast_address);

    new_match->height = height;
    new_match->width = width;
    assert(height > 0 && width > 0);
    new_match->grid = malloc(height * width * sizeof(uint8_t));
    new_match->exploded_walls_bitmap = malloc(height * width * sizeof(uint8_t));
    initialize_grid(new_match);

    // Put the player on the grid
    int initial_position = get_player_initial_position(player_id, width, height);
    new_match->grid[initial_position] = ENCODE_PLAYER(player_id);
    new_match->players_current_position[player_id] = initial_position;

    if (mode == TEAM_MODE) {
        new_match->players_team[player_id] =
                get_player_team(initial_position, width, height);
    }

    new_match->freq = freq;
    pthread_mutex_init(&new_match->mutex, 0);
    return new_match;
}

int add_player_to_match(Match *match, int client_socket, int mode) {
    pthread_mutex_lock(&match->mutex);

    int current_player_id = match->players_count;

    // Update current match status
    match->players_count++;
    match->players[current_player_id] = current_player_id;
    match->sockets_tcp[current_player_id] = client_socket;

    // Put the player on the grid
    int initial_position = get_player_initial_position(
            current_player_id, match->width, match->height);
    match->players_current_position[current_player_id] = initial_position;
    match->grid[initial_position] = ENCODE_PLAYER(current_player_id);

    if(mode == TEAM_MODE){
        match->players_team[current_player_id] =
                get_player_team(initial_position, match->width, match->height);
    }

    pthread_mutex_unlock(&match->mutex);

    return current_player_id;
}

Match *create_new_match_4_opponents(int client_socket, int current_udp_port,
                                    int height, int width,
                                    char *multicast_address, int freq) {
    printf("Creating a new match for 4 opponents\n");
    return create_new_match(client_socket, current_udp_port, height, width,
                            multicast_address, freq, FOUR_OPPONENTS_MODE);
}

int add_player_to_match_4_opponents(Match *match, int client_socket) {
    return add_player_to_match(match, client_socket, FOUR_OPPONENTS_MODE);
}

Match *create_new_match_2_teams(int client_socket, int current_udp_port,
                                int height, int width, char *multicast_address,
                                int freq) {
    printf("Creating a new match for 2 teams\n");
    return create_new_match(client_socket, current_udp_port, height, width,
                            multicast_address, freq, TEAM_MODE);
}

int add_player_to_match_2_teams(Match *match, int client_socket) {
    return add_player_to_match(match, client_socket, TEAM_MODE);
}

void initialize_grid(Match *match) {
    int grid_size = match->height * match->width;

    if(match->grid == NULL) {
        print_log(LOG_ERROR, "Called initialize_grid with NULL grid\n");
        exit(-1);
    }

    memset(match->grid, EMPTY_CELL, grid_size * sizeof(*match->grid));
    memset(match->exploded_walls_bitmap, EMPTY_CELL, grid_size * sizeof(*match->exploded_walls_bitmap));

     for (int i = 1; i < match->height - 1; i++) {
       if (i % 2 == 0) continue;
       for (int j = 1; j < match->width - 1; j++) {
         if (j % 2 == 1) {
           match->grid[i * match->width + j] = INDESTRUCTIBLE_WALL;
         }
       }
     }
     srand(time(NULL));
     int a_place = grid_size / 3;
     int nb = 0;
     while (nb < a_place) {
       int i = rand() % (match->height - 2) + 1;
       int j = rand() % (match->width - 2) + 1;
       if (match->grid[i * match->width + j] == EMPTY_CELL) {
         match->grid[i * match->width + j] = DESTRUCTIBLE_WALL;
         nb++;
       }
     }
};

// We use this constant such that a message with num [0, OVERFLOW_DETECTION]
// is considered greater than a message with num [NUM_MAX - OVERFLOW_DETECTION,
// NUM_MAX]
int has_overflown(int current_num, int new_num) {
    return ((new_num > 0 && new_num < OVERFLOW_DETECTION_BUFFER) &&
            (current_num > NUM_MAX - OVERFLOW_DETECTION_BUFFER));
}

void update_latest_movement(Match *match, int player_index, int num,
                            int action) {
    // Only update the num in the last_actions buffer if the new num is greater
    // than the current one but be aware of the overflow from NUM_MAX to 0
    uint16_t current_num = match->latest_movements[player_index].num;

    if (num == 0 || num > current_num || has_overflown(current_num, num)) {
        VERBOSE_PRINTF(
                "Num is bigger than our previously stored num or it has overflown\n");
        match->latest_movements[player_index].num = num;
        match->latest_movements[player_index].action = action;
        match->latest_movements[player_index].is_pending = 1;
    }
}

void update_latest_bomb_drop(Match *match, int player_index, int num,
                             int action) {
    // Only update the num in the last_actions buffer if the new num is greater
    // than the current one but be aware of the overflow from NUM_MAX to 0
    uint16_t current_num = match->latest_bombs[player_index].num;

    if ((num > current_num) || has_overflown(current_num, num)) {
        DEBUG_PRINTF(
                "Num is bigger than our previously stored num or it has overflown\n");
        match->latest_bombs[player_index].num = num;
        match->latest_bombs[player_index].action = action;
        match->latest_bombs[player_index].is_pending = 1;
    }
}

void handle_action_message(Match *match, ActionMessage actionMessage) {
    int player_id = GET_ID(&actionMessage.message_header);

    int num = GET_NUM(&actionMessage);
    int action = GET_ACTION(&actionMessage);

    // For simplicity, we use the id as the player index
    int player_index = player_id;
    printf("Player %d has sent action %d with num %d\n", player_id, action, num);

    pthread_mutex_lock(&match->mutex);

    switch (action) {
        case MOVE_NORTH:
        case MOVE_EAST:
        case MOVE_SOUTH:
        case MOVE_WEST:
            update_latest_movement(match, player_index, num, action);
            break;
        case DROP_BOMB:
            update_latest_bomb_drop(match, player_index, num, action);
            break;
        case CANCEL_LATEST_MOVE:
            // We cancel the action by setting it as not pending
            match->latest_movements[player_index].is_pending = 0;
            break;
        default:
            printf("Invalid action %d\n", action);
            break;
    }

    pthread_mutex_unlock(&match->mutex);
}

#define INVALID_ACTION -1
#define OUT_OF_BOUNDS -2
#define OCCUPIED_CELL -3
#define SUCCESS 0

int move_player(Match *match, int player_index, int action,
                CellStatusUpdate *result_from, CellStatusUpdate *result_to) {
    int player_position = match->players_current_position[player_index];
    int new_position = -1;

    switch (action) {
        case MOVE_NORTH:
            new_position = player_position - match->width;
            break;
        case MOVE_EAST:
            new_position = player_position + 1;
            break;
        case MOVE_SOUTH:
            new_position = player_position + match->width;
            break;
        case MOVE_WEST:
            new_position = player_position - 1;
            break;
        default:
            printf("Invalid action %d in move_player\n", action);
            return INVALID_ACTION;
    }

    if (new_position < 0 || new_position >= match->height * match->width) {
        printf("Player %d tried to move out of bounds\n", player_index);
        return OUT_OF_BOUNDS;
    }

    if (!(match->grid[new_position] == EMPTY_CELL ||
          match->grid[new_position] == EXPLODED_BY_BOMB)) {
        printf("Player %d tried to move to an occupied cell\n", player_index);
        return OCCUPIED_CELL;
    }

    VERBOSE_PRINTF("Player %d is moving from %d to %d\n", player_index,
                   player_position, new_position);

    // If a player has just dropped a bomb, then he still occupies that one cell
    // before he moves, so we only update that old cell to an EMPTY_CELL if there
    // was no bomb there before
    if (match->grid[player_position] != BOMB) {
        VERBOSE_PRINTF("Old position is not a bomb, setting as EMPTY_CELL\n");
        match->grid[player_position] = match->exploded_walls_bitmap[player_position];
        result_from->status = match->exploded_walls_bitmap[player_position];
    }

    match->grid[new_position] = ENCODE_PLAYER(player_index);
    result_to->status = ENCODE_PLAYER(player_index);

    match->players_current_position[player_index] = new_position;

    int row = player_position / match->width;
    int col = player_position % match->width;

    result_from->row = row;
    result_from->col = col;

    row = new_position / match->width;
    col = new_position % match->width;

    result_to->row = row;
    result_to->col = col;

    return SUCCESS;
}

void drop_bomb(Match *match, int player_index, CellStatusUpdate *result) {
    printf("Dropping bomb for player %d\n", player_index);
    match->grid[match->players_current_position[player_index]] = BOMB;

    int row = match->players_current_position[player_index] / match->width;
    int col = match->players_current_position[player_index] % match->width;

    result->row = row;
    result->col = col;
    result->status = BOMB;

    Bomb *bomb = malloc(sizeof(Bomb));
    bomb->row = row;
    bomb->col = col;
    bomb->player = player_index;
    bomb->seconds_counter = BOMB_TIME_TO_EXPLODE_SECONDS;
    bomb->next = NULL;
    bomb->prev = NULL;

    if (match->bombs_head == NULL) {
        match->bombs_head = bomb;
        match->bombs_tail = bomb;
    } else {
        bomb->prev = match->bombs_tail;
        match->bombs_tail->next = bomb;
        match->bombs_tail = bomb;
    }
}

void send_partial_updates(CellStatusUpdate *movement_updates,
                          int movement_count, CellStatusUpdate *bomb_updates,
                          int bomb_count, Match *match) {
    MessageHeader header = {0};
    SET_CODEREQ(&header, SERVER_PARTIAL_MATCH_UPDATE);
    SET_ID(&header, 0);
    SET_EQ(&header, 0);
    header.header_line = htons(header.header_line);

    MatchUpdateHeader match_update_header;
    memset(&match_update_header, 0, sizeof(match_update_header));
    match_update_header.header = header;
    match_update_header.num = match->partial_update_current_num;
    match->partial_update_current_num++;
    match_update_header.count = movement_count + bomb_count;

    int total_size = sizeof(match_update_header) +
                     (sizeof(CellStatusUpdate) * (movement_count + bomb_count));

    char *message = malloc(total_size);
    memcpy(message, &match_update_header, sizeof(match_update_header));
    memcpy(message + sizeof(match_update_header), movement_updates,
           sizeof(CellStatusUpdate) * movement_count);
    memcpy(message + sizeof(match_update_header) +
           sizeof(CellStatusUpdate) * movement_count,
           bomb_updates, sizeof(CellStatusUpdate) * bomb_count);

    struct sockaddr_in6 address;
    memcpy(&address, &match->multicast_addr, sizeof(address));

    write_loop_udp(match->outbound_socket_udp, message, total_size, &address,
                   sizeof(address));

    free(message);
}

void process_partial_updates(Match *match) {
    int movement_count = 0;
    int bomb_count = 0;

    // Every successful movement will generate 2 updates:
    // one for the cell the player is moving from and one for the cell the player
    // is moving to Every freq time, only one action per player is allowed, so we
    // can have at most 2 * MAX_PLAYERS_PER_MATCH movement updates
    CellStatusUpdate movement_updates[2 * MAX_PLAYERS_PER_MATCH];
    memset(movement_updates, 0, 2 * MAX_PLAYERS_PER_MATCH * sizeof(CellStatusUpdate));

    // Every bomb drop will generate 1 update
    CellStatusUpdate bomb_updates[MAX_PLAYERS_PER_MATCH];
    memset(movement_updates, 0, MAX_PLAYERS_PER_MATCH * sizeof(CellStatusUpdate));

    // From the spec, "le serveur prend en compte un déplacement vers l’ouest avec
    // dépôt de bombe sur la case d’arrivée," so we process the movements first
    // and then the bombs
    pthread_mutex_lock(&match->mutex);
    for (int player = 0; player < match->players_count; player++) {
        if (match->latest_movements[player].is_pending) {
            int result;
            switch (match->latest_movements[player].action) {
                case MOVE_NORTH:
                case MOVE_EAST:
                case MOVE_SOUTH:
                case MOVE_WEST:
                    result =
                            move_player(match, player, match->latest_movements[player].action,
                                        &movement_updates[movement_count],
                                        &movement_updates[movement_count + 1]);
                    if (result == SUCCESS) {
                        movement_count += 2;
                    }
                    match->latest_movements[player].is_pending = 0;
                    break;
                default:
                    printf("Invalid action %d in process_partial_updates\n",
                           match->latest_movements[player].action);
                    break;
            }
        }

        if (match->latest_bombs[player].is_pending) {
            drop_bomb(match, player, &bomb_updates[bomb_count]);
            match->latest_bombs[player].is_pending = 0;
            bomb_count++;
        }
    }
    pthread_mutex_unlock(&match->mutex);
    if(movement_count > 0 || bomb_count > 0) {
        send_partial_updates(movement_updates, movement_count, bomb_updates,
                             bomb_count, match);
    }
}

void send_full_grid_to_all_players(Match *match) {
    for (int i = 0; i < match->players_count; i++) {
        MessageHeader header = {0};
        SET_CODEREQ(&header, SERVER_FULL_MATCH_STATUS);
        SET_ID(&header, 0);
        SET_EQ(&header, 0);
        header.header_line = htons(header.header_line);

        MatchFullUpdateHeader match_update_header;
        memset(&match_update_header, 0, sizeof(match_update_header));
        match_update_header.header = header;
        match_update_header.num = match->full_update_current_num;
        match->full_update_current_num++;
        match_update_header.height = match->height;
        match_update_header.width = match->width;

        char *message = malloc(sizeof(match_update_header) +
                               sizeof(uint8_t) * match->height * match->width);
        memcpy(message, &match_update_header, sizeof(match_update_header));
        memcpy(message + sizeof(match_update_header), match->grid,
               sizeof(uint8_t) * match->height * match->width);

        int total_size = sizeof(match_update_header) +
                         sizeof(uint8_t) * match->height * match->width;

        struct sockaddr_in6 address;
        memcpy(&address, &match->multicast_addr, sizeof(address));

        write_loop_udp(match->outbound_socket_udp, message, total_size, &address,
                       sizeof(address));

        free(message);
    }
}

void kill_or_explode(Match* match, int i, int j) {
    int cell = i * match->width + j;

    // Check if the nearby cell contains a destructible wall
    if (match->grid[cell] == DESTRUCTIBLE_WALL) {
        match->grid[cell] = EXPLODED_BY_BOMB; // Destroy the wall
        match->exploded_walls_bitmap[cell] = EXPLODED_BY_BOMB;
    } else if (match->grid[cell] >= PLAYER_OFFSET) {
        int player = DECODE_PLAYER(match->grid[cell]);
        match->player_status[player] = DEAD;
        match->players_current_position[player] = -1;
        match->grid[cell] = EMPTY_CELL; // Put an empty cell where player was
        print_log(LOG_DEBUG, "Killing player %d\n", player);
    }
}

void explode_bomb(Match* match, int i, int j){
    // Check if the bomb is within the grid boundaries
    if (i < 0 || i >= match->height || j < 0 || j >= match->width) {
        printf("Warning: called explode_bomb on cell out of bounds\n");
        return;
    }

    // Check if the cell contains a bomb
    int cell = i * match->width + j;
    if (match->grid[cell] != BOMB) {
        printf("Warning: called explode_bomb on cell without a bomb in it\n");
        return;
    }

    DEBUG_PRINTF("Exploding bomb at (%d, %d)\n", i, j);

    match->grid[cell] = match->exploded_walls_bitmap[cell]; // Remove the bomb

    // Vertical explosions
    for(int k = -2; k <= 2; k++) {
        int ni = i + k;
        if (ni >= 0 && ni < match->height) {
            kill_or_explode(match, ni, j);
        }
    }

    // Horizontal explosions
    for(int l = -2; l <= 2; l++) {
        int nj = j + l;
        if (nj >= 0 && nj < match->width) {
            kill_or_explode(match, i, nj);
        }
    }

    // Diagonal explosions
    for(int k = -1; k <= 1; k += 2) {
        for(int l = -1; l <= 1; l += 2) {
            int ni = i + k;
            int nj = j + l;
            if (ni >= 0 && ni < match->height && nj >= 0 && nj < match->width) {
                kill_or_explode(match, ni, nj);
            }
        }
    }
}

void explode_bombs(Match *match) {
    pthread_mutex_lock(&match->mutex);
    Bomb *current_bomb = match->bombs_head;

    while (current_bomb != NULL) {
        current_bomb->seconds_counter--;

        if (current_bomb->seconds_counter == 0) {
            explode_bomb(match, current_bomb->row, current_bomb->col);

            // Remove the bomb from the list
            if (current_bomb->prev != NULL && current_bomb->next != NULL) { // Bomb is in the middle
                current_bomb->prev->next = current_bomb->next;
                current_bomb->next->prev = current_bomb->prev;
            } else if (current_bomb->prev == NULL) { // Bomb is at the head
                match->bombs_head = current_bomb->next;
                if (current_bomb->next != NULL) { // Could be null if only element
                    match->bombs_head->prev = NULL;
                }
                if(match->bombs_head == NULL) { // Happens when there was only one bomb in the list
                    match->bombs_tail = NULL;
                }
                if(match->bombs_head != NULL && match->bombs_head->next == NULL) {
                    match->bombs_tail = match->bombs_head;
                }
            } else { // Bomb is at the tail
                match->bombs_tail = current_bomb->prev;
                if (current_bomb->prev != NULL) {
                    match->bombs_tail->next = NULL;
                }
            }

            assert(check_bomb_linked_list_consistency(match));
            Bomb *to_free = current_bomb;
            current_bomb = current_bomb->next;
            free(to_free);
            continue;
        }

        current_bomb = current_bomb->next;
    }
    pthread_mutex_unlock(&match->mutex);
}

int should_finish_match_four_opponents(Match *match) {
    int players_alive = 0;
    int winning_player = -1;

    for(int i = 0; i < match->players_count; i++) {
        if(match->player_status[i] == READY_TO_PLAY) {
            players_alive++;
            winning_player = i;
        }
    }

    // Override for testing
    if(match->players_count == 1) {
        if (players_alive == 0) {
            return ALL_PLAYERS_ARE_DEAD;
        }
        return NO_WINNER_YET;
    }

    if(players_alive > 1) {
        return NO_WINNER_YET;
    }

    if (players_alive == 0) {
        return ALL_PLAYERS_ARE_DEAD;
    }

    return winning_player;
}

int should_finish_match_two_teams(Match *match) {
    int team_0_alive = 0;
    int team_1_alive = 0;

    for(int i = 0; i < match->players_count; i++) {
        if(match->player_status[i] == READY_TO_PLAY) {
            assert(match->players_team[i] == 0 || match->players_team[i] == 1);
            if(match->players_team[i] == 0) {
                team_0_alive++;
            } else {
                team_1_alive++;
            }
        }
    }

    if(team_0_alive == 0 && team_1_alive == 0) {
        return ALL_PLAYERS_ARE_DEAD;
    }

    if(team_0_alive == 0 && team_1_alive > 0) return 1;
    if(team_0_alive > 0 && team_1_alive == 0) return 0;

    return NO_WINNER_YET;
}

/**
 * Determines whether the match should finish or not.
 *
 * @param match The match object.
 * @return The winning player or team, depending on the mode, if there is one.
 *  Otherwise, NO_WINNER_YER or ALL_PLAYERS_ARE_DEAD is returned.
 */
int should_finish_match(Match* match) {
    switch(match->mode) {
        case FOUR_OPPONENTS_MODE:
            return should_finish_match_four_opponents(match);
            break;
        case TEAM_MODE:
            return should_finish_match_two_teams(match);
            break;
        default:
            printf("Invalid mode found in should_finish_match");
            exit(-1);
            break;
    }
};

void free_bombs(Bomb *head) {
    Bomb *current = head;
    while (current != NULL) {
        Bomb *next = current->next;
        free(current);
        current = next;
    }
}

/**
 * Finish the match and send the end game message to all players.
 * Expected to be called within the match updater thread.
 *
 * @param match The match to finish.
 * @param result The result of the match.
 */
void finish_match(Match* match, int result) {
    printf("Finishing match...\n");
    pthread_mutex_lock(&match->mutex);
    match->is_match_finished = 1;

    MessageHeader end_game_message;

    if(match->mode == FOUR_OPPONENTS_MODE) {
        printf("Match has 4 opponents and result is %d\n", result);
        SET_CODEREQ(&end_game_message, SERVER_RESPONSE_MATCH_END_4_OPPONENTS);
        SET_ID(&end_game_message, result);
    } else {
        printf("Match has two teams and result is %d\n", result);
        SET_CODEREQ(&end_game_message, SERVER_RESPONSE_MATCH_START_2_TEAMS);
        SET_EQ(&end_game_message, result);
    }

    end_game_message.header_line = htons(end_game_message.header_line);

    printf("Cancelling all other threads...\n");
    for(int i = 0; i < match->players_count; i++) {
        pthread_cancel(*match->tcp_player_handler_threads[i]);
    }
    pthread_cancel(*match->match_handler_thread);

    pthread_mutex_unlock(&match->mutex);

    printf("Waiting for all other threads to finish...\n");
    pthread_join(*match->match_handler_thread, NULL);
    free(match->match_handler_thread);

    for(int i = 0; i < match->players_count; i++) {
        printf("Waiting for player %d thread to finish...\n", i);
        pthread_join(*match->tcp_player_handler_threads[i], NULL);
        free(match->tcp_player_handler_threads[i]);
        printf("Sending end game message to player %d\n", i);
        write_loop(match->sockets_tcp[i], &end_game_message, sizeof(end_game_message), 0);
        close(match->sockets_tcp[i]);
    }

    printf("Closing UDP socket and freeing Match structure\n");
    close(match->inbound_socket_udp);
    close(match->outbound_socket_udp);
    free(match->grid);
    free(match->exploded_walls_bitmap);
    free_bombs(match->bombs_head);
    pthread_mutex_destroy(&match->mutex);
    free(match);
};