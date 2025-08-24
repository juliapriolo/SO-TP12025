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

static void print_board_flat(const GameState *s, int *out_last_row) {
    int term_rows, term_cols;
    getmaxyx(stdscr, term_rows, term_cols);

    int cell_w = 5; // | %2d |
    int cell_h = 2; // borde + contenido
    int board_width = (int)s->width * cell_w + 1;
    int board_height = (int)s->height * cell_h + 1;

    if (board_width > term_cols || board_height + 4 > term_rows) {
        mvprintw(2, 2, "Terminal demasiado pequeña (%dx%d).",
                 term_cols, term_rows);
        mvprintw(3, 2, "Reduce el tablero (%ux%u) o agranda la ventana.",
                 s->width, s->height);
        if (out_last_row) *out_last_row = 4;
        return;
    }

    int start_row = 2;
    int start_col = 0;

    attron(A_BOLD);
    mvprintw(start_row - 1, start_col, " TABLERO (%ux%u) ", s->width, s->height);
    attroff(A_BOLD);

    int row = start_row;

    // Línea superior
    int col = start_col;
    mvaddch(row, col++, ACS_ULCORNER);
    for (unsigned int x = 0; x < s->width; ++x) {
        for (int i = 0; i < 4; i++) mvaddch(row, col++, ACS_HLINE);
        mvaddch(row, col++, (x == (unsigned int)(s->width - 1)) ? ACS_URCORNER : ACS_TTEE);
    }
    row++;

    // Filas con datos
    for (unsigned int y = 0; y < s->height; ++y) {
        col = start_col;
        mvaddch(row, col++, ACS_VLINE);
        for (unsigned int x = 0; x < s->width; ++x) {
            int32_t cell = s->board[y * s->width + x];
            if (cell % 2 == 0) attron(COLOR_PAIR(2)); // pares → verde
            else attron(COLOR_PAIR(3));               // impares → amarillo

            mvprintw(row, col, " %2d ", cell);

            attroff(COLOR_PAIR(2));
            attroff(COLOR_PAIR(3));
            col += 4;
            mvaddch(row, col++, ACS_VLINE);
        }
        row++;

        // Línea intermedia o inferior
        col = start_col;
    if (y == (unsigned int)(s->height - 1)) {
            mvaddch(row, col++, ACS_LLCORNER);
            for (unsigned int x = 0; x < s->width; ++x) {
                for (int i = 0; i < 4; i++) mvaddch(row, col++, ACS_HLINE);
                mvaddch(row, col++, (x == (unsigned int)(s->width - 1)) ? ACS_LRCORNER : ACS_BTEE);
            }
        } else {
            mvaddch(row, col++, ACS_LTEE);
            for (unsigned int x = 0; x < s->width; ++x) {
                for (int i = 0; i < 4; i++) mvaddch(row, col++, ACS_HLINE);
                mvaddch(row, col++, (x == (unsigned int)(s->width - 1)) ? ACS_RTEE : ACS_PLUS);
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

    for (unsigned int i = 0; i < s->player_count && i < 9; ++i) {
        const PlayerInfo *p = &s->players[i];

        if (p->blocked) attron(COLOR_PAIR(4)); // bloqueado → rojo
        else attron(COLOR_PAIR(1));            // normal → cian

        mvprintw(row++, 0,
            "  [%u] %-12s score=%u  valid=%u  invalid=%u  pos=(%u,%u)  %s",
            i, p->name, p->score, p->valid_moves,
            p->invalid_moves, p->y, p->x,
            p->blocked ? "BLOCKED" : "OK");

        attroff(COLOR_PAIR(1));
        attroff(COLOR_PAIR(4));
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
            init_pair(1, COLOR_CYAN, -1);     // jugadores
            init_pair(2, COLOR_GREEN, -1);    // números pares
            init_pair(3, COLOR_YELLOW, -1);   // números impares
            init_pair(4, COLOR_RED, -1);      // jugadores bloqueados
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
        reader_exit(sync);

        if (!headless) {
            mvprintw(LINES - 2, 0, "==============================================");
            mvprintw(LINES - 1, 0, "finished=%s", done ? "true" : "false");
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
