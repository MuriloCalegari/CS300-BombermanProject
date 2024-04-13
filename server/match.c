#include "match.h"
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>

void fill_address(struct sockaddr_storage *dst, int current_udp_port, char *multicast_address) {
  struct sockaddr_in6 addr_ipv6;
  memset(&addr_ipv6, 0, sizeof(addr_ipv6));
  addr_ipv6.sin6_family = AF_INET6;
  addr_ipv6.sin6_port = htons(current_udp_port);

  int s = inet_pton(AF_INET6, multicast_address, &addr_ipv6.sin6_addr);

  if(s <= 0){
    perror("inet_pton");
    exit(1);
  }

  // Copy the address to the sockaddr_storage
  memcpy(dst, &addr_ipv6, sizeof(addr_ipv6));
}

int get_player_initial_position(int id, int width, int height) {
  switch(id) {
    case 0:
      return 0; // Top left corner
    case 1:
      return width - 1; // Top right corner
    case 2:
      return height * width - 1; // Bottom right corner
    case 3:
      return height * width - width; // Bottom left corner
    default:
      printf("Invalid player id: %d\n", id);
      exit(-1);
  }
}

int get_player_team(int initial_position, int width, int height) {
  if(initial_position == 0 || initial_position == height * width - 1) {
    return 0;
  } else {
    return 1;
  }
}

Match *create_new_match_4_opponents(int client_socket, int current_udp_port, int height, int width, char *multicast_address) {
  Match *new_match = malloc(sizeof(Match));
  memset(new_match, 0, sizeof(Match));

  int player_id = 0;

  new_match->mode = FOUR_OPPONENTS_MODE;
  new_match->players_count = 1;
  new_match->players[player_id] = 0;
  new_match->sockets_tcp[player_id] = client_socket;

  // TODO set port to listen
  new_match->multicast_port = current_udp_port;
  current_udp_port++;

  fill_address(&new_match->multicast_addr, current_udp_port, multicast_address);

  new_match->height = height;
  new_match->width = width;
  new_match->grid = malloc(height * width * sizeof(uint8_t));

  // Put the player on the grid
  new_match->grid[get_player_initial_position(player_id, width, height)] = ENCODE_PLAYER(player_id);

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
  match->grid[get_player_initial_position(current_player_id, match->width, match->height)] = ENCODE_PLAYER(current_player_id);

  pthread_mutex_unlock(&match->mutex);

  return current_player_id;
}

Match *create_new_match_2_teams(int client_socket, int current_udp_port, int height, int width, char *multicast_address) {
  Match *new_match = malloc(sizeof(Match));
  memset(new_match, 0, sizeof(Match));

  int player_id = 0;

  new_match->mode = FOUR_OPPONENTS_MODE;
  new_match->players_count = 1;
  new_match->players[player_id] = 0;
  new_match->sockets_tcp[player_id] = client_socket;

  // TODO set port to listen
  new_match->multicast_port = current_udp_port;
  current_udp_port++;

  fill_address(&new_match->multicast_addr, current_udp_port, multicast_address);

  new_match->height = height;
  new_match->width = width;
  new_match->grid = malloc(height * width * sizeof(uint8_t));

  // Put the player on the grid
  int initial_position = get_player_initial_position(player_id, width, height);
  new_match->grid[initial_position] = ENCODE_PLAYER(player_id);
  new_match->players_team[player_id] = get_player_team(initial_position, width, height);

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
  int initial_position = get_player_initial_position(current_player_id, match->width, match->height);
  match->grid[initial_position] = ENCODE_PLAYER(current_player_id);
  match->players_team[current_player_id] = get_player_team(initial_position, match->width, match->height);

  pthread_mutex_unlock(&match->mutex);

  return current_player_id;
}

void initialize_grid(Match* match) {
    int grid_size = match->height * match->width;

    memset(match->grid, EMPTY_CELL, grid_size * sizeof(*match->grid));

    // TODO place destructible and undestructible walls on the grid
    for(int i = 1; i < match->height - 1; i++){
      if(i % 2 == 0)continue;
      for(int j = 1; j  < match->width - 1; j++){
        if(j % 2 == 1){
          match->grid[i * match->width + j] = INDESTRUCTIBLE_WALL;
        }
      }
    }
    srand(time(NULL));
    int a_place = grid_size/3;
    int nb = 0;
    while(nb < a_place){
      int i = rand() % (match->height - 2) + 1; 
      int j = rand() % (match->width - 2) + 1;
      if(match->grid[i * match->width + j] == EMPTY_CELL){
        match->grid[i * match->width + j] = DESTRUCTIBLE_WALL;
        nb++;
      }
    }
    
};

void handle_action_message(Match *match, int num, int action) {
    // TODO implement 
}