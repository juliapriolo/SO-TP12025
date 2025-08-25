// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "shm.h"
#include "state.h"
#include "sync.h"
#include <sync_reader.h> 
#include <sync_writer.h>  

#define MAX_PLAYERS 9

/* Direcciones 0..7: 0=arriba y sentido horario (igual que player.c) */
static const int DX[8] = {0, 1, 1, 1, 0, -1, -1, -1};
static const int DY[8] = {-1, -1, 0, 1, 1, 1, 0, -1};

static void die(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(EXIT_FAILURE);
}

static void sleep_ms(long ms) {
    struct timespec ts = { .tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000L };
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {}
}

static uint64_t now_ms_monotonic(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

/* ----------- Colocación determinista de jugadores en una grilla 3x3 ----------- */
static void initial_positions(unsigned w, unsigned h, unsigned n,
                              unsigned short xs[MAX_PLAYERS],
                              unsigned short ys[MAX_PLAYERS]) {
    /* anclas en una grilla 3x3 (centros relativos) */
    const int gx[3] = { 1, 2, 3 };
    const int gy[3] = { 1, 2, 3 };
    unsigned count = 0;
    for (int ry = 0; ry < 3 && count < n; ++ry) {
        for (int rx = 0; rx < 3 && count < n; ++rx) {
            unsigned short x = (unsigned short)(((unsigned)gx[rx] * (unsigned)(w + 1)) / 4);
            unsigned short y = (unsigned short)(((unsigned)gy[ry] * (unsigned)(h + 1)) / 4);
            if (x >= w) x = (unsigned short)(w - 1);
            if (y >= h) y = (unsigned short)(h - 1);
            xs[count] = x;
            ys[count] = y;
            ++count;
        }
    }
}

/* ----------- Helpers de nombre y basename ----------- */
/*
static const char *basename_const(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}
*/

/* ----------- Impresión del estado final de procesos ----------- */
static void print_child_status(pid_t pid, int status, const char *label, const PlayerInfo *pinfo_or_null) {
    if (WIFEXITED(status)) {
        int code = WEXITSTATUS(status);
        if (pinfo_or_null) {
            fprintf(stderr, "%s pid=%d exit=%d score=%u\n", label, (int)pid, code, pinfo_or_null->score);
        } else {
            fprintf(stderr, "%s pid=%d exit=%d\n", label, (int)pid, code);
        }
    } else if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        if (pinfo_or_null) {
            fprintf(stderr, "%s pid=%d signal=%d score=%u\n", label, (int)pid, sig, pinfo_or_null->score);
        } else {
            fprintf(stderr, "%s pid=%d signal=%d\n", label, (int)pid, sig);
        }
    } else {
        if (pinfo_or_null) {
            fprintf(stderr, "%s pid=%d (status=0x%x) score=%u\n", label, (int)pid, status, pinfo_or_null->score);
        } else {
            fprintf(stderr, "%s pid=%d (status=0x%x)\n", label, (int)pid, status);
        }
    }
}

/* ----------- Parseo simple de argumentos ----------- */
typedef struct {
    unsigned width, height;
    long delay_ms;
    unsigned timeout_s;
    unsigned seed_set;
    unsigned seed;
    const char *view_path;           /* opcional */
    unsigned player_count;
    const char *player_paths[MAX_PLAYERS];
} Args;

static void usage(const char *argv0) {
    fprintf(stderr,
        "Uso: %s [-w width] [-h height] [-d delay_ms] [-t timeout_s] [-s seed] "
        "[-v view] -p player1 [player2 ... player9]\n", argv0);
}

static void parse_args(int argc, char **argv, Args *out) {
    *out = (Args){
        .width = 10, .height = 10, .delay_ms = 200, .timeout_s = 10,
        .seed_set = 0, .seed = 0, .view_path = NULL, .player_count = 0
    };

    int opt;
    while ((opt = getopt(argc, argv, "w:h:d:t:s:v:p:")) != -1) {
        switch (opt) {
            case 'w': out->width  = (unsigned)atoi(optarg); break;
            case 'h': out->height = (unsigned)atoi(optarg); break;
            case 'd': out->delay_ms = strtol(optarg, NULL, 10); break;
            case 't': out->timeout_s = (unsigned)atoi(optarg); break;
            case 's': out->seed = (unsigned)strtoul(optarg, NULL, 10); out->seed_set = 1; break;
            case 'v': out->view_path = optarg; break;
            case 'p': {
                /* Después de -p vienen 1..9 rutas; las tomamos todas las restantes */
                out->player_count = 0;
                out->player_paths[out->player_count++] = optarg;
                while (optind < argc && argv[optind][0] != '-') {
                    if (out->player_count >= MAX_PLAYERS) die("Demasiados jugadores. Nuesto (max %d).", MAX_PLAYERS);
                    out->player_paths[out->player_count++] = argv[optind++];
                }
                break;
            }
            default: usage(argv[0]); exit(EXIT_FAILURE);
        }
    }

    if (out->width < 10 || out->height < 10) die("width/height mínimo = 10");
    if (out->player_count == 0) die("Debe especificar al menos 1 jugador con -p");
    if (!out->seed_set) out->seed = (unsigned)time(NULL);
}

/* ----------- Inicialización de SHM Estado y Sync ----------- */
static void init_board(GameState *st, unsigned w, unsigned h) {
    for (unsigned y = 0; y < h; ++y) {
        for (unsigned x = 0; x < w; ++x) {
            int val = (rand() % 9) + 1; /* 1..9 */
            st->board[y * w + x] = val;
        }
    }
}

static void init_sync(GameSync *sync, unsigned n_players) {
    const int pshared = 1;

    /* Handshake máster <-> vista */
    if (sem_init(&sync->sem_master_to_view, pshared, 0) == -1)
        die("sem_init(sem_master_to_view): %s", strerror(errno));
    if (sem_init(&sync->sem_view_to_master, pshared, 0) == -1)
        die("sem_init(sem_view_to_master): %s", strerror(errno));

    /* Lectores-escritores con prioridad al escritor (C, D, E, F) */
    if (sem_init(&sync->sem_turnstile,    pshared, 1) == -1)  /* C: molinete */
        die("sem_init(sem_turnstile): %s", strerror(errno));
    if (sem_init(&sync->sem_state,        pshared, 1) == -1)  /* D: mutex estado */
        die("sem_init(sem_state): %s", strerror(errno));
    if (sem_init(&sync->sem_reader_mutex, pshared, 1) == -1)  /* E: protege readers_count */
        die("sem_init(sem_reader_mutex): %s", strerror(errno));
    sync->readers_count = 0;                                  /* F */

    /* G[i]: una “ventana” por jugador. Dejalo en 1 para habilitar el primer envío */
    for (unsigned i = 0; i < 9; ++i) {
        unsigned init_val = (i < n_players) ? 1u : 0u;
        if (sem_init(&sync->sem_player_can_send[i], pshared, init_val) == -1)
            die("sem_init(sem_player_can_send[%u]): %s", i, strerror(errno));
    }
}


/* ----------- Proceso de spawn ----------- */
typedef struct {
    pid_t pid;
    int   pipe_rd;     /* lado de lectura que usa el master */
    int   pipe_wr;     /* solo para cerrar en el master */
    const char *path;
} Child;

typedef struct {
    Args        args;
    GameState  *state;
    GameSync   *sync;
    size_t      state_bytes;
    Child       view;                  /* opcional: pid != 0 si lanzado */
    Child       players[MAX_PLAYERS];
} Master;


//static void close_unused_fds_in_child(const Child *children, unsigned n, int keep_wr) {
    /* Cerrar TODOS los fds de escritura salvo keep_wr (para evitar herencia de pipes) */
    /*for (unsigned i = 0; i < n; ++i) {
        if (children[i].pipe_wr >= 0 && children[i].pipe_wr != keep_wr)
            close(children[i].pipe_wr);
        if (children[i].pipe_rd >= 0)
            close(children[i].pipe_rd);
    }
}*/

static pid_t spawn_view(Master *M) {
    if (!M->args.view_path) return 0;

    pid_t pid = fork();
    if (pid < 0) die("fork(view): %s", strerror(errno));
    if (pid == 0) {
        /* hijo: exec view */
        char wbuf[32], hbuf[32];
        snprintf(wbuf, sizeof wbuf, "%u", M->args.width);
        snprintf(hbuf, sizeof hbuf, "%u", M->args.height);
    const char *argvv[] = { M->args.view_path, wbuf, hbuf, NULL };
    char *argvv_nc[4];
    for (int k = 0; k < 4; ++k) argvv_nc[k] = (char *)(uintptr_t)argvv[k];
    execv(M->args.view_path, argvv_nc);
        perror("execv(view)"); _exit(127);
    }
    return pid;
}

static void spawn_players(Master *M, unsigned short px[MAX_PLAYERS], unsigned short py[MAX_PLAYERS]) {
    for (unsigned i = 0; i < M->args.player_count; ++i) {
        int pipefd[2];
        if (pipe(pipefd) == -1) die("pipe(): %s", strerror(errno));

        M->players[i].pipe_rd = pipefd[0];
        M->players[i].pipe_wr = pipefd[1];
        M->players[i].path    = M->args.player_paths[i];

        /* Inicializar jugador en el estado (nombre, pos, etc.) */
        PlayerInfo *p = &M->state->players[i];
        memset(p, 0, sizeof *p);
    snprintf(p->name, sizeof p->name, "player%u", i % 10000);
        p->score = 0;
        p->valid_moves = 0;
        p->invalid_moves = 0;
        p->x = px[i];
        p->y = py[i];
        p->blocked = false;
        p->pid = 0; /* se setea tras fork */

        /* Marcar su celda inicial como capturada por -id (0..8). La celda inicial no otorga puntos. */
        int idx = (int)py[i] * (int)M->args.width + (int)px[i];
        M->state->board[idx] = -(int)i;

        pid_t pid = fork();
        if (pid < 0) die("fork(player): %s", strerror(errno));
        if (pid == 0) {
            /* Hijo (jugador): su stdout debe ser el lado de escritura del pipe */
            if (dup2(pipefd[1], STDOUT_FILENO) == -1) { perror("dup2(player)"); _exit(127); }
            /* Cerrar extremos no usados y NO heredar otros pipes */
            for (unsigned k = 0; k < M->args.player_count; ++k) {
                if (M->players[k].pipe_rd >= 0) close(M->players[k].pipe_rd);
                if (M->players[k].pipe_wr >= 0 && M->players[k].pipe_wr != pipefd[1]) close(M->players[k].pipe_wr);
            }
            if (M->view.pid > 0) { /* nada especial que cerrar del view */ }

            /* Ejecutar binario del jugador */
            char wbuf[32], hbuf[32];
            snprintf(wbuf, sizeof wbuf, "%u", M->args.width);
            snprintf(hbuf, sizeof hbuf, "%u", M->args.height);
            const char *argvp[] = { M->args.player_paths[i], wbuf, hbuf, NULL };
            char *argvp_nc[4];
            for (int k = 0; k < 4; ++k) argvp_nc[k] = (char *)(uintptr_t)argvp[k];
            execv(M->args.player_paths[i], argvp_nc);
            perror("execv(player)"); _exit(127);
        }
        /* Padre: cerrar lado de escritura (lo mantiene para control si quiere) */
        M->players[i].pid = pid;
        M->state->players[i].pid = pid;
        /* El master NUNCA escribe al jugador; cerramos el WR para poder detectar EOF si el jugador muere */
        close(M->players[i].pipe_wr);
        M->players[i].pipe_wr = -1;

        /* Hacer no bloqueante el read-end para usar select() seguro */
        int flags = fcntl(M->players[i].pipe_rd, F_GETFL, 0);
        (void) fcntl(M->players[i].pipe_rd, F_SETFL, flags | O_NONBLOCK);
    }
}

/* ----------- Validación y aplicación de movimiento ----------- */
static bool cell_in_bounds(unsigned w, unsigned h, int nx, int ny) {
    return nx >= 0 && ny >= 0 && (unsigned)nx < w && (unsigned)ny < h;
}

/* Retorna true si el movimiento fue válido y modificó el estado */
static bool apply_move_locked(GameState *st, unsigned player_idx, unsigned char move) {
    if (move > 7) {
        st->players[player_idx].invalid_moves++;
        return false;
    }
    int dx = DX[move], dy = DY[move];
    int x = (int)st->players[player_idx].x;
    int y = (int)st->players[player_idx].y;
    int nx = x + dx, ny = y + dy;

    if (!cell_in_bounds(st->width, st->height, nx, ny)) {
        st->players[player_idx].invalid_moves++;
        return false;
    }
    int32_t *cellp = &st->board[(unsigned)ny * st->width + (unsigned)nx];
    if (*cellp <= 0) {
        /* no libre: ya capturada (<=0) o jugador encima */
        st->players[player_idx].invalid_moves++;
        return false;
    }

    /* Válido: sumar recompensa, marcar celda capturada, actualizar pos/counters */
    st->players[player_idx].score += (unsigned)*cellp;
    st->players[player_idx].valid_moves++;
    *cellp = -(int)player_idx; /* capturada por este jugador */
    st->players[player_idx].x = (unsigned short)nx;
    st->players[player_idx].y = (unsigned short)ny;
    return true;
}

/* ----------- Notificar vista si existe ----------- */
static void notify_view_and_delay_if_any(Master *M) {
    if (M->view.pid > 0) {
        (void)sem_post(&M->sync->sem_master_to_view);
        /* esperar a que la vista imprima */
        while (sem_wait(&M->sync->sem_view_to_master) == -1 && errno == EINTR) {}
    }
    /* dormir -d ms SIEMPRE (aún sin vista) para ritmo estable */
    if (M->args.delay_ms > 0) sleep_ms(M->args.delay_ms);
}

/* ----------- Señalizar a un jugador que puede enviar el próximo movimiento ----------- */
static void allow_next_send(Master *M, unsigned i) {
    (void)sem_post(&M->sync->sem_player_can_send[i]);
}

/* ----------- Fase de finalización ----------- */
static void set_finished_and_wake_all(Master *M) {
    writer_enter(M->sync);
        M->state->finished = true;
    writer_exit(M->sync);

    /* Despertar a la vista para que haga el último render */
    if (M->view.pid > 0) {
        (void)sem_post(&M->sync->sem_master_to_view);
        while (sem_wait(&M->sync->sem_view_to_master) == -1 && errno == EINTR) {}
    }

    /* Despertar al menos una vez a cada jugador (si alguno está en sem_wait) */
    for (unsigned i = 0; i < M->args.player_count; ++i) {
        (void)sem_post(&M->sync->sem_player_can_send[i]);
    }
}

/* ----------- main ----------- */
int main(int argc, char **argv) {
    signal(SIGPIPE, SIG_IGN);

    Args args;
    parse_args(argc, argv, &args);

    srand(args.seed);

    /* Crear SHM Estado y Sync */
    size_t state_bytes = gamestate_bytes((uint16_t)args.width, (uint16_t)args.height);
    GameState *state = (GameState *)shm_create("/game_state", state_bytes, O_RDWR);
    if (!state) die("shm_create(/game_state) failed");

    GameSync *sync = (GameSync *)shm_create("/game_sync", sizeof(GameSync), O_RDWR);
    if (!sync) die("shm_create(/game_sync) failed");

    /* Inicializar estructuras */
    writer_enter(sync);
        memset(state, 0, state_bytes);
        state->width  = (unsigned short)args.width;
        state->height = (unsigned short)args.height;
        state->player_count = args.player_count;
        state->finished = false;
    writer_exit(sync);

    init_sync(sync, args.player_count);

    writer_enter(sync);
        init_board(state, args.width, args.height);
    writer_exit(sync);

    /* Colocar jugadores en posiciones “justas” */
    unsigned short px[MAX_PLAYERS], py[MAX_PLAYERS];
    initial_positions(args.width, args.height, args.player_count, px, py);

    /* Lanzar vista (si hay), luego jugadores */
    Master M = {
        .args = args, .state = state, .sync = sync, .state_bytes = state_bytes,
        .view = { .pid = 0, .pipe_rd = -1, .pipe_wr = -1, .path = args.view_path }
    };

    if (args.view_path) {
        pid_t vpid = spawn_view(&M);
        M.view.pid = vpid;
    }

    writer_enter(sync);
        /* marcar celdas iniciales y seteos de PlayerInfo (parte en spawn) */
    writer_exit(sync);
    spawn_players(&M, px, py);

    /* Primera notificación a la vista para dibujar estado inicial */
    notify_view_and_delay_if_any(&M);

    /* ---------- Bucle principal: select() + round-robin ---------- */
    uint64_t last_valid_ms = now_ms_monotonic();
    unsigned rr_next = 0; /* índice del próximo a intentar atender primero */

    /* Transformar timeout_s a ms */
    const uint64_t timeout_ms = (uint64_t)args.timeout_s * 1000ULL;

    for (;;) {
        /* Armar fd_set con los pipes vivos */
        fd_set rfds;
        FD_ZERO(&rfds);
        int maxfd = -1;
        unsigned alive = 0;

        for (unsigned i = 0; i < args.player_count; ++i) {
            int fd = M.players[i].pipe_rd;
            if (fd >= 0) {
                FD_SET(fd, &rfds);
                if (fd > maxfd) maxfd = fd;
                alive++;
            }
        }

        /* Si ya no hay pipes vivos, finalizamos */
        if (alive == 0) {
            /* Terminó todo: nadie puede enviar más */
            set_finished_and_wake_all(&M);
            break;
        }

        /* Calcular cuánto falta para el timeout relativo a la ÚLTIMA jugada válida */
        uint64_t now = now_ms_monotonic();
        uint64_t elapsed = now - last_valid_ms;
        if (elapsed >= timeout_ms) {
            /* Timeout: fin del juego */
            set_finished_and_wake_all(&M);
            break;
        }
        uint64_t remain_ms = timeout_ms - elapsed;
        struct timeval tv;
        tv.tv_sec = (time_t)(remain_ms / 1000ULL);
        tv.tv_usec = (suseconds_t)((remain_ms % 1000ULL) * 1000ULL);

        int ready = select(maxfd + 1, &rfds, NULL, NULL, &tv);
        if (ready < 0) {
            if (errno == EINTR) continue;
            perror("select");
            set_finished_and_wake_all(&M);
            break;
        }
        if (ready == 0) {
            /* select timeout relativo: se chequea arriba en el loop */
            continue;
        }

        /* Política RR: buscamos el primer jugador con FD listo, empezando en rr_next */
        bool processed_one = false;
        for (unsigned k = 0; k < args.player_count; ++k) {
            unsigned i = (rr_next + k) % args.player_count;
            int fd = M.players[i].pipe_rd;
            if (fd < 0) continue;
            if (!FD_ISSET(fd, &rfds)) continue;

            /* Leer EXACTAMENTE 1 byte si hay; detectar EOF */
            unsigned char move;
            ssize_t rd = read(fd, &move, 1);
            if (rd == 0) {
                /* EOF -> jugador bloqueado */
                writer_enter(sync);
                    state->players[i].blocked = true;
                writer_exit(sync);
                close(M.players[i].pipe_rd);
                M.players[i].pipe_rd = -1;
                /* no hay "allow_next_send" porque ya no vive */
                continue;
            } else if (rd < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
                perror("read(pipe)");
                /* tratamos como bloqueado igualmente */
                writer_enter(sync);
                    state->players[i].blocked = true;
                writer_exit(sync);
                close(M.players[i].pipe_rd);
                M.players[i].pipe_rd = -1;
                continue;
            }

            /* Tenemos un byte: aplicar movimiento bajo lock de ESCRITOR */
            bool was_valid;
            writer_enter(sync);
                was_valid = apply_move_locked(state, i, move);
            writer_exit(sync);

            /* Notificar a vista SOLO si hubo cambio de estado */
            if (was_valid) {
                notify_view_and_delay_if_any(&M);
                last_valid_ms = now_ms_monotonic();
            }

            /* Desbloquear al jugador para que pueda mandar otro movimiento */
            allow_next_send(&M, i);

            /* Avanzar round-robin y salir (una sola solicitud por iteración) */
            rr_next = (i + 1) % args.player_count;
            processed_one = true;
            break;
        }

        /* Si no procesamos ninguno (p.ej. los listos dieron error/EOF), volvemos al select */
        (void)processed_one;
    }

    /* ---------- Esperar a que terminen vista y jugadores; loguear estado ---------- */
    /* Post extra a la vista por si quedó esperando */
    if (M.view.pid > 0) (void)sem_post(&M.sync->sem_master_to_view);

    /* Espera de hijos + logs */
    int status;
    /* Primero los jugadores, imprimiendo puntaje */
    for (unsigned i = 0; i < args.player_count; ++i) {
        pid_t pid = M.players[i].pid;
        if (pid <= 0) continue;
        while (waitpid(pid, &status, 0) == -1 && errno == EINTR) {}
        print_child_status(pid, status, "player", &state->players[i]);
    }
    /* Luego la vista (si la hubo) */
    if (M.view.pid > 0) {
        while (waitpid(M.view.pid, &status, 0) == -1 && errno == EINTR) {}
        print_child_status(M.view.pid, status, "view", NULL);
    }

    /* Limpieza */
    shm_unmap(sync, sizeof(GameSync));
    shm_unmap(state, state_bytes);
    /* No hacemos shm_unlink para dejar debugging posible; si querés, descomentá:
       shm_delete("/game_state"); shm_delete("/game_sync"); */

    return 0;
}
