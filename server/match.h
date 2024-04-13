#include "context.h"
#include "../common/messages.h"

void initialize_grid(Match* match);
Match *create_new_match_4_opponents(int client_socket, int current_udp_port, int height, int width, char *multicast_address);
Match *create_new_match_2_teams(int client_socket, int current_udp_port, int height, int width, char *multicast_address);
int add_player_to_match_4_opponents(Match *match, int client_socket);
int add_player_to_match_2_teams(Match *match, int client_socket);

void handle_action_message(Match *match, int num, int action);