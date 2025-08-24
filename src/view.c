// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
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
#include <sync_reader.h>
#include <ncurses.h>
#include <signal.h>

/* Devuelve el índice del jugador que está en (x,y) o -1 si no hay */
static int player_at(const GameState *s, unsigned x, unsigned y) {
    for (unsigned i = 0; i < s->player_count && i < 9; ++i) {
        if (s->players[i].x == x && s->players[i].y == y) return (int)i;
    }
    return -1;
}

/* Reglas de ganador:
   1) mayor score; 2) si empatan: menor valid_moves;
   3) si empatan: menor invalid_moves; 4) si todos iguales: -1 (empate) */
static int calcular_ganador(const GameState *s) {
    if (s->player_count == 0) return -1;

    int best = 0;
    for (unsigned i = 1; i < s->player_count; ++i) {
        const PlayerInfo *b = &s->players[best];
        const PlayerInfo *p = &s->players[i];

        if (p->score > b->score) best = (int)i;
        else if (p->score == b->score) {
            if (p->valid_moves < b->valid_moves) best = (int)i;
            else if (p->valid_moves == b->valid_moves) {
                if (p->invalid_moves < b->invalid_moves) best = (int)i;
            }
        }
    }

    /* ¿hay otro exactamente igual al best? → empate */
    const PlayerInfo *g = &s->players[best];
    for (unsigned i = 0; i < s->player_count; ++i) {
        if (i == (unsigned)best) continue;
        const PlayerInfo *p = &s->players[i];
        if (p->score == g->score &&
            p->valid_moves == g->valid_moves &&
            p->invalid_moves == g->invalid_moves) {
            return -1;
        }
    }
    return best;
}

static void print_board_flat(const GameState *s, int *out_last_row) {
    int term_rows, term_cols;
    getmaxyx(stdscr, term_rows, term_cols);

    const int cell_w = 5; /* "| %2d " → 5 columnas */
    const int cell_h = 2; /* borde + contenido */
    const int board_width  = (int)s->width  * cell_w + 1;
    const int board_height = (int)s->height * cell_h + 1;

    if (board_width > term_cols || board_height + 4 > term_rows) {
        mvprintw(1, 2, "Terminal demasiado pequeña (%dx%d).", term_cols, term_rows);
        mvprintw(2, 2, "Reduce el tablero (%ux%u) o agranda la ventana.", s->width, s->height);
        if (out_last_row) *out_last_row = 4;
        return;
    }

    const int start_row = 1;  /* compacto: arriba */
    const int start_col = 0;  /* alineado a la izquierda */

    attron(A_BOLD);
    mvprintw(start_row, start_col, " TABLERO (%ux%u) ", s->width, s->height);
    attroff(A_BOLD);

    int row = start_row + 1;

    /* Línea superior */
    int col = start_col;
    mvaddch(row, col++, ACS_ULCORNER);
    for (unsigned x = 0; x < s->width; ++x) {
        for (int i = 0; i < 4; i++) mvaddch(row, col++, ACS_HLINE);
        mvaddch(row, col++, (x == (unsigned)(s->width - 1)) ? ACS_URCORNER : ACS_TTEE);
    }
    row++;

    /* Filas con valores */
    for (unsigned y = 0; y < s->height; ++y) {
        col = start_col;
        mvaddch(row, col++, ACS_VLINE);
        for (unsigned x = 0; x < s->width; ++x) {
            int32_t cell = s->board[y * s->width + x];

            /* Color: jugador (único) o base para celdas normales */
            int pid = player_at(s, x, y);
            if (pid >= 0) {
                attron(COLOR_PAIR(10 + (pid % 7)));   /* solo color de texto */
            } else {
                attron(COLOR_PAIR(2));                /* color base único */
            }

            mvprintw(row, col, " %2d ", cell);

            if (pid >= 0) {
                attroff(COLOR_PAIR(10 + (pid % 7)));
            } else {
                attroff(COLOR_PAIR(2));
            }

            col += 4;
            mvaddch(row, col++, ACS_VLINE);
        }
        row++;

        /* Línea intermedia o inferior */
        col = start_col;
    if (y == (unsigned)(s->height - 1)) {
            mvaddch(row, col++, ACS_LLCORNER);
            for (unsigned x = 0; x < s->width; ++x) {
                for (int i = 0; i < 4; i++) mvaddch(row, col++, ACS_HLINE);
                mvaddch(row, col++, (x == (unsigned)(s->width - 1)) ? ACS_LRCORNER : ACS_BTEE);
            }
        } else {
            mvaddch(row, col++, ACS_LTEE);
            for (unsigned x = 0; x < s->width; ++x) {
                for (int i = 0; i < 4; i++) mvaddch(row, col++, ACS_HLINE);
                mvaddch(row, col++, (x == (unsigned)(s->width - 1)) ? ACS_RTEE : ACS_PLUS);
            }
        }
        row++;
    }

    if (out_last_row) *out_last_row = row;
}

static void print_players(const GameState *s, int board_last_row) {
    int row = board_last_row + 1;

    attron(A_BOLD);
    mvprintw(row++, 0, " JUGADORES (%u):", s->player_count);
    attroff(A_BOLD);

    for (unsigned i = 0; i < s->player_count && i < 9; ++i) {
        const PlayerInfo *p = &s->players[i];
        int pair = 10 + (int)(i % 7); /* 10..16 */

        attron(COLOR_PAIR(pair));
        mvprintw(row, 0,
                 "  [%u] %-12s score=%u  valid=%u  invalid=%u  pos=(%u,%u)  ",
                 i, p->name, p->score, p->valid_moves, p->invalid_moves, p->y, p->x);
        attroff(COLOR_PAIR(pair));

        if (p->blocked) {
            attron(COLOR_PAIR(4));
            printw("BLOCKED");
            attroff(COLOR_PAIR(4));
        } else {
            attron(COLOR_PAIR(pair));
            printw("OK");
            attroff(COLOR_PAIR(pair));
        }
        row++;
    }
}

int main(int argc, char *argv[]) {
    signal(SIGPIPE, SIG_IGN);

    if (getenv("TERM") == NULL) {
        setenv("TERM", "xterm-256color", 1);
    }

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
    } else {
        set_term(scr);
        ncurses_initialized = 1;
        cbreak();
        noecho();
        curs_set(0);
        if (has_colors()) {
            start_color();
            use_default_colors();
            /* Colores base */
            init_pair(1, COLOR_CYAN,   -1);   /* títulos */
            init_pair(2, COLOR_WHITE,  -1);   /* ► todas las celdas normales */
            init_pair(4, COLOR_RED,    -1);   /* alertas/bloqueado */
            /* Paleta de jugadores (solo color de texto, sin fondo) */
            init_pair(10, COLOR_CYAN,    -1);
            init_pair(11, COLOR_MAGENTA, -1);
            init_pair(12, COLOR_YELLOW,  -1);
            init_pair(13, COLOR_GREEN,   -1);
            init_pair(14, COLOR_BLUE,    -1);
            init_pair(15, COLOR_WHITE,   -1);
            init_pair(16, COLOR_RED,     -1);
        }
    }

    int error = 0;
    for (;;) {
        if (sem_wait(&sync->sem_master_to_view) == -1) {
            if (errno == EINTR) continue;
            error = 1;
            perror("sem_wait(sem_master_to_view)");
            break;
        }

        int done = 0;
        int winner_idx = -2; /* -2 = no evaluado, -1 = empate, >=0 índice ganador */

        if (!headless) {
            clear();
        }

        reader_enter(sync);
            int last_row = 2;
            if (!headless) {
                print_board_flat(state, &last_row);
                print_players(state, last_row);
            }
            done = state->finished ? 1 : 0;
            if (done) {
                winner_idx = calcular_ganador(state);
            }
        reader_exit(sync);

        if (!headless) {
            int footer_row = LINES - 3;
            if (footer_row < 0) footer_row = 0;

            mvprintw(footer_row++, 0, "==============================================================");
            mvprintw(footer_row++, 0, "finished=%s", done ? "true" : "false");

            if (done) {
                if (winner_idx == -1) {
                    attron(COLOR_PAIR(4));
                    mvprintw(footer_row++, 0, "Resultado: EMPATE");
                    attroff(COLOR_PAIR(4));
                } else if (winner_idx >= 0) {
                    int pair = 10 + (winner_idx % 7);
                    const PlayerInfo *winfo = &state->players[winner_idx];
                    attron(COLOR_PAIR(pair));
                    mvprintw(footer_row++, 0,
                             "Ganador: [%d] %s  score=%u  valid=%u  invalid=%u",
                             winner_idx, winfo->name, winfo->score,
                             winfo->valid_moves, winfo->invalid_moves);
                    attroff(COLOR_PAIR(pair));
                }
            }
            refresh();
        }

        if (sem_post(&sync->sem_view_to_master) == -1) {
            error = 1;
            perror("sem_post(sem_view_to_master)");
            break;
        }

        if (done) break;
    }

    if (ncurses_initialized) endwin();
    if (scr) delscreen(scr);
    shm_unmap(sync, sizeof(GameSync));
    shm_unmap(state, state_bytes);
    return error ? 1 : 0;
}
