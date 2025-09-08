#ifndef GAME_H
#define GAME_H

#include "master_utils.h"

// --- board ---
void initial_positions(unsigned w, unsigned h, unsigned n,
                      unsigned short xs[MAX_PLAYERS], unsigned short ys[MAX_PLAYERS]);
void init_board(GameState *st, unsigned w, unsigned h);

// --- moves ---
bool cell_in_bounds(unsigned w, unsigned h, int nx, int ny);
bool player_can_move(const GameState *st, unsigned player_idx);
unsigned count_players_that_can_move(const GameState *st);
bool apply_move_locked(GameState *st, unsigned player_idx, unsigned char move);

// --- podium ---
int compare_players_for_podium(const void *a, const void *b);
void print_podium(const GameState *state);

#endif // GAME_H
