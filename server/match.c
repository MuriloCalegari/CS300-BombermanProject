#include "match.h"
#include <string.h>

void initialize_grid(Match* match) {
    int grid_size = match->height * match->width;

    memset(match->grid, EMPTY_CELL, grid_size * sizeof(*match->grid));
};