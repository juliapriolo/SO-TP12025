// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#define _POSIX_C_SOURCE 200809L
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
#include <time.h> // nanosleep
#include <unistd.h>

// Direcciones (0..7): 0=arriba y sentido horario
static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};

static void sleep_ms(long ms) {
	struct timespec ts = {.tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000L};
	// Reintentar si una señal interrumpe el sueño
	while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {
	}
}

static int find_self_index(GameState *st) {
	pid_t me = getpid();
	for (unsigned i = 0; i < st->player_count && i < 9; ++i) {
		if (st->players[i].pid == me)
			return (int) i;
	}
	return -1;
}

// Devuelve la mejor dirección hacia una celda libre (>0), o -1 si ninguna
static int pick_direction(GameState *st, unsigned short x, unsigned short y) {
	int best_dir = -1;
	int best_reward = -1;
	for (int dir = 0; dir < 8; ++dir) {
		int nx = (int) x + dx[dir];
		int ny = (int) y + dy[dir];
		if (nx < 0 || ny < 0 || nx >= st->width || ny >= st->height)
			continue;
		int32_t cell = st->board[ny * st->width + nx];
		if (cell > 0 && cell > best_reward) {
			best_reward = cell;
			best_dir = dir;
		}
	}
	return best_dir;
}

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

	// Ignorar SIGPIPE (por si muere el máster antes)
	signal(SIGPIPE, SIG_IGN);

	// Esperar a que aparezca mi índice en players[]
	int self = -1;
	bool fin = false;
	while (self < 0 && !fin) {
		reader_enter(sync);
		self = find_self_index(state);
		fin = state->finished;
		reader_exit(sync);
		if (self < 0 && !fin)
			sleep_ms(1);
	}
	if (fin) {
		shm_unmap(sync, sizeof(GameSync));
		shm_unmap(state, state_bytes);
		return 0;
	}
	if (self < 0) {
		fprintf(stderr, "player: no encuentro mi índice en players[].\n");
		shm_unmap(sync, sizeof(GameSync));
		shm_unmap(state, state_bytes);
		return 1;
	}

	// Bucle principal
	while (!fin) {
		// Esperar turno (G[i]), manejando EINTR
		while (sem_wait(&sync->sem_player_can_send[self]) == -1) {
			if (errno == EINTR)
				continue;
			perror("sem_wait(G[i])");
			goto cleanup;
		}

		// Leer estado de forma sincronizada
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

cleanup:
	shm_unmap(sync, sizeof(GameSync));
	shm_unmap(state, state_bytes);
	return 0;
}
