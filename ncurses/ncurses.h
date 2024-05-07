#include <ncurses.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "../common/messages.h"

#define MAX_VERTICAL_LINE 3

typedef enum ACTION { NONE, UP, DOWN, LEFT, RIGHT, QUIT, ENTER, BOMB_ACTION} ACTION;

typedef enum COLOR_PAIR_INDEXES { DEFAULT_COLOR, TCHAT_COLOR, BOMB_COLOR } COLOR_PAIR_INDEXES;

typedef struct board {
    uint8_t *grid;
    int w;
    int h;
} board;

typedef struct line_r{
    char data[MAX_VERTICAL_LINE][SIZE_MAX_MESSAGE];
    int len[MAX_VERTICAL_LINE];
    int nb_line;
} line_r;

typedef struct line_w {
    char data[SIZE_MAX_MESSAGE];
    int cursor;
} line_w;

typedef struct pos {
    int x;
    int y;
} pos;

typedef struct gameboard{
    board *b;
    line_r *lr;
    line_w *lw;
    pos *p;
    int init;
} gameboard;

void setup_board(board* board, int lines, int columns);
void free_board(board* board);
int get_grid(board* b, int x, int y);
void set_grid(board* b, int x, int y, int v);
void refresh_game(board* b, line_w* lw, line_r* lr);
ACTION control(line_w* l, int mode);
int perform_action(ACTION a);
gameboard* create_board();
void free_gameboard(gameboard *g);
void update_grid(board* b, char *up);
void test(gameboard *g);