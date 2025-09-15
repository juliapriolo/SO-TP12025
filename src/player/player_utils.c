// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#define _POSIX_C_SOURCE 200809L
#include "player_utils.h"
#include "directions.h"
#include "game.h"
#include "state.h"
#include "timing.h"
#include <errno.h>
#include <stdint.h>
#include <unistd.h>

static int count_future_moves(GameState *st, unsigned short x, unsigned short y) {
	static const int dx[] = {0, 1, 1, 1, 0, -1, -1, -1};
	static const int dy[] = {-1, -1, 0, 1, 1, 1, 0, -1};

	int count = 0;
	for (int i = 0; i < 8; ++i) {
		int nx = (int) x + dx[i];
		int ny = (int) y + dy[i];

		// Usar la misma validación que el master
		if (cell_in_bounds(st->width, st->height, nx, ny)) {
			int idx = ny * (int) st->width + nx;
			if (st->board[idx] > 0)
				count++;
		}
	}
	return count;
}

int find_self_index(GameState *st) {
	pid_t me = getpid();
	for (unsigned i = 0; i < st->player_count && i < 9; ++i) {
		if (st->players[i].pid == me)
			return (int) i;
	}
	return -1;
}

int pick_direction(GameState *st, unsigned short x, unsigned short y) {
	static const int dx[] = {0, 1, 1, 1, 0, -1, -1, -1};
	static const int dy[] = {-1, -1, 0, 1, 1, 1, 0, -1};

	int best_dir = -1;
	int best_value = -1;
	int fallback_dir = -1;
	int fallback_value = -1;

	for (int dir = 0; dir < 8; ++dir) {
		int nx = (int) x + dx[dir];
		int ny = (int) y + dy[dir];

		if (!cell_in_bounds(st->width, st->height, nx, ny))
			continue;

		int idx = ny * (int) st->width + nx;

		if (st->board[idx] > 0) {
			if (fallback_dir == -1 || st->board[idx] > fallback_value) {
				fallback_dir = dir;
				fallback_value = st->board[idx];
			}

			int future_moves = count_future_moves(st, (unsigned short) nx, (unsigned short) ny);

			if (future_moves == 0) {
				continue;
			}

			if (st->board[idx] > best_value) {
				best_value = st->board[idx];
				best_dir = dir;
			}
		}
	}

	if (best_dir != -1) {
		return best_dir;
	}

	return fallback_dir;
}