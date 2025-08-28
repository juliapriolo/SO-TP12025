#ifndef SO_TP12025_VIEW_UTILS_H
#define SO_TP12025_VIEW_UTILS_H

#define _POSIX_C_SOURCE 200809L

#include "state.h"
#include <ncurses.h>

#define MAX_PLAYERS 9
#define CELL_WIDTH 5
#define CELL_HEIGHT 2
#define CELL_INNER_WIDTH 4

/* Devuelve el índice del jugador que está en (x,y) o -1 si no hay ninguno */
int player_at(const GameState *s, unsigned x, unsigned y);

/* Dibuja el tablero "compacto" y usa 'trail_idx':
 * trail_idx[idx] = 0 -> sin rastro; 1..9 -> (jugador+1) que pasó por ahí */
void print_board_flat(const GameState *s, const uint8_t *trail_idx, int *out_last_row);

/* Devuelve la última fila usada (+1). */
int print_players(const GameState *s, int board_last_row);

/* Devuelve última fila usada. */
int print_final_summary(const GameState *s, int start_row);

#endif // SO_TP12025_VIEW_UTILS_H