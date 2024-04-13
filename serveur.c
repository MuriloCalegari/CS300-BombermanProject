#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>
#include "server/network.h"
#include "server/match.h"
#include "common/util.h"

#define HEIGHT 16
#define WIDTH 16
#define PLAYERS_PER_MATCH 4
#define MULTICAST_ADDRESS "ff12::1"

int current_udp_port = 1024; // Used for multicast groups
int server_udp_port;
int server_tcp_port;

void handle_first_tcp_message(int client_socket, MessageHeader message_header, Match **current_4_opponents, Match **current_2_teams);
int should_setup_new_match(Match *match);
int can_start_match(Match *match);
void start_match(Match* match);
void *tcp_player_handler(void *arg);
void *match_handler(void *arg);

/*
  The main thread is responsible for taking in new gamers in and setting up their matches,
  while matches themselves are executed in separate threads.
*/

int main(int argc, char** args) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <tcp_port> <udp_port>\n", args[0]);
    exit(1);
  }

  server_tcp_port = atoi(args[1]);
  server_udp_port = atoi(args[2]);
  int sock = prepare_socket_and_listen(server_tcp_port);

  Match *current_match_4_opponents = NULL;
  Match *current_match_2_teams = NULL;

  while(1){
    printf("Waiting for next player to connect...\n");

    int client_socket = wait_for_next_player(sock);
    printf("Player connected. Reading first message from player\n");

    MessageHeader message_header;
    
    read_loop(client_socket, &message_header, sizeof(MessageHeader), 0);
    handle_first_tcp_message(client_socket, message_header, &current_match_4_opponents, &current_match_2_teams);

    if(should_setup_new_match(current_match_4_opponents)) {
      current_match_4_opponents = NULL;
    }

    if(should_setup_new_match(current_match_2_teams)) {
      current_match_2_teams = NULL;
    }
  }

  //*** fermeture socket client ***
  // close(client_socket); // TODO we should clear this at some point, as well as the malloc-ed Match pointers

  //*** fermeture socket serveur ***
  close(sock);
  
  return 0;
}

void start_match(Match* match) {
  printf("Starting match with %d players\n", match->players_count);

  initialize_grid(match);

  MatchHandlerThreadContext *context = malloc(sizeof(MatchHandlerThreadContext));
  context->match = match;

  pthread_t pthread; // TODO Consider storing somewhere
  launch_thread(match_handler, context);
}

/* Used by the main thread to decide if it should setup a new match with no players */
int should_setup_new_match(Match *match) {
  if(match == NULL || match->players_count == PLAYERS_PER_MATCH) {
    return 1;
  }

  return 0;
}

int can_start_match(Match *match) {
  assert(match != NULL);

  if(match->players_count != PLAYERS_PER_MATCH) return 0;

  for(int i = 0; i < PLAYERS_PER_MATCH; i++) {
    if(!match->players_ready_status[i]) return 0;
  }

  return 1;
}

int get_player_initial_position(int id) {
  switch(id) {
    case 0:
      return 0; // Top left corner
    case 1:
      return WIDTH - 1; // Top right corner
    case 2:
      return HEIGHT * WIDTH - 1; // Bottom right corner
    case 3:
      return HEIGHT * WIDTH - WIDTH; // Bottom left corner
    default:
      printf("Invalid player id: %d\n", id);
      exit(-1);
  }
}

int get_player_team(int initial_position) {
  if(initial_position == 0 || initial_position == HEIGHT * WIDTH - 1) {
    return 0;
  } else {
    return 1;
  }
}

void fill_address(struct sockaddr_storage *dst, int current_udp_port) {
  struct sockaddr_in6 addr_ipv6;
  memset(&addr_ipv6, 0, sizeof(addr_ipv6));
  addr_ipv6.sin6_family = AF_INET6;
  addr_ipv6.sin6_port = htons(current_udp_port);

  int s = inet_pton(AF_INET6, MULTICAST_ADDRESS, &addr_ipv6.sin6_addr);

  if(s <= 0){
    perror("inet_pton");
    exit(1);
  }

  // Copy the address to the sockaddr_storage
  memcpy(dst, &addr_ipv6, sizeof(addr_ipv6));
}

Match *create_new_match_4_opponents(int client_socket) {
  Match *new_match = malloc(sizeof(Match));
  memset(new_match, 0, sizeof(Match));

  int player_id = 0;

  new_match->mode = FOUR_OPPONENTS_MODE;
  new_match->players_count = 1;
  new_match->players[player_id] = 0;
  new_match->sockets_tcp[player_id] = client_socket;

  new_match->multicast_port = current_udp_port;
  current_udp_port++;

  fill_address(&new_match->multicast_addr, current_udp_port);

  new_match->height = HEIGHT;
  new_match->width = WIDTH;
  new_match->grid = malloc(HEIGHT * WIDTH * sizeof(uint8_t));

  // Put the player on the grid
  new_match->grid[get_player_initial_position(player_id)] = ENCODE_PLAYER(player_id);

  pthread_mutex_init(&new_match->mutex, 0);
  return new_match;
}

int add_player_to_match_4_opponents(Match *match, int client_socket) {
  pthread_mutex_lock(&match->mutex);

  int current_player_id = match->players_count;

  // Update current match status
  match->players_count++;
  match->players[current_player_id] = current_player_id;
  match->sockets_tcp[current_player_id] = client_socket;

  // Put the player on the grid
  match->grid[get_player_initial_position(current_player_id)] = ENCODE_PLAYER(current_player_id);

  pthread_mutex_unlock(&match->mutex);

  return current_player_id;
}

Match *create_new_match_2_teams(int client_socket) {
  Match *new_match = malloc(sizeof(Match));
  memset(new_match, 0, sizeof(Match));

  int player_id = 0;

  new_match->mode = FOUR_OPPONENTS_MODE;
  new_match->players_count = 1;
  new_match->players[player_id] = 0;
  new_match->sockets_tcp[player_id] = client_socket;

  new_match->multicast_port = current_udp_port;
  current_udp_port++;

  fill_address(&new_match->multicast_addr, current_udp_port);

  new_match->height = HEIGHT;
  new_match->width = WIDTH;
  new_match->grid = malloc(HEIGHT * WIDTH * sizeof(uint8_t));

  // Put the player on the grid
  int initial_position = get_player_initial_position(player_id);
  new_match->grid[initial_position] = ENCODE_PLAYER(player_id);
  new_match->players_team[player_id] = get_player_team(initial_position);

  pthread_mutex_init(&new_match->mutex, 0);
  return new_match;
}

int add_player_to_match_2_teams(Match *match, int client_socket) {
  pthread_mutex_lock(&match->mutex);

  int current_player_id = match->players_count;

  // Update current match status
  match->players_count++;
  match->players[current_player_id] = current_player_id;
  match->sockets_tcp[current_player_id] = client_socket;

  // Put the player on the grid
  int initial_position = get_player_initial_position(current_player_id);
  match->grid[initial_position] = ENCODE_PLAYER(current_player_id);
  match->players_team[current_player_id] = get_player_team(initial_position);

  pthread_mutex_unlock(&match->mutex);

  return current_player_id;
}

void handle_client_ready_to_play(MessageHeader message_header, Match *match) {
  pthread_mutex_lock(&match->mutex);
  /* Find the respective player based on its socket number */
  for(int i = 0; i < match->players_count; i++) {
    if(match->players[i] == GET_ID(&message_header)) {
      match->players_ready_status[i] = 1;
    }
  }
  pthread_mutex_lock(&match->mutex);
}

void send_new_match_info_message(Match *match, int player_index, int mode) {
  NewMatchMessage message;
  memset(&message, 0, sizeof(message));

  if(mode == TEAM_MODE) {
    SET_CODEREQ(&message.header, SERVER_RESPONSE_MATCH_START_2_TEAMS);
  } else if(mode == FOUR_OPPONENTS_MODE) {
    SET_CODEREQ(&message.header, SERVER_RESPONSE_MATCH_START_4_OPPONENTS);
  }

  SET_ID(&message.header, match->players[player_index]);
  SET_EQ(&message.header, match->players_team[player_index]);

  message.port_udp = htons(server_udp_port);
  message.port_mdiff = htons(match->multicast_port);

  struct sockaddr_in6 address;
  memcpy(&address, &match->multicast_addr, sizeof(address));
  memcpy(&message.adr_mdiff, &address.sin6_addr, sizeof(address.sin6_addr));
  
  printf("Sending information to the player about their new match\n");
  write_loop(match->sockets_tcp[player_index], &message, sizeof(message), 0);
}

void launch_tcp_player_handler(Match *match, int player_index) {
  PlayerHandlerThreadContext *context = malloc(sizeof(PlayerHandlerThreadContext));
  context->player_index = player_index;
  context->match = match;

  launch_thread(tcp_player_handler, context);
}

void handle_first_tcp_message(int client_socket, MessageHeader message_header, Match **current_4_opponents, Match **current_2_teams) {
  switch(GET_CODEREQ(&(message_header))) {
    case NEW_MATCH_4_OPPONENTS:
      printf("Player wants to join a match with 4 opponents.");
      int current_player_index;
      if(*current_4_opponents == NULL) {
        printf(" There are no current pending matches, so we'll start one\n");
        Match *new_match = create_new_match_4_opponents(client_socket);
        current_player_index = new_match->players[0];
        *current_4_opponents = new_match;
        launch_tcp_player_handler(new_match, current_player_index);
      } else {
        printf(" There is a pending match, so we'll add this player to it\n");
        current_player_index = add_player_to_match_4_opponents(*current_4_opponents, client_socket);
      }
      send_new_match_info_message(*current_4_opponents, current_player_index, FOUR_OPPONENTS_MODE);
      break;
    case NEW_MATCH_2_TEAMS:
      printf("Player wants to join a match with 2 teams.");
      if(*current_2_teams == NULL) {
        printf(" There are no current pending matches, so we'll start one\n");
        Match *new_match = create_new_match_2_teams(client_socket);
        current_player_index = new_match->players[0];
        *current_2_teams = new_match;
        launch_tcp_player_handler(new_match, current_player_index);
      } else {
        printf(" There is a pending match, so we'll add this player to it\n");
        current_player_index = add_player_to_match_2_teams(*current_2_teams, client_socket);
      }
      send_new_match_info_message(*current_2_teams, current_player_index, TEAM_MODE);
      break;
    default:
      printf("Invalid CODEREQ %d for handle_first_tcp_message context", GET_CODEREQ(&(message_header)));
      exit(-1);
  }
}

void *tcp_player_handler(void *arg) {
  PlayerHandlerThreadContext *context = (PlayerHandlerThreadContext *) arg;
  Match *match = context->match;
  int player_index = context->player_index;

  while(1) {
    MessageHeader message_header;
    read_loop(match->sockets_tcp[player_index], &message_header, sizeof(MessageHeader), 0);

    switch(GET_CODEREQ(&(message_header))) {
      case CLIENT_READY_TO_PLAY_4_OPPONENTS:
      case CLIENT_READY_TO_PLAY_2_TEAMS:
        printf("Player %d is ready to play\n", GET_ID(&message_header));
        handle_client_ready_to_play(message_header, match);

        pthread_mutex_lock(&match->mutex);
        if(can_start_match(match)) {
          start_match(match);
          pthread_mutex_unlock(&match->mutex);
        }
        break;
    }
  }

  return NULL;
}

void *match_handler(void *arg) {
  MatchHandlerThreadContext *context = (MatchHandlerThreadContext *) arg;
  Match *match = context->match;

  while(1) {
  }
}