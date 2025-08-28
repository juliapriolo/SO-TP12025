// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "shm.h"
#include "sync.h"
#include "view_utils.h"
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sync_reader.h>

int main(int argc, char *argv[]) {
	signal(SIGPIPE, SIG_IGN);

	if (getenv("TERM") == NULL)
		setenv("TERM", "xterm-256color", 1);

	int ncurses_initialized = 0;
	SCREEN *scr = NULL;
	int headless = 0;

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

	size_t state_bytes = gamestate_bytes((uint16_t) w, (uint16_t) h);

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

	/* Inicialización ncurses liberable */
	scr = newterm(NULL, stdout, stdin);
	if (!scr) {
		fprintf(stderr, "Advertencia: no pude inicializar ncurses (TERM/terminfo?). "
						"Corriendo en modo headless.\n");
		headless = 1;
	}
	else {
		set_term(scr);
		ncurses_initialized = 1;
		cbreak();
		noecho();
		curs_set(0);
		if (has_colors()) {
			start_color();
			use_default_colors();
			/* Colores base */
			init_pair(1, COLOR_CYAN, -1);  /* títulos */
			init_pair(2, COLOR_WHITE, -1); /* celdas normales (texto) */
			init_pair(4, COLOR_RED, -1);   /* alertas/bloqueado */

			/* Paleta de jugadores (texto para la lista) 10..18 */
			init_pair(10, COLOR_CYAN, -1);
			init_pair(11, COLOR_MAGENTA, -1);
			init_pair(12, COLOR_YELLOW, -1);
			init_pair(13, COLOR_GREEN, -1);
			init_pair(14, COLOR_BLUE, -1);
			init_pair(15, COLOR_WHITE, -1);
			init_pair(16, COLOR_RED, -1);
			if (COLORS >= 256) {
				init_pair(17, 208, -1); /* orange (xterm-256) */
				init_pair(18, 129, -1); /* purple (xterm-256) */
			}
			else if (COLORS >= 16) {
				init_pair(17, 9, -1);  /* bright red */
				init_pair(18, 13, -1); /* bright magenta */
			}
			else {
				init_pair(17, COLOR_BLACK, -1);
				init_pair(18, COLOR_BLACK, -1);
			}

			/* Paleta de jugadores para fondo de casilla 20..28 */
			init_pair(20, -1, COLOR_CYAN);
			init_pair(21, -1, COLOR_MAGENTA);
			init_pair(22, -1, COLOR_YELLOW);
			init_pair(23, -1, COLOR_GREEN);
			init_pair(24, -1, COLOR_BLUE);
			init_pair(25, -1, COLOR_WHITE);
			init_pair(26, -1, COLOR_RED);
			if (COLORS >= 256) {
				init_pair(27, -1, 208); /* orange */
				init_pair(28, -1, 129); /* purple */
			}
			else if (COLORS >= 16) {
				init_pair(27, -1, 9);  /* bright red */
				init_pair(28, -1, 13); /* bright magenta */
			}
			else {
				init_pair(27, -1, COLOR_BLACK);
				init_pair(28, -1, COLOR_BLACK);
			}
		}
	}

	/* Buffer de rastro (solo en la view). Guardamos índice de jugador+1 (0=sin rastro). */
	uint8_t *trail = NULL;
	if (!headless) {
		trail = (uint8_t *) calloc((size_t) state->width * (size_t) state->height, 1);
		if (!trail) {
			fprintf(stderr, "Error: sin memoria para trail.\n");
			if (ncurses_initialized)
				endwin();
			if (scr)
				delscreen(scr);
			shm_unmap(sync, sizeof(GameSync));
			shm_unmap(state, state_bytes);
			return 1;
		}
	}

	int error = 0;
	int done = 0;

	for (;;) {
		if (sem_wait(&sync->sem_master_to_view) == -1) {
			if (errno == EINTR)
				continue;
			error = 1;
			perror("sem_wait(sem_master_to_view)");
			break;
		}

		if (!headless)
			clear();

		reader_enter(sync);
		/* Actualizamos rastro: marcamos la celda actual de cada jugador con su índice+1 */
		if (!headless && has_colors()) {
			for (unsigned i = 0; i < state->player_count && i < MAX_PLAYERS; ++i) {
				const PlayerInfo *p = &state->players[i];
				if (p->x < state->width && p->y < state->height) {
					const size_t idx = (size_t) p->y * state->width + p->x;
					trail[idx] = (uint8_t) (i + 1); /* 1..9 */
				}
			}
		}

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

		if (done)
			break;
	}

	/* ===== Render FINAL  ===== */
	if (!headless && !error) {
		/* Última actualización de rastro por si hubo movimiento en el tick final */
		reader_enter(sync);
		if (has_colors()) {
			for (unsigned i = 0; i < state->player_count && i < MAX_PLAYERS; ++i) {
				const PlayerInfo *p = &state->players[i];
				if (p->x < state->width && p->y < state->height) {
					const size_t idx = (size_t) p->y * state->width + p->x;
					trail[idx] = (uint8_t) (i + 1); /* 1..9 */
				}
			}
		}
		reader_exit(sync);

		clear();
		int last_row = 2;
		print_board_flat(state, trail, &last_row);
		int after_summary = print_final_summary(state, last_row);

		attron(A_BOLD);
		mvprintw(after_summary + 1, 0, "Juego terminado. Presioná cualquier tecla para salir...");
		attroff(A_BOLD);
		refresh();

		/* Espera bloqueante de una tecla para no borrar la pantalla al salir */
		nodelay(stdscr, FALSE);
		(void) getch();
	}

	/* En modo headless, imprimimos un resumen textual claro */
	if (headless) {
		fprintf(stderr, "=== RESUMEN FINAL ===\n");
		fprintf(stderr, "idx  %-12s  %7s  %8s  %9s  %-8s\n", "nombre", "score", "válidas", "inválidas", "estado");
		for (unsigned i = 0; i < state->player_count && i < MAX_PLAYERS; ++i) {
			const PlayerInfo *p = &state->players[i];
			fprintf(stderr, "[%u]  %-12s  %7u  %8u  %9u  %-8s\n", i, p->name, p->score, p->valid_moves,
					p->invalid_moves, p->blocked ? "BLOCKED" : "OK");
		}
	}

	if (ncurses_initialized)
		endwin();
	if (scr)
		delscreen(scr);
	free(trail);
	shm_unmap(sync, sizeof(GameSync));
	shm_unmap(state, state_bytes);
	return error ? 1 : 0;
}
