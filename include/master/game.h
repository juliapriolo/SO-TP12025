#ifndef GAME_H
#define GAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/select.h>
#include <sys/time.h>
#include "state.h"
#include "master_utils.h"

typedef struct {
	fd_set rfds;
	int maxfd;
	unsigned alive_count;
} FdSetInfo;

FdSetInfo setup_fd_set(const Master *M);

typedef struct {
	struct timeval tv;
	bool timeout_reached;
} TimeoutInfo;

TimeoutInfo calculate_timeout(uint64_t last_valid_ms, uint64_t timeout_ms);

typedef struct {
	bool game_ended;
	bool move_was_valid;
	uint64_t new_last_valid_ms;
} MoveProcessResult;

MoveProcessResult process_player_move(Master *M, unsigned player_idx);

void initial_positions(unsigned w, unsigned h, unsigned n,
                      unsigned short xs[MAX_PLAYERS], unsigned short ys[MAX_PLAYERS]);
void init_board(GameState *st, unsigned w, unsigned h);

static inline bool cell_in_bounds(unsigned w, unsigned h, int nx, int ny) {
    return nx >= 0 && ny >= 0 && (unsigned)nx < w && (unsigned)ny < h;
}
bool player_can_move(const GameState *st, unsigned player_idx);
unsigned count_players_that_can_move(const GameState *st);
bool apply_move_locked(GameState *st, unsigned player_idx, unsigned char move);

int compare_players_for_podium(const void *a, const void *b);
void print_podium(const GameState *state);

static inline size_t board_idx(const GameState *st, unsigned x, unsigned y) {
    return (size_t)y * st->width + x;
}

#endif // GAME_H
