// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "view_utils.h"

#include <stdlib.h>

static int player_text_pair(unsigned i);
static int player_bg_pair(unsigned i);
static bool check_terminal_size(const GameState *s, int *out_last_row);
static int get_cell_background_color(const GameState *s, const uint8_t *trail_idx, unsigned x, unsigned y);
static void print_board_header(const GameState *s);
static void print_board_top_border(const GameState *s);
static void print_cell_content(const GameState *s, const uint8_t *trail_idx, unsigned x, unsigned y, int row, int col);
static void print_board_row_separator(const GameState *s, unsigned y, int row);
static int print_board_rows(const GameState *s, const uint8_t *trail_idx);

void print_board_flat(const GameState *s, const uint8_t *trail_idx, int *out_last_row) {
	if (!check_terminal_size(s, out_last_row)) {
		return;
	}

	print_board_header(s);
	print_board_top_border(s);
	int last_row = print_board_rows(s, trail_idx); // ← Capturar la fila retornada

	if (out_last_row) {
		*out_last_row = last_row; // ← Usar la fila real
	}
}

int print_players(const GameState *s, int board_last_row) {
	int row = board_last_row + 1;

	attron(A_BOLD);
	mvprintw(row++, 0, " JUGADORES (%u):", s->player_count);
	attroff(A_BOLD);

	for (unsigned i = 0; i < s->player_count && i < MAX_PLAYERS; ++i) {
		const PlayerInfo *p = &s->players[i];
		int pair = player_text_pair(i);

		/* Usar formato compacto para ahorrar espacio vertical */
		attron(COLOR_PAIR(pair));
		mvprintw(row, 0, " [%u] %-8s %3u %2u/%2u (%u,%u) ", i, p->name, p->score, p->valid_moves, p->invalid_moves,
				 p->x, p->y);
		attroff(COLOR_PAIR(pair));

		if (p->blocked) {
			attron(COLOR_PAIR(4));
			attron(A_BOLD);
			printw("BLOCKED");
			attroff(A_BOLD);
			attroff(COLOR_PAIR(4));
		}
		else {
			attron(COLOR_PAIR(pair));
			printw("OK");
			attroff(COLOR_PAIR(pair));
		}
		row++;
	}
	return row; /* última fila utilizada + 1 */
}

int print_final_summary(const GameState *s, int start_row) {
	int row = start_row + 1;
	attron(A_BOLD);
	mvprintw(row++, 0, " RESUMEN FINAL ");
	attroff(A_BOLD);

	mvprintw(row++, 0, "%-4s  %-12s  %7s  %8s  %9s  %-8s", "idx", "nombre", "score", "válidas", "inválidas", "estado");

	for (unsigned i = 0; i < s->player_count && i < MAX_PLAYERS; ++i) {
		const PlayerInfo *p = &s->players[i];
		int pair = player_text_pair(i);
		attron(COLOR_PAIR(pair));

		// Datos con anchos exactos que coinciden con el header
		mvprintw(row, 0, "%-4u  %-12s  %7u  %8u  %9u  %-8s", i, p->name, p->score, p->valid_moves, p->invalid_moves,
				 p->blocked ? "BLOCKED" : "OK");
		attroff(COLOR_PAIR(pair));
		row++;
	}

	row++;
	return row;
}

static int player_text_pair(unsigned i) {
	return 10 + (int) (i % 9);
} /* 10..18 */
static int player_bg_pair(unsigned i) {
	return 20 + (int) (i % 9);
} /* 20..28 */

int player_at(const GameState *s, unsigned x, unsigned y) {
	for (unsigned i = 0; i < s->player_count && i < MAX_PLAYERS; ++i) {
		if (s->players[i].x == x && s->players[i].y == y) {
			return (int) i;
		}
	}
	return -1;
}

static bool check_terminal_size(const GameState *s, int *out_last_row) {
	int term_rows, term_cols;
	getmaxyx(stdscr, term_rows, term_cols);

	const int board_width = (int) s->width * CELL_WIDTH + 1;
	const int board_height = (int) s->height * CELL_HEIGHT + 1;

	if (board_width > term_cols || board_height + 6 > term_rows) {
		mvprintw(1, 2, "Terminal demasiado pequeña (%dx%d).", term_cols, term_rows);
		mvprintw(2, 2, "Reduce el tablero (%ux%u) o agranda la ventana.", s->width, s->height);
		if (out_last_row) {
			*out_last_row = 4;
		}
		return false;
	}
	return true;
}

static int get_cell_background_color(const GameState *s, const uint8_t *trail_idx, unsigned x, unsigned y) {
	if (!has_colors()) {
		return 0;
	}

	int pid = player_at(s, x, y);
	if (pid >= 0) {
		return player_bg_pair((unsigned) pid);
	}

	if (trail_idx) {
		const size_t idx = (size_t) y * s->width + x;
		if (trail_idx[idx]) {
			unsigned j = (unsigned) (trail_idx[idx] - 1);
			return player_bg_pair(j);
		}
	}

	return 0;
}

static void print_board_header(const GameState *s) {
	attron(A_BOLD);
	mvprintw(1, 0, " TABLERO (%ux%u) ", s->width, s->height);
	attroff(A_BOLD);
}

static void print_board_top_border(const GameState *s) {
	int row = 2;
	int col = 0;

	mvaddch(row, col++, ACS_ULCORNER); // ┌
	for (unsigned x = 0; x < s->width; ++x) {
		for (int i = 0; i < CELL_INNER_WIDTH; i++) {
			mvaddch(row, col++, ACS_HLINE); // ────
		}
		mvaddch(row, col++, (x == (unsigned) (s->width - 1)) ? ACS_URCORNER : ACS_TTEE);
	}
}

static void print_cell_content(const GameState *s, const uint8_t *trail_idx, unsigned x, unsigned y, int row, int col) {
	const size_t idx = (size_t) y * s->width + x;
	const int32_t cell = s->board[idx];

	int bgpair = get_cell_background_color(s, trail_idx, x, y);

	if (bgpair) {
		attron(COLOR_PAIR(bgpair));
		mvhline(row, col, ' ', CELL_INNER_WIDTH);
		attroff(COLOR_PAIR(bgpair));
	}
	else {
		attron(COLOR_PAIR(2));
		mvprintw(row, col, " %2d ", cell);
		attroff(COLOR_PAIR(2));
	}
}

static void print_board_row_separator(const GameState *s, unsigned y, int row) {
	int col = 0;

	if (y == (unsigned) (s->height - 1)) {
		// Última fila: esquinas inferiores
		mvaddch(row, col++, ACS_LLCORNER);
		for (unsigned x = 0; x < s->width; ++x) {
			for (int i = 0; i < CELL_INNER_WIDTH; i++) {
				mvaddch(row, col++, ACS_HLINE);
			}
			mvaddch(row, col++, (x == (unsigned) (s->width - 1)) ? ACS_LRCORNER : ACS_BTEE);
		}
	}
	else {
		// Filas intermedias: cruces
		mvaddch(row, col++, ACS_LTEE);
		for (unsigned x = 0; x < s->width; ++x) {
			for (int i = 0; i < CELL_INNER_WIDTH; i++) {
				mvaddch(row, col++, ACS_HLINE);
			}
			mvaddch(row, col++, (x == (unsigned) (s->width - 1)) ? ACS_RTEE : ACS_PLUS);
		}
	}
}

static int print_board_rows(const GameState *s, const uint8_t *trail_idx) {
	int row = 3; // Después del header y borde superior

	for (unsigned y = 0; y < s->height; ++y) {
		// Borde izquierdo
		mvaddch(row, 0, ACS_VLINE);

		// Contenido de las celdas
		int col = 1;
		for (unsigned x = 0; x < s->width; ++x) {
			print_cell_content(s, trail_idx, x, y, row, col);
			col += CELL_INNER_WIDTH;
			mvaddch(row, col++, ACS_VLINE);
		}
		row++;

		// Línea intermedia o inferior
		print_board_row_separator(s, y, row);
		row++;
	}

	return row; // ← RETORNAR la última fila usada
}

int initialize_ncurses(SCREEN **scr, int *ncurses_initialized, int *headless) {
	*ncurses_initialized = 0, *headless = 0;

	if (getenv("TERM") == NULL) {
		setenv("TERM", "xterm-256color", 1);
	}

	*scr = newterm(NULL, stdout, stdin);

	if (!*scr) {
		*headless = 1;
		return OK;
	}

	set_term(*scr);
	*ncurses_initialized = 1;
	cbreak();
	noecho();
	curs_set(0);

	if (has_colors()) {
		start_color();
		use_default_colors();
		setup_ncurses_colors();
	}
	return OK;
}

void setup_ncurses_colors(void) {
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

	init_pair(17, COLOR_BLACK, -1);
	init_pair(18, COLOR_BLACK, -1);

    // Paleta de jugadores para fondo de casilla 20..28
    short bg_colors[9] = {
        COLOR_CYAN, COLOR_MAGENTA, COLOR_YELLOW, COLOR_GREEN,
        COLOR_BLUE, COLOR_WHITE, COLOR_RED, COLOR_BLACK, COLOR_BLACK
    };
    for (int i = 0; i < 9; ++i) {
        init_pair((short)(20 + i), -1, (short)bg_colors[i]);
    }
}

void render_final(const GameState *state, uint8_t *trail, int headless) {
	if (!headless) {
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
	else {
		fprintf(stderr, "=== RESUMEN FINAL ===\n");
		fprintf(stderr, "idx  %-12s  %7s  %8s  %9s  %-8s\n", "nombre", "score", "válidas", "inválidas", "estado");
		for (unsigned i = 0; i < state->player_count && i < MAX_PLAYERS; ++i) {
			const PlayerInfo *p = &state->players[i];
			fprintf(stderr, "[%u]  %-12s  %7u  %8u  %9u  %-8s\n", i, p->name, p->score, p->valid_moves,
					p->invalid_moves, p->blocked ? "BLOCKED" : "OK");
		}
	}
}

void update_player_trail(const GameState *state, uint8_t *trail, int headless) {
	if (!headless && has_colors()) {
		for (unsigned i = 0; i < state->player_count && i < MAX_PLAYERS; ++i) {
			const PlayerInfo *p = &state->players[i];
			if (p->x < state->width && p->y < state->height) {
				const size_t idx = (size_t) p->y * state->width + p->x;
				trail[idx] = (uint8_t) (i + 1); /* 1..9 */
			}
		}
	}
}

void destroy_ncurses(SCREEN *scr, int ncurses_initialized) {
	if (ncurses_initialized) {
		endwin();
	}
	if (scr) {
		delscreen(scr);
	}
}
