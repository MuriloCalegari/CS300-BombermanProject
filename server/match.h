#include "server_utils.h"
#include "../common/messages.h"
#include <unistd.h>

#define GET_CELL(match, i, j) ((match)->grid)[(i) * ((match)->width) + (j)]

void initialize_grid(Match* match);
Match *create_new_match_4_opponents(int client_socket, int current_udp_port, int height, int width, char *multicast_address, int freq);
Match *create_new_match_2_teams(int client_socket, int current_udp_port, int height, int width, char *multicast_address, int freq);
int add_player_to_match_4_opponents(Match *match, int client_socket);
int add_player_to_match_2_teams(Match *match, int client_socket);

void handle_action_message(Match *match, ActionMessage actionMessage);

void process_partial_updates(Match *match);
void send_full_grid_to_all_players(Match *match);

void explode_bombs(Match* match);
void kill_player(Match *match, int player_index);

int should_finish_match(Match* match);

#define NO_WINNER_YET -1
#define ALL_PLAYERS_ARE_DEAD -2
void finish_match(Match* match, int result);