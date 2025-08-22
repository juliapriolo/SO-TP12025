// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#define _POSIX_C_SOURCE 200809L
#include "shm.h"
#include "state.h"
#include "sync.h"
#include <fcntl.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sync_reader.h>


// Direcciones (0..7): 0=arriba y sentido horario
static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};

static int find_self_index(GameState *st) {
	pid_t me = getpid();
	int idx = -1;
	for (unsigned i = 0; i < st->player_count && i < 9; ++i) {
		if (st->players[i].pid == me) {
			idx = (int) i;
			break;
		}
	}
	return idx;
}

// Devuelve una dirección 0..7 si encuentra una celda libre (>0), o -1 si ninguna
static int pick_direction(GameState *st, unsigned short x, unsigned short y) {
	for (int dir = 0; dir < 8; ++dir) {
		int nx = (int) x + dx[dir];
		int ny = (int) y + dy[dir];
		if (nx < 0 || ny < 0 || nx >= st->width || ny >= st->height)
			continue;
		int32_t cell = st->board[ny * st->width + nx];
		if (cell > 0)
			return dir; // libre: recompensa 1..9
	}
	return -1;
}

int main(int argc, char *argv[]) {
	if (argc < 3) {
		fprintf(stderr, "Uso: %s <width> <height>\n", argv[0]);
		return 1;
	}
	unsigned w = (unsigned) atoi(argv[1]);
	unsigned h = (unsigned) atoi(argv[2]);
	if (w <= 0 || h <= 0) {
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

	// Encontrar mi índice i en players[] comparando pid
	int self = -1;
	reader_enter(sync);
	self = find_self_index(state);
	bool fin = state->finished;
	reader_exit(sync);
	if (self < 0) {
		fprintf(stderr, "player: no encuentro mi índice en players[].\n");
		shm_unmap(sync, sizeof(GameSync));
		shm_unmap(state, state_bytes);
		return 1;
	}

	// Bucle principal: esperar G[self], leer estado de forma sincronizada, elegir dirección, escribir 1 byte a fd=1
	while (!fin) {
		if (sem_wait(&sync->sem_player_can_send[self]) == -1) {
			perror("sem_wait(G[i])");
			break;
		}

		// Si el juego terminó, no enviar nada
		reader_enter(sync);
		fin = state->finished;
		unsigned short x = state->players[self].x;
		unsigned short y = state->players[self].y;
		int dir = -1;
		if (!fin)
			dir = pick_direction(state, x, y);
		reader_exit(sync);

		if (fin)
			break;

		unsigned char move =
			(dir >= 0) ? (unsigned char) dir : 0; // si no hay libres, intentamos 0 (probablemente inválido)
		ssize_t wr = write(1, &move, 1);		  // pipe anónimo: máster asocia fd=1 al extremo de escritura
		if (wr != 1) {
			perror("write(move)");
			break;
		}

		// El máster validará, procesará y, cuando corresponda, volverá a habilitar con G[self]
	}

	// Cerrar limpio: no mandamos nada más; el máster verá EOF si el proceso termina (queda "bloqueado")
	shm_unmap(sync, sizeof(GameSync));
	shm_unmap(state, state_bytes);
	return 0;
}
