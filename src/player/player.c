// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#define _POSIX_C_SOURCE 200809L
#include "player_utils.h"
#include "shm.h"
#include "state.h"
#include "sync.h"
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sync_reader.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
	if (argc < 3) {
		fprintf(stderr, "Uso: %s <width> <height>\n", argv[0]);
		return 1;
	}

	unsigned w = (unsigned) atoi(argv[1]);
	unsigned h = (unsigned) atoi(argv[2]);
	if (w == 0 || h == 0) {
		fprintf(stderr, "Width/Height inválidos\n");
		return 1;
	}

	size_t state_bytes = gamestate_bytes((uint16_t) w, (uint16_t) h);

	// Conectarse a SHM
	GameState *state = (GameState *) shm_connect("/game_state", state_bytes, O_RDONLY);
	if (!state) {
		perror("shm_connect(/game_state)");
		return 1;
	}
	GameSync *sync = (GameSync *) shm_connect("/game_sync", sizeof(GameSync), O_RDWR);
	if (!sync) {
		perror("shm_connect(/game_sync)");
		shm_unmap(state, state_bytes);
		return 1;
	}

	signal(SIGPIPE, SIG_IGN);

	int self = -1;
	bool fin = false;
	reader_enter(sync);
	self = find_self_index(state);
	fin = state->finished;
	reader_exit(sync);
	if (fin) {
		shm_unmap(sync, sizeof(GameSync));
		shm_unmap(state, state_bytes);
		return 0;
	}
	if (self < 0) {
		fprintf(stderr, "player: no encuentro su índice en players[].\n");
		shm_unmap(sync, sizeof(GameSync));
		shm_unmap(state, state_bytes);
		return 1;
	}

	// Bucle principal
	while (!fin) {
		int sem_res;
		do {
			sem_res = sem_wait(&sync->sem_player_can_send[self]);
		} while (sem_res == -1 && errno == EINTR);

		if (sem_res == -1) {
			perror("sem_wait(G[i])");
			break;
		}

		unsigned short x, y;
		int dir = -1;
		reader_enter(sync);
		fin = state->finished;
		x = state->players[self].x;
		y = state->players[self].y;
		if (!fin)
			dir = pick_direction(state, x, y);
		reader_exit(sync);

		if (fin)
			break;

		unsigned char move = (dir >= 0) ? (unsigned char) dir : 0;
		ssize_t wr = write(STDOUT_FILENO, &move, 1);
		if (wr != 1) {
			perror("write(move)");
			break;
		}
	}

	shm_unmap(sync, sizeof(GameSync));
	shm_unmap(state, state_bytes);
	return 0;
}
