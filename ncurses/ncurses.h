#include <ncurses.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define TEXT_SIZE 150
#define MAX_VERTICAL_LINE 3

typedef enum ACTION { NONE, UP, DOWN, LEFT, RIGHT, QUIT, ENTER } ACTION;

typedef struct board {
    char* grid;
    int w;
    int h;
} board;

typedef struct line_r{
    char data[MAX_VERTICAL_LINE][TEXT_SIZE];
    int len[MAX_VERTICAL_LINE];
} line_r;

typedef struct line_w {
    char data[TEXT_SIZE];
    int cursor;
} line_w;

typedef struct pos {
    int x;
    int y;
} pos;

void setup_board(board* board);
void free_board(board* board);
int get_grid(board* b, int x, int y);
void set_grid(board* b, int x, int y, int v);
void refresh_game(board* b, line_w* lw, line_r* lr);
ACTION control(line_w* l);
int perform_action(board* b, pos* p, ACTION a);