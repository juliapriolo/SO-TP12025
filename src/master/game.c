// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include "game.h"
#include "sync_writer.h"
#include "notify.h"
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

FdSetInfo setup_fd_set(const Master *M) {
	FdSetInfo info;
	FD_ZERO(&info.rfds);
	info.maxfd = -1;
	info.alive_count = 0;

	for (unsigned i = 0; i < M->args.player_count; ++i) {
		int fd = M->players[i].pipe_rd;
		if (fd >= 0) {
			FD_SET(fd, &info.rfds);
			if (fd > info.maxfd)
				info.maxfd = fd;
			info.alive_count++;
		}
	}

	return info;
}

TimeoutInfo calculate_timeout(uint64_t last_valid_ms, uint64_t timeout_ms) {
	TimeoutInfo info;
	
	struct timeval tv_now;
	gettimeofday(&tv_now, NULL);
	uint64_t now = (uint64_t)tv_now.tv_sec * 1000ULL + (uint64_t)tv_now.tv_usec / 1000ULL;
	uint64_t elapsed = now - last_valid_ms;
	
	if (elapsed >= timeout_ms) {
		info.timeout_reached = true;
		info.tv.tv_sec = 0;
		info.tv.tv_usec = 0;
		return info;
	}
	
	info.timeout_reached = false;
	uint64_t remain_ms = timeout_ms - elapsed;
	info.tv.tv_sec = (time_t) (remain_ms / 1000ULL);
	info.tv.tv_usec = (suseconds_t) ((remain_ms % 1000ULL) * 1000ULL);
	
	return info;
}

MoveProcessResult process_player_move(Master *M, unsigned player_idx) {
    MoveProcessResult result = {0};
    
    int fd = M->players[player_idx].pipe_rd;
    if (fd < 0) {
        result.move_was_valid = false;
        return result;
    }

    unsigned char move;
    ssize_t rd = read(fd, &move, 1);
    if (rd == 0) {
        writer_enter(M->sync);
        M->state->players[player_idx].blocked = true;
        writer_exit(M->sync);
        close(M->players[player_idx].pipe_rd);
        M->players[player_idx].pipe_rd = -1;
        result.move_was_valid = false;
        return result;
    }
    else if (rd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            result.move_was_valid = false;
            return result;
        }
        perror("read(pipe)");
        writer_enter(M->sync);
        M->state->players[player_idx].blocked = true;
        writer_exit(M->sync);
        close(M->players[player_idx].pipe_rd);
        M->players[player_idx].pipe_rd = -1;
        result.move_was_valid = false;
        return result;
    }

    bool was_valid;
    bool all_blocked = false;
    writer_enter(M->sync);
    was_valid = apply_move_locked(M->state, player_idx, move);
    unsigned players_that_can_move = count_players_that_can_move(M->state);
    all_blocked = (players_that_can_move == 0);
    writer_exit(M->sync);

    if (was_valid) {
        notify_view_and_delay_if_any(M);
        struct timeval tv_now;
        gettimeofday(&tv_now, NULL);
        result.new_last_valid_ms = (uint64_t)tv_now.tv_sec * 1000ULL + (uint64_t)tv_now.tv_usec / 1000ULL;
        result.move_was_valid = true;
    } else {
        result.move_was_valid = false;
    }

    if (all_blocked) {
        result.game_ended = true;
        return result;
    }

    allow_next_send(M, player_idx);
    return result;
}