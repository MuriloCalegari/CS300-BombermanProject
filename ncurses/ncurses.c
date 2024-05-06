// Build with -lncurses option

#include <ncurses.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "ncurses.h"

gameboard* create_board(){
    gameboard* g = malloc(sizeof(gameboard));
    g->b = malloc(sizeof(board));;
    g->lr = malloc(sizeof(line_r));
    g->lr->nb_line = 0;
    g->lw = malloc(sizeof(line_w));
    g->lw->cursor = 0;
    g->p = malloc(sizeof(pos));
    g->p->x = 0; g->p->y = 0;
    g->init = 0;

    for(int i=0; i < MAX_VERTICAL_LINE; i++){
        g->lr->len[i] = 0;
    }

    // NOTE: All ncurses operations (getch, mvaddch, refresh, etc.) must be done on the same thread.
    initscr(); /* Start curses mode */
    raw(); /* Disable line buffering */
    intrflush(stdscr, FALSE); /* No need to flush when intr key is pressed */
    keypad(stdscr, TRUE); /* Required in order to get events from keyboard */
    nodelay(stdscr, TRUE); /* Make getch non-blocking */
    noecho(); /* Don't echo() while we do getch (we will manually print characters when relevant) */
    curs_set(0); // Set the cursor to invisible
    start_color(); // Enable colors
    init_pair(1, COLOR_YELLOW, COLOR_BLACK); // Define a new color style (text is yellow, background is black)
    return g;
}

void free_gameboard(gameboard *g){
    free_board(g->b);
    free(g->lr);
    free(g->lw);
    free(g->p);
    free(g);
}

void setup_board(board* board, int lines, int columns) {
    board->h = lines + 1 + 3; //  4 (1 tchat write, 3 tchat read) row for chat
    board->w = columns;
    board->grid = malloc(sizeof(uint8_t) * lines * columns);
    for(int i=0; i < (lines*columns); i++){
        board->grid[i] = 0;
    }
}

void free_board(board* board) {
//    free(board->grid); // TODO uncomment when you allocate memory for the grid dynamically (with malloc and varible height and width)
    free(board);
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
    for (y = 0; y < b->h-4; y++) {
        for (x = 0; x < b->w; x++) {
            char c;
            switch (get_grid(b,x,y)) {
                case EMPTY_CELL:
                    c = ' ';
                    break;
                case INDESTRUCTIBLE_WALL:
                    c = '#';
                    break;
                case DESTRUCTIBLE_WALL:
                    c = '+';
                    break;
                case BOMB:
                    c = '*';
                    break;
                case EXPLODED_BY_BOMB:
                    c = 'X';
                    break;
                case 5: // joueur 1
                    c = '1';
                    break;
                case 6: // joueur 2
                    c = '2';
                    break;
                case 7: // joueur 3
                    c = '3';
                    break;
                case 8: // joueur 4
                    c = '4';
                    break;
                default:
                    c = '?';
                    break;
            }
            mvaddch(y,x,c);
        }
    }
    // Update chat text
    attron(COLOR_PAIR(1)); // Enable custom color 1
    attron(A_BOLD); // Enable bold
    // tchat read
    int i = 0;
    for(y = b->h-4; y < b->h-1; y++){
        for(x = 0; x < b->w; x++){
            if(x < lr->len[i]){
                mvaddch(y, x, lr->data[i][x]);
            }else{
                mvaddch(y, x, ' ');
            }
        }
        i++;
    }
    // tchat write
    for (x = 0; x < b->w; x++) {
        if (x >= SIZE_MAX_MESSAGE || x >= lw->cursor)
            mvaddch(b->h-1, x, ' ');
        else
            mvaddch(b->h-1, x, lw->data[x]);
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

int perform_action(ACTION a) {
    int res = 0;
    switch (a) {
        case LEFT:
            res=2; break;
        case RIGHT:
            res=3; break;
        case UP:
            res=4; break;
        case DOWN:
            res=5; break;
        case ENTER:
            return 1;
        case QUIT:
            return -1;
        default: break;
    }
    return res;
}


void test(gameboard *g){
    int up[DIM*DIM];
    for(int i=0; i<DIM; i++){
        up[i] = 1;
    }

    for(int j=DIM; j<DIM; j++){
        srand(time(NULL));
        up[j] = rand()%3;
    }

    for(int i=0; i<DIM; i++){
        up[i*DIM] = 2;
    }

    //g->b->grid = up;
    for(int i=0; i<DIM*DIM; i++){
        g->b->grid[i] = up[i];
    }
}

/*
int main(){
    gameboard *g = create_board();
    strcpy(g->lr->data[0], "bonjour");
    g->lr->len[0] = strlen("bonjour");
    strcpy(g->lr->data[1], "world");
    g->lr->len[1] = strlen("world");
    strcpy(g->lr->data[2], "francois");
    g->lr->len[2] = strlen("francois");
    while(1){
        ACTION a = control(g->lw);
        if(perform_action(g->b, g->p, a) == -1) break;
        test(g);
        refresh_game(g->b, g->lw, g->lr);
        usleep(70*1000);
    }
    curs_set(1); // Set the cursor to visible again
    endwin();
    printf("%s\n", g->lr->data[1]);
    printf("test\n");
    printf("%d\n", g->b->h);
    return 0;
}
*/
