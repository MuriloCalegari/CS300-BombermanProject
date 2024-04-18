// Build with -lncurses option

#include <ncurses.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "ncurses.h"


void setup_board(board* board) {
    int lines; int columns;
    getmaxyx(stdscr,lines,columns);
    board->h = lines - 2 - 1 - 3; // 2 rows reserved for border, 4 (1 tchat write, 3 tchat read) row for chat
    board->w = columns - 2; // 2 columns reserved for border
    board->grid = calloc((board->w)*(board->h),sizeof(char));
}

void free_board(board* board) {
    free(board->grid);
}

int get_grid(board* b, int x, int y) {
    return b->grid[y*b->w + x];
}

void set_grid(board* b, int x, int y, int v) {
    b->grid[y*b->w + x] = v;
}

void refresh_game(board* b, line_w* lw, line_r* lr) {
    // Update grid
    int x,y;
    for (y = 0; y < b->h; y++) {
        for (x = 0; x < b->w; x++) {
            char c;
            switch (get_grid(b,x,y)) {
                case 0:
                    c = ' ';
                    break;
                case 1:
                    c = 'O';
                    break;
                default:
                    c = '?';
                    break;
            }
            mvaddch(y+1,x+1,c);
        }
    }
    for (x = 0; x < b->w+2; x++) {
        mvaddch(0, x, '-');
        mvaddch(b->h+1, x, '-');
    }
    for (y = 0; y < b->h+2; y++) {
        mvaddch(y, 0, '|');
        mvaddch(y, b->w+1, '|');
    }
    // Update chat text
    attron(COLOR_PAIR(1)); // Enable custom color 1
    attron(A_BOLD); // Enable bold
    // tchat read
    for(y = b->h+2; y < b->h+2+3; y++){
        for(x = 0; x < b->w+2; x++){
            if(lr->len[y] < x)
                mvaddch(0, x, lr->data[y][x]);
        }
    }
    // tchat write
    for (x = 0; x < b->w+2; x++) {
        if (x >= SIZE_MAX_MESSAGE || x >= lw->cursor)
            mvaddch(b->h+2+3, x, ' ');
        else
            mvaddch(b->h+2+3, x, lw->data[x]);
    }
    attroff(A_BOLD); // Disable bold
    attroff(COLOR_PAIR(1)); // Disable custom color 1
    refresh(); // Apply the changes to the terminal
}

ACTION control(line_w* l) {
    int c;
    int prev_c = ERR;
    // We consume all similar consecutive key presses
    while ((c = getch()) != ERR) { // getch returns the first key press in the queue
        if (prev_c != ERR && prev_c != c) {
            ungetch(c); // put 'c' back in the queue
            break;
        }
        prev_c = c;
    }
    ACTION a = NONE;
    switch (prev_c) {
        case ERR: break;
        case KEY_LEFT:
            a = LEFT; break;
        case KEY_RIGHT:
            a = RIGHT; break;
        case KEY_UP:
            a = UP; break;
        case KEY_DOWN:
            a = DOWN; break;
        case 27: // ESC
            a = QUIT; break;
        case KEY_BACKSPACE:
            if (l->cursor > 0) l->cursor--;
            break;
        case '\n': 
            a = ENTER; break;
        default:
            if (prev_c >= ' ' && prev_c <= '~' && l->cursor < SIZE_MAX_MESSAGE)
                l->data[(l->cursor)++] = prev_c;
            break;
    }
    return a;
}

int perform_action(board* b, pos* p, ACTION a) {
    int xd = 0;
    int yd = 0;
    int res = 0;
    switch (a) {
        case LEFT:
            xd = -1; yd = 0; res=2; break;
        case RIGHT:
            xd = 1; yd = 0; res=3; break;
        case UP:
            xd = 0; yd = -1; res=4; break;
        case DOWN:
            xd = 0; yd = 1; res=5; break;
        case ENTER:
            return 1;
        case QUIT:
            return -1;
        default: break;
    }
    p->x += xd; p->y += yd;
    p->x = (p->x + b->w)%b->w;
    p->y = (p->y + b->h)%b->h;
    set_grid(b,p->x,p->y,1);
    return res;
}