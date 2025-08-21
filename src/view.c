#define _POSIX_C_SOURCE 200809L
#include "shm.h"
#include "state.h"
#include "sync.h"
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_board_flat(const GameState *s) {
	printf("Tablero (%ux%u):\n", s->width, s->height);
	for (unsigned int y = 0; y < s->height; ++y) {
		for (unsigned int x = 0; x < s->width; ++x) {
			int32_t cell = s->board[y * s->width + x];
			printf("%2d ", cell);
		}
		printf("\n");
	}
}

static void print_players(const GameState *s) {
	printf("Jugadores (%u):\n", s->player_count);
	for (unsigned int i = 0; i < s->player_count && i < 9; ++i) {
		const PlayerInfo *p = &s->players[i];
		printf("  [%u] %-16s  score=%u  valid=%u  invalid=%u  pos=(%u,%u)  %s\n", i, p->name, p->score, p->valid_moves,
			   p->invalid_moves, p->x, p->y, p->blocked ? "BLOCKED" : "OK");
	}
}

int main(int argc, char *argv[]) {
	if (argc < 3) {
		fprintf(stderr, "Uso: %s <width> <height>\n", argv[0]);
		return 1;
	}

	unsigned w = (unsigned) atoi(argv[1]);
	unsigned h = (unsigned) atoi(argv[2]);
	if (w <= 0 || h <= 0) {
		fprintf(stderr, "Width/Height inválidos.\n");
		return 1;
	}

	// Tamaño correcto del struct flexible
	size_t state_bytes = gamestate_bytes((uint16_t) w, (uint16_t) h);

	// Conectarse a ambas SHM
	GameState *state = (GameState *) shm_connect("/game_state", state_bytes, O_RDONLY);
	if (!state) {
		fprintf(stderr, "Error: shm_connect(/game_state): %s\n", strerror(errno));
		return 1;
	}

	GameSync *sync = (GameSync *) shm_connect("/game_sync", sizeof(GameSync), O_RDWR);
	if (!sync) {
		fprintf(stderr, "Error: shm_connect(/game_sync): %s\n", strerror(errno));
		shm_unmap(state, state_bytes);
		return 1;
	}

	while (!state->finished) {
		if (sem_wait(&sync->sem_master_to_view) == -1) {
			if (errno == EINTR)
				continue;
			perror("sem_wait(sem_master_to_view)");
			break;
		}

		printf("========================================\n");
		print_board_flat(state);
		print_players(state);
		printf("finished=%s\n", state->finished ? "true" : "false");
		fflush(stdout);

		if (sem_post(&sync->sem_view_to_master) == -1) {
			perror("sem_post(sem_view_to_master)");
			break;
		}
	}

	shm_unmap(sync, sizeof(GameSync));
	shm_unmap(state, state_bytes);
	return 0;
}
