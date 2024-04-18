#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>
#include <poll.h>
#include <time.h>
#include "server/network.h"
#include "server/match.h"
#include "common/util.h"

#define LONG_FREQ_MS 1000
#define BOMB_TIME_TO_EXPLODE 3000
#define HEIGHT 16
#define WIDTH 16
#define PLAYERS_PER_MATCH 4
#define MULTICAST_ADDRESS "ff12::1"

int current_udp_port = 1024; // Used for multicast groups
int server_tcp_port;
int freq;

void handle_first_tcp_message(int client_socket, MessageHeader message_header, Match **current_4_opponents, Match **current_2_teams);
int should_setup_new_match(Match *match);
int can_start_match(Match *match);
void start_match(Match* match);
void *tcp_player_handler(void *arg);
void *match_handler(void *arg);
void *match_updater_thread_handler(void *arg);

/*
  The main thread is responsible for taking in new gamers in and setting up their matches,
  while matches themselves are executed in separate threads.
*/

int main(int argc, char** args) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <tcp_port> <freq>\n", args[0]);
    exit(1);
  }

  server_tcp_port = atoi(args[1]);
  freq = atoi(args[2]);

  if(freq < 0 || LONG_FREQ_MS % freq != 0) {
	fprintf(stderr, "Invalid freq value: %d\n", freq);
	if(LONG_FREQ_MS % freq != 0) {
		fprintf(stderr, "Freq must divide %d\n", LONG_FREQ_MS);
	}
	exit(1);
  }

  int sock = prepare_socket_and_listen(server_tcp_port);

  Match *current_match_4_opponents = NULL;
  Match *current_match_2_teams = NULL;

  while(1){
    printf("\n--- Waiting for next player to connect... ---\n\n");

    int client_socket = wait_for_next_player(sock);
    printf("Player connected. Reading first message from player\n");

    MessageHeader message_header;
    
    read_loop(client_socket, &message_header, sizeof(MessageHeader), 0);
    handle_first_tcp_message(client_socket, message_header, &current_match_4_opponents, &current_match_2_teams);

    if(should_setup_new_match(current_match_4_opponents)) {
      DEBUG_PRINTF("MAIN: Setting current_match_4_opponents to NULL");
      current_match_4_opponents = NULL;
    }

    if(should_setup_new_match(current_match_2_teams)) {
      DEBUG_PRINTF("MAIN: Setting current_match_2_teams to NULL");
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

  launch_thread(match_handler, context);
  launch_thread(match_updater_thread_handler, context);
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

void handle_client_ready_to_play(MessageHeader message_header, Match *match) {
  pthread_mutex_lock(&match->mutex);
  /* Find the respective player based on its socket number */
  for(int i = 0; i < match->players_count; i++) {
    if(match->players[i] == GET_ID(&message_header)) {
      match->players_ready_status[i] = 1;
    }
  }
  pthread_mutex_unlock(&match->mutex);
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

  message.port_udp = htons(match->udp_server_port);
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
        printf(" There are no current pending matches, so we'll start one on port %d\n", current_udp_port);
        Match *new_match = create_new_match_4_opponents(client_socket, current_udp_port, HEIGHT, WIDTH, MULTICAST_ADDRESS, freq);
        current_udp_port++;
        current_player_index = new_match->players[0];
        *current_4_opponents = new_match;
        launch_tcp_player_handler(new_match, current_player_index);
      } else {
        printf(" There is a pending match, so we'll add this player to it\n");
        current_player_index = add_player_to_match_4_opponents(*current_4_opponents, client_socket);
        launch_tcp_player_handler(*current_4_opponents, current_player_index);
      }
      send_new_match_info_message(*current_4_opponents, current_player_index, FOUR_OPPONENTS_MODE);
      break;
    case NEW_MATCH_2_TEAMS:
      printf("Player wants to join a match with 2 teams.");
      if(*current_2_teams == NULL) {
        printf(" There are no current pending matches, so we'll start one\n");
        Match *new_match = create_new_match_2_teams(client_socket, current_udp_port, HEIGHT, WIDTH, MULTICAST_ADDRESS, freq);
        current_udp_port++;
        current_player_index = new_match->players[0];
        *current_2_teams = new_match;
        launch_tcp_player_handler(new_match, current_player_index);
      } else {
        printf(" There is a pending match, so we'll add this player to it\n");
        current_player_index = add_player_to_match_2_teams(*current_2_teams, client_socket);
        launch_tcp_player_handler(*current_2_teams, current_player_index);
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

  printf("TCP_HANDLER: Thread initialized for player %d, waiting for client to be ready\n", player_index);

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
        }
        pthread_mutex_unlock(&match->mutex);
        break;
    }
  }

  return NULL;
}

void *match_handler(void *arg) {
  MatchHandlerThreadContext *context = (MatchHandlerThreadContext *) arg;
  Match *match = context->match;

  while(1) {
    ActionMessage action_message;

    read_loop(match->udp_server_port, &action_message, sizeof(ActionMessage), 0);

    if((GET_CODEREQ(&action_message.message_header)) == ACTION_MESSAGE_4_OPPONENTS
        || (GET_CODEREQ(&action_message.message_header)) == ACTION_MESSAGE_2_TEAMS) {
          printf("Received invalid CODEREQ %d on UDP port. Ignoring...\n", GET_CODEREQ(&action_message.message_header));
          continue;
    };

    handle_action_message(match, action_message);
  }
}

void *match_updater_thread_handler(void *arg) {
  MatchHandlerThreadContext *context = (MatchHandlerThreadContext *) arg;
  Match *match = context->match;

  int short_update_count_before_full_update = LONG_FREQ_MS / match->freq;

  int elapsed_time_ms = 0;

  while(1) {
    for(int i = 0; i < short_update_count_before_full_update; i++) {
      process_partial_updates(match);
      struct timespec req = {0};
      req.tv_sec = 0;
      req.tv_nsec = match->freq * 1000000;
      nanosleep(&req, (struct timespec *)NULL);
      elapsed_time_ms += match->freq;
    }

    if(elapsed_time_ms >= BOMB_TIME_TO_EXPLODE) {
      explode_bombs(match);
      elapsed_time_ms = 0;
    }

    send_full_grid_to_all_players(match);
  }
}