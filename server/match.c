#include "match.h"

#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../common/util.h"
#include "network.h"

void fill_address(struct sockaddr_storage *dst, int current_udp_port,
                  char *multicast_address) {
  struct sockaddr_in6 addr_ipv6;
  memset(&addr_ipv6, 0, sizeof(addr_ipv6));
  addr_ipv6.sin6_family = AF_INET6;
  addr_ipv6.sin6_port = htons(current_udp_port);

  int s = inet_pton(AF_INET6, multicast_address, &addr_ipv6.sin6_addr);

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
  new_match->udp_server_port = udp_port;
  new_match->socket_udp =
      setup_udp_listening_socket(new_match->udp_server_port);

  fill_address(&new_match->multicast_addr, udp_port, multicast_address);

  new_match->height = height;
  new_match->width = width;
  new_match->grid = malloc(height * width * sizeof(uint8_t));

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

  memset(match->grid, EMPTY_CELL, grid_size * sizeof(*match->grid));

  // TODO place destructible and undestructible walls on the grid
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

  if ((num > current_num) || has_overflown(current_num, num)) {
    printf(
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

  if (match->grid[new_position] != EMPTY_CELL ||
      match->grid[new_position] != EXPLODED_BY_BOMB) {
    printf("Player %d tried to move to an occupied cell\n", player_index);
    return OCCUPIED_CELL;
  }

  // If a player has just dropped a bomb, then he still occupies that one cell
  // before he moves, so we only update that old cell to an EMPTY_CELL if there
  // was no bomb there before
  if (match->grid[player_position] != BOMB) {
    // TODO need to account for EXPLODED_BY_BOMB,
    // probably use a bitmap of walls that have been exploded
    match->grid[player_position] = EMPTY_CELL;
  }
  match->grid[new_position] = ENCODE_PLAYER(player_index);

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
}

void send_partial_updates(CellStatusUpdate *movement_updates,
                          int movement_count, CellStatusUpdate *bomb_updates,
                          int bomb_count, Match *match) {
  MessageHeader header = {0};
  SET_CODEREQ(&header, SERVER_PARTIAL_MATCH_UPDATE);
  SET_ID(&header, 0);
  SET_EQ(&header, 0);

  MatchUpdateHeader match_update_header;
  memset(&match_update_header, 0, sizeof(match_update_header));
  match_update_header.header = header;
  match_update_header.num = match->partial_update_current_num;
  match->partial_update_current_num++;
  match_update_header.count = movement_count + bomb_count;

  char *message =
      malloc(sizeof(match_update_header) +
             sizeof(CellStatusUpdate) * (movement_count + bomb_count));
  memcpy(message, &match_update_header, sizeof(match_update_header));
  memcpy(message + sizeof(match_update_header), movement_updates,
         sizeof(CellStatusUpdate) * movement_count);
  memcpy(message + sizeof(match_update_header) +
             sizeof(CellStatusUpdate) * movement_count,
         bomb_updates, sizeof(CellStatusUpdate) * bomb_count);

  int total_size = sizeof(match_update_header) +
                   (sizeof(CellStatusUpdate) * (movement_count + bomb_count));

  struct sockaddr_in6 address;
  memcpy(&address, &match->multicast_addr, sizeof(address));

  write_loop_udp(match->socket_udp, message, total_size, &address,
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

  // Every bomb drop will generate 1 update
  CellStatusUpdate bomb_updates[MAX_PLAYERS_PER_MATCH];

  // From the spec, "le serveur prend en compte un déplacement vers l’ouest avec
  // dépôt de bombe sur la case d’arrivée," so we process the movements first
  // and then the bombs
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

  send_partial_updates(movement_updates, movement_count, bomb_updates,
                       bomb_count, match);
}

void send_full_grid_to_all_players(Match *match) {
  for (int i = 0; i < match->players_count; i++) {
    MessageHeader header = {0};
    SET_CODEREQ(&header, SERVER_FULL_MATCH_STATUS);
    SET_ID(&header, 0);
    SET_EQ(&header, 0);

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

    write_loop_udp(match->socket_udp, message, total_size, &address,
                  sizeof(address));

    free(message);
  }
}

void explode_bombs(Match *match) { return; }