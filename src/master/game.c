// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "game.h"
#include "directions.h"

void initial_positions(unsigned w, unsigned h, unsigned n,
					   unsigned short xs[MAX_PLAYERS], unsigned short ys[MAX_PLAYERS]) {
	const int gx[3] = {1, 2, 3};
	const int gy[3] = {1, 2, 3};
	unsigned count = 0;
	for (int ry = 0; ry < 3 && count < n; ++ry) {
		for (int rx = 0; rx < 3 && count < n; ++rx) {
			unsigned short x = (unsigned short) (((unsigned) gx[rx] * (w + 1U)) / 4U);
			unsigned short y = (unsigned short) (((unsigned) gy[ry] * (h + 1U)) / 4U);
			if (x >= w) x = (unsigned short) (w - 1U);
			if (y >= h) y = (unsigned short) (h - 1U);
			xs[count] = x;
			ys[count] = y;
			++count;
		}
	}
}

void init_board(GameState *st, unsigned w, unsigned h) {
    for (unsigned y = 0; y < h; ++y) {
        for (unsigned x = 0; x < w; ++x) {
            int val = (rand() % 9) + 1; /* 1..9 */
            st->board[board_idx(st, x, y)] = val;
        }
    }
}



bool player_can_move(const GameState *st, unsigned player_idx) {
	if (player_idx >= st->player_count)
		return false;
	unsigned short x = st->players[player_idx].x;
	unsigned short y = st->players[player_idx].y;

	for (int dir = 0; dir < 8; ++dir) {
    int nx = (int) x + DX[dir];
    int ny = (int) y + DY[dir];
	if (cell_in_bounds(st->width, st->height, nx, ny)) {
			int32_t cell = st->board[(unsigned) ny * st->width + (unsigned) nx];
			if (cell > 0)
				return true;
		}
	}
	return false;
}

unsigned count_players_that_can_move(const GameState *st) {
	unsigned can_move = 0;
	for (unsigned i = 0; i < st->player_count; ++i) {
		if (!st->players[i].blocked && player_can_move(st, i)) {
			can_move++;
		}
	}
	return can_move;
}

static void invalidate_and_maybe_block(GameState *st, unsigned player_idx) {
	st->players[player_idx].invalid_moves++;
	if (!player_can_move(st, player_idx)) st->players[player_idx].blocked = true;
}

static void update_blocked_after_move(GameState *st, unsigned player_idx) {
	if (!player_can_move(st, player_idx)) st->players[player_idx].blocked = true;
}

bool apply_move_locked(GameState *st, unsigned player_idx, unsigned char move) {
	if (move > 7) {
		invalidate_and_maybe_block(st, player_idx);
		return false;
	}
	int dx = DX[move], dy = DY[move];
	int x = (int) st->players[player_idx].x;
	int y = (int) st->players[player_idx].y;
	int nx = x + dx, ny = y + dy;

	if (!cell_in_bounds(st->width, st->height, nx, ny)) {
		invalidate_and_maybe_block(st, player_idx);
		return false;
	}
	int32_t *cellp = &st->board[board_idx(st, (unsigned)nx, (unsigned)ny)];
	if (*cellp <= 0) {
		invalidate_and_maybe_block(st, player_idx);
		return false;
	}

	st->players[player_idx].score += (unsigned) *cellp;
	st->players[player_idx].valid_moves++;
	*cellp = -(int) player_idx;
	st->players[player_idx].x = (unsigned short) nx;
	st->players[player_idx].y = (unsigned short) ny;

	update_blocked_after_move(st, player_idx);
	return true;
}

int compare_players_for_podium(const void *a, const void *b) {
	const PlayerInfo *pa = (const PlayerInfo *) a;
	const PlayerInfo *pb = (const PlayerInfo *) b;

	if (pa->score > pb->score) return -1;
	if (pa->score < pb->score) return 1;
	if (pa->valid_moves < pb->valid_moves) return -1;
	if (pa->valid_moves > pb->valid_moves) return 1;

	if (pa->invalid_moves < pb->invalid_moves) return -1;
	if (pa->invalid_moves > pb->invalid_moves) return 1;

	return 0;
}

static void copy_players(PlayerInfo *dest, const PlayerInfo *src, unsigned count) {
	for (unsigned i = 0; i < count; ++i) {
		dest[i] = src[i];
	}
}

static void print_podium_header(void) {
	printf("\n");
	printf("=== PODIO FINAL ===\n");
	printf("POS  JUGADOR      PUNTAJE  VALIDOS  INVALIDOS\n");
	printf("==========================================\n");
}

static void print_podium_rows(const PlayerInfo *players, unsigned count) {
	for (unsigned i = 0; i < count; ++i) {
		const char *medal = "   ";
		if (i == 0) medal = "[1]";
		else if (i == 1) medal = "[2]";
		else if (i == 2) medal = "[3]";
		printf("%-3s  %-12s %-8u %-8u %-8u\n", medal, players[i].name, players[i].score,
			   players[i].valid_moves, players[i].invalid_moves);
	}
	printf("==========================================\n");
}

static void print_tie_breakers(void) {
	printf("\nCriterios de desempate:\n");
	printf("1. Mayor puntaje\n");
	printf("2. Menor cantidad de movimientos validos (eficiencia)\n");
	printf("3. Menor cantidad de movimientos invalidos\n");
	printf("4. Empate si todos los criterios son iguales\n");
}

void print_podium(const GameState *state) {
	PlayerInfo players_copy[MAX_PLAYERS];
	copy_players(players_copy, state->players, state->player_count);
	qsort(players_copy, state->player_count, sizeof(PlayerInfo), compare_players_for_podium);
	print_podium_header();
	print_podium_rows(players_copy, state->player_count);

	bool has_ties = false;
	for (unsigned i = 0; i < state->player_count - 1; ++i) {
		if (compare_players_for_podium(&players_copy[i], &players_copy[i + 1]) == 0) {
			has_ties = true;
			break;
		}
	}
	if (has_ties) {
		print_tie_breakers();
	}
	printf("\n");
}
