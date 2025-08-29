// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "shm.h"
#include "sync.h"
#include "sync_reader.h"
#include "view_utils.h"
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
	signal(SIGPIPE, SIG_IGN);

	if (argc < 3) {
		fprintf(stderr, "Uso: %s <width> <height>\n", argv[0]);
		return 1;
	}

	unsigned w = (unsigned) atoi(argv[1]);
	unsigned h = (unsigned) atoi(argv[2]);
	if (w == 0 || h == 0) {
		fprintf(stderr, "Width/Height inválidos.\n");
		return 1;
	}

	int ncurses_initialized = 0;
	SCREEN *scr = NULL;
	int headless = 0;

	if (initialize_ncurses(&scr, &ncurses_initialized, &headless) != OK) {
		fprintf(stderr, "Failed to initialize ncurses.\n");
		return 1;
	}

	size_t state_bytes = gamestate_bytes((uint16_t) w, (uint16_t) h);

	GameState *state = (GameState *) shm_connect("/game_state", state_bytes, O_RDONLY);
	if (!state) {
		fprintf(stderr, "Error: shm_connect(/game_state): %s\n", strerror(errno));
		destroy_ncurses(scr, ncurses_initialized);
		return 1;
	}

	GameSync *sync = (GameSync *) shm_connect("/game_sync", sizeof(GameSync), O_RDWR);
	if (!sync) {
		fprintf(stderr, "Error: shm_connect(/game_sync): %s\n", strerror(errno));
		shm_unmap(state, state_bytes);
		destroy_ncurses(scr, ncurses_initialized);
		return 1;
	}

	/* Buffer de rastro (solo en la view). Guardamos índice de jugador+1 (0=sin rastro). */
	uint8_t *trail = NULL;
	if (!headless) {
		trail = (uint8_t *) calloc((size_t) state->width * (size_t) state->height, 1);
		if (!trail) {
			fprintf(stderr, "Error: sin memoria para trail.\n");
			destroy_ncurses(scr, ncurses_initialized);
			shm_unmap(sync, sizeof(GameSync));
			shm_unmap(state, state_bytes);
			return 1;
		}
	}

	int error = 0;
	int done = 0;

	while (1) {
		if (sem_wait(&sync->sem_master_to_view) == -1) {
			if (errno == EINTR) {
				continue;
			}
			error = 1;
			perror("sem_wait(sem_master_to_view)");
			break;
		}

		if (!headless) {
			clear();
		}

		reader_enter(sync);
		/* Actualizamos rastro: marcamos la celda actual de cada jugador con su índice+1 */

		update_player_trail(state, trail, headless);

		int last_row = 2;
		if (!headless) {
			print_board_flat(state, trail, &last_row);
			last_row = print_players(state, last_row); /* <- ahora devuelve última fila */
		}
		done = state->finished ? 1 : 0;
		reader_exit(sync);

		if (!headless) {
			/* Footer compacto pero informativo - siempre en la última línea disponible */
			int footer_row = (last_row < LINES - 1) ? last_row : LINES - 1;
			mvprintw(footer_row, 0, "=== finished=%s (esperando al master) ===", done ? "true" : "false");
			refresh();
		}

		/* Handshake: devolvemos el turno al master siempre, incluso al final */
		if (sem_post(&sync->sem_view_to_master) == -1) {
			error = 1;
			perror("sem_post(sem_view_to_master)");
			break;
		}

		if (done) {
			break;
		}
	}

	/* ===== Render FINAL  ===== */
	if (!error) {
		/* Última actualización de rastro por si hubo movimiento en el tick final */
		reader_enter(sync);
		update_player_trail(state, trail, headless);
		reader_exit(sync);
	}

	render_final(state, trail, headless);

	destroy_ncurses(scr, ncurses_initialized);
	free(trail);
	shm_unmap(sync, sizeof(GameSync));
	shm_unmap(state, state_bytes);
	return error ? 1 : 0;
}