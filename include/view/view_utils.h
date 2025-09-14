#ifndef SO_TP12025_VIEW_UTILS_H
#define SO_TP12025_VIEW_UTILS_H

#define _POSIX_C_SOURCE 200809L

#include "state.h"
#include "sync.h"
#include <ncurses.h>

#define CELL_WIDTH 5
#define CELL_HEIGHT 2
#define CELL_INNER_WIDTH 4

int player_at(const GameState *s, unsigned x, unsigned y);

void print_board_flat(const GameState *s, const uint8_t *trail_idx, int *out_last_row);

int print_players(const GameState *s, int board_last_row);

int print_final_summary(const GameState *s, int start_row);

int initialize_ncurses(SCREEN **scr, int *ncurses_initialized, int *headless);

void setup_ncurses_colors(void);

void render_final(const GameState *state, uint8_t *trail, int headless);

void update_player_trail(const GameState *state, uint8_t *trail, int headless);

void destroy_ncurses(SCREEN *scr, int ncurses_initialized);

#endif // SO_TP12025_VIEW_UTILS_H
