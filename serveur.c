#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>
#include <time.h>
#include "server/network.h"
#include "server/match.h"
#include "common/util.h"

#define HEIGHT 15
#define WIDTH 15
#define MULTICAST_ADDRESS "ff02::1"

int current_udp_port = 11000; // Used for multicast groups
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
  if (argc != 3) {
    fprintf(stderr, "Usage: %s <tcp_port> <freq>\n", args[0]);
    exit(1);
  }

  server_tcp_port = atoi(args[1]);
  freq = atoi(args[2]);

  if(freq < 0 || LONG_FREQ_MS % freq != 0 || freq < MIN_SHORT_FREQ) {
    fprintf(stderr, "Invalid freq value: %d\n", freq);
    if(LONG_FREQ_MS % freq != 0) {
      fprintf(stderr, "Freq must divide %d\n", LONG_FREQ_MS);
    }
    if(freq < MIN_SHORT_FREQ) {
      fprintf(stderr, "Freq must be at least %d\n", MIN_SHORT_FREQ);
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
    message_header.header_line = ntohs(message_header.header_line);
    handle_first_tcp_message(client_socket, message_header, &current_match_4_opponents, &current_match_2_teams);

    if(should_setup_new_match(current_match_4_opponents)) {
      DEBUG_PRINTF("MAIN: Setting current_match_4_opponents to NULL\n");
      current_match_4_opponents = NULL;
    }

    if(should_setup_new_match(current_match_2_teams)) {
      DEBUG_PRINTF("MAIN: Setting current_match_2_teams to NULL\n");
      current_match_2_teams = NULL;
    }
  }

  //*** fermeture socket serveur ***
  close(sock);
  
  return 0;
}

void start_match(Match* match) {
  printf("Starting match with %d players\n", match->players_count);

  MatchHandlerThreadContext *context = malloc(sizeof(MatchHandlerThreadContext));
  context->match = match;

  match->match_handler_thread = launch_thread(match_handler, context);
  match->match_updater_thread = launch_thread_with_mode(match_updater_thread_handler, context, PTHREAD_CREATE_DETACHED);
}

/* Used by the main thread to decide if it should set up a new match with no players */
int should_setup_new_match(Match *match) {
  if(match == NULL || match->players_count == MAX_PLAYERS_PER_MATCH) {
    return 1;
  }

  return 0;
}

int can_start_match(Match *match) {
  assert(match != NULL);

  if(match->players_count != MAX_PLAYERS_PER_MATCH) return 0;

  for(int i = 0; i < MAX_PLAYERS_PER_MATCH; i++) {
    if(!match->player_status[i]) return 0;
  }

  return 1;
}

void handle_client_ready_to_play(MessageHeader message_header, Match *match) {
  pthread_mutex_lock(&match->mutex);
  /* Find the respective player based on its socket number */
  for(int i = 0; i < match->players_count; i++) {
    if(match->players[i] == GET_ID(&message_header)) {
      match->player_status[i] = READY_TO_PLAY;
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
  message.header.header_line = htons(message.header.header_line);

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

  match->tcp_player_handler_threads[player_index] = launch_thread(tcp_player_handler, context);
}

void handle_first_tcp_message(int client_socket, MessageHeader message_header, Match **current_4_opponents, Match **current_2_teams) {
  switch(GET_CODEREQ(&(message_header))) {
    case NEW_MATCH_4_OPPONENTS:
      printf("Player wants to join a match with 4 opponents.");
      int current_player_index;
      if(*current_4_opponents == NULL) {
        printf(" There are no current pending matches, so we'll start one on port %d\n", current_udp_port);
        Match *new_match = create_new_match_4_opponents(client_socket, current_udp_port, HEIGHT, WIDTH, MULTICAST_ADDRESS, freq);
        current_udp_port += 2;
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
        current_udp_port += 2;
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

void clean_arg(void *arg) {
  printf("THREAD_HANDLER: Cleaning up thread context\n");
  free(arg);
}

void send_message(Match *match, int player_index, int mode){
  // get_message
  TChatHeader msg;
  uint8_t len;
  read_loop(match->sockets_tcp[player_index], &len, sizeof(uint8_t), 0);
  char data[len];
  read_loop(match->sockets_tcp[player_index], &data, len+1, 0);
  msg.data_len = len;
  // fprintf(stderr, "taille %d,message:%s\n",len,&data[1]);
  SET_ID(&msg.header, match->players[player_index]);

  if(mode == T_CHAT_ALL_PLAYERS){
    for(int i=0; i<match->players_count; i++){
      if(i!=player_index){
        SET_CODEREQ(&msg.header, SERVER_TCHAT_SENT_ALL_PLAYERS);
        msg.header.header_line = htons(msg.header.header_line);

        write_loop(match->sockets_tcp[i], &msg, sizeof(TChatHeader), 0);
        write_loop(match->sockets_tcp[i], &data[1], len, 0);
        fprintf(stderr, "taille %d,message:%s\n",len,&data[1]);
      }
    }
  }else {
    for(int i=0; (i<match->players_count) && (match->players_team[i]==match->players_team[player_index]); i++){
      if(i!=player_index){
        SET_CODEREQ(&msg.header, SERVER_TCHAT_SENT_TEAM);
        SET_EQ(&msg.header, match->players_team[player_index]);
        msg.header.header_line = htons(msg.header.header_line);

        write_loop(match->sockets_tcp[i], &msg, sizeof(TChatHeader), 0);
        write_loop(match->sockets_tcp[i], &data[1], len, 0);
      }
    }
  }
}

void *tcp_player_handler(void *arg) {
  PlayerHandlerThreadContext *context = (PlayerHandlerThreadContext *) arg;
  Match *match = context->match;
  int player_index = context->player_index;

  pthread_cleanup_push(clean_arg, arg);

  printf("TCP_HANDLER: Thread initialized for player %d, waiting for client to be ready\n", player_index);

  while(1) {
    MessageHeader message_header;
    read_loop(match->sockets_tcp[player_index], &message_header, sizeof(MessageHeader), 0);
    message_header.header_line = ntohs(message_header.header_line);

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
      case T_CHAT_ALL_PLAYERS:
        pthread_mutex_lock(&match->mutex); 
        send_message(match, player_index, T_CHAT_ALL_PLAYERS);
        pthread_mutex_unlock(&match->mutex); 
        break;
      case T_CHAT_TEAM:
        pthread_mutex_lock(&match->mutex);
        send_message(match, player_index, T_CHAT_ALL_PLAYERS);
        pthread_mutex_unlock(&match->mutex);
        break;
    }
  }

  pthread_cleanup_pop(0);

  return NULL;
}

void *match_handler(void *arg) {
  MatchHandlerThreadContext *context = (MatchHandlerThreadContext *) arg;
  Match *match = context->match;

  pthread_cleanup_push(clean_arg, arg);

  while(1) {
    ActionMessage action_message;

    read_loop(match->inbound_socket_udp, &action_message, sizeof(ActionMessage), 0);
    action_message.action_identifier = ntohs(action_message.action_identifier);
    action_message.message_header.header_line = ntohs(action_message.message_header.header_line);

    if(!((GET_CODEREQ(&action_message.message_header)) == ACTION_MESSAGE_4_OPPONENTS
        || (GET_CODEREQ(&action_message.message_header)) == ACTION_MESSAGE_2_TEAMS)) {
          printf("Received invalid CODEREQ %d on UDP port. Ignoring...\n", GET_CODEREQ(&action_message.message_header));
          continue;
    }

    handle_action_message(match, action_message);
  }

  pthread_cleanup_pop(0);
}

void *match_updater_thread_handler(void *arg) {
  MatchHandlerThreadContext *context = (MatchHandlerThreadContext *) arg;
  Match *match = context->match;
  pthread_t *thread = match->match_updater_thread;

  pthread_cleanup_push(clean_arg, arg)

  int short_update_count_before_full_update = LONG_FREQ_MS / match->freq;

  while(1) {
    for(int i = 0; i < short_update_count_before_full_update; i++) {
      process_partial_updates(match);
      struct timespec req = {0};
      req.tv_sec = 0;
      req.tv_nsec = match->freq * 1000000;
      nanosleep(&req, (struct timespec *)NULL);
    }

    int result = should_finish_match(match);

    if(result == NO_WINNER_YET) {
      explode_bombs(match);
      send_full_grid_to_all_players(match);
    } else {
      finish_match(match, result);
      break;
    }
  }

  print_log(LOG_DEBUG, "Freeing match_updater_thread\n");
  free(thread);

  pthread_cleanup_pop(0);
  return NULL;
}