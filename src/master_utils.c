#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <semaphore.h>
#include "master_utils.h"
#include "sync_writer.h"
#include "shm.h"

/* direcciones 0..7: 0=arriba y sentido horario (igual que player.c) */
static const int DX[8] = {0, 1, 1, 1, 0, -1, -1, -1};
static const int DY[8] = {-1, -1, 0, 1, 1, 1, 0, -1};

void die(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	exit(EXIT_FAILURE);
}

void sleep_ms(long ms) {
	struct timespec ts = {.tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000L};
	while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {
	}
}

uint64_t now_ms_monotonic(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t) ts.tv_sec * 1000ULL + (uint64_t) ts.tv_nsec / 1000000ULL;
}

void initial_positions(unsigned w, unsigned h, unsigned n, unsigned short xs[MAX_PLAYERS],
							  unsigned short ys[MAX_PLAYERS]) {
	/* anclas en una grilla 3x3 (centros relativos) */
	const int gx[3] = {1, 2, 3};
	const int gy[3] = {1, 2, 3};
	unsigned count = 0;
	for (int ry = 0; ry < 3 && count < n; ++ry) {
		for (int rx = 0; rx < 3 && count < n; ++rx) {
			unsigned short x = (unsigned short) (((unsigned) gx[rx] * (w + 1U)) / 4U);
			unsigned short y = (unsigned short) (((unsigned) gy[ry] * (h + 1U)) / 4U);
			if (x >= w)
				x = (unsigned short) (w - 1U);
			if (y >= h)
				y = (unsigned short) (h - 1U);
			xs[count] = x;
			ys[count] = y;
			++count;
		}
	}
}

/* impresion del estado final de procesos */
void print_child_status(pid_t pid, int status, const char *label, const PlayerInfo *pinfo_or_null) {
	if (WIFEXITED(status)) {
		int code = WEXITSTATUS(status);
		if (pinfo_or_null) {
			fprintf(stderr, "%s pid=%d exit=%d score=%u\n", label, (int) pid, code, pinfo_or_null->score);
		}
		else {
			fprintf(stderr, "%s pid=%d exit=%d\n", label, (int) pid, code);
		}
	}
	else if (WIFSIGNALED(status)) {
		int sig = WTERMSIG(status);
		if (pinfo_or_null) {
			fprintf(stderr, "%s pid=%d signal=%d score=%u\n", label, (int) pid, sig, pinfo_or_null->score);
		}
		else {
			fprintf(stderr, "%s pid=%d signal=%d\n", label, (int) pid, sig);
		}
	}
	else {
		if (pinfo_or_null) {
			fprintf(stderr, "%s pid=%d (status=0x%x) score=%u\n", label, (int) pid, status, pinfo_or_null->score);
		}
		else {
			fprintf(stderr, "%s pid=%d (status=0x%x)\n", label, (int) pid, status);
		}
	}
}

void usage(const char *argv0) {
	fprintf(stderr,
			"Uso: %s [-w width] [-h height] [-d delay_ms] [-t timeout_s] [-s seed] "
			"[-v view] -p player1 [player2 ... player9]\n",
			argv0);
}

void parse_args(int argc, char **argv, Args *out) {
	*out = (Args){.width = 10,
				  .height = 10,
				  .delay_ms = 200,
				  .timeout_s = 10,
				  .seed_set = 0,
				  .seed = 0,
				  .view_path = NULL,
				  .player_count = 0};

	int opt;
	while ((opt = getopt(argc, argv, "w:h:d:t:s:v:p:")) != -1) {
		switch (opt) {
			case 'w':
				out->width = (unsigned) atoi(optarg);
				break;
			case 'h':
				out->height = (unsigned) atoi(optarg);
				break;
			case 'd':
				out->delay_ms = strtol(optarg, NULL, 10);
				break;
			case 't':
				out->timeout_s = (unsigned) atoi(optarg);
				break;
			case 's':
				out->seed = (unsigned) strtoul(optarg, NULL, 10);
				out->seed_set = 1;
				break;
			case 'v':
				out->view_path = optarg;
				break;
			case 'p': {
				/* despues de -p vienen 1..9 rutas; las tomamos todas las restantes */
				out->player_count = 0;
				out->player_paths[out->player_count++] = optarg;
				while (optind < argc && argv[optind][0] != '-') {
					if (out->player_count >= MAX_PLAYERS)
						die("Demasiados jugadores (max %d).", MAX_PLAYERS);
					out->player_paths[out->player_count++] = argv[optind++];
				}
				break;
			}
			default:
				usage(argv[0]);
				exit(EXIT_FAILURE);
		}
	}

	if (out->width < 10 || out->height < 10)
		die("width/height mínimo = 10");
	if (out->player_count == 0)
		die("Debe especificar al menos 1 jugador con -p");
	if (!out->seed_set)
		out->seed = (unsigned) time(NULL);
}

/* Inicialización de shm estado y sync */
void init_board(GameState *st, unsigned w, unsigned h) {
	for (unsigned y = 0; y < h; ++y) {
		for (unsigned x = 0; x < w; ++x) {
			int val = (rand() % 9) + 1; /* 1..9 */
			st->board[y * w + x] = val;
		}
	}
}

void init_sync(GameSync *sync, unsigned n_players) {
	const int pshared = 1;

	/* handshake master <-> vista */
	if (sem_init(&sync->sem_master_to_view, pshared, 0) == -1)
		die("sem_init(sem_master_to_view): %s", strerror(errno));
	if (sem_init(&sync->sem_view_to_master, pshared, 0) == -1)
		die("sem_init(sem_view_to_master): %s", strerror(errno));

	/* lectores-escritores con prioridad al escritor (C, D, E, F) */
	if (sem_init(&sync->sem_turnstile, pshared, 1) == -1) /* C: molinete */
		die("sem_init(sem_turnstile): %s", strerror(errno));
	if (sem_init(&sync->sem_state, pshared, 1) == -1) /* D: mutex estado */
		die("sem_init(sem_state): %s", strerror(errno));
	if (sem_init(&sync->sem_reader_mutex, pshared, 1) == -1) /* E: protege readers_count */
		die("sem_init(sem_reader_mutex): %s", strerror(errno));
	sync->readers_count = 0; /* F */

	/* G[i]: una “ventana” por jugador. dejalo en 1 para habilitar el primer envio */
	for (unsigned i = 0; i < 9; ++i) {
		unsigned init_val = (i < n_players) ? 1u : 0u;
		if (sem_init(&sync->sem_player_can_send[i], pshared, init_val) == -1)
			die("sem_init(sem_player_can_send[%u]): %s", i, strerror(errno));
	}
}

pid_t spawn_view(Master *M) {
	if (!M->args.view_path)
		return 0;

	pid_t pid = fork();
	if (pid < 0)
		die("fork(view): %s", strerror(errno));
	if (pid == 0) {
		/* hijo: exec view */
		char wbuf[32], hbuf[32];
		char view_path_copy[256];
		snprintf(wbuf, sizeof wbuf, "%u", M->args.width);
		snprintf(hbuf, sizeof hbuf, "%u", M->args.height);
		snprintf(view_path_copy, sizeof view_path_copy, "%s", M->args.view_path);
		char *argvv[] = {view_path_copy, wbuf, hbuf, NULL};
		execv(view_path_copy, argvv);
		perror("execv(view)");
		_exit(127);
	}
	return pid;
}

void spawn_players(Master *M, unsigned short px[MAX_PLAYERS], unsigned short py[MAX_PLAYERS]) {
	for (unsigned i = 0; i < M->args.player_count; ++i) {
		int pipefd[2];
		if (pipe(pipefd) == -1)
			die("pipe(): %s", strerror(errno));

		M->players[i].pipe_rd = pipefd[0];
		M->players[i].pipe_wr = pipefd[1];
		M->players[i].path = M->args.player_paths[i];

		/* inicializar jugador en el estado (nombre, pos, etc) */
		PlayerInfo *p = &M->state->players[i];
		memset(p, 0, sizeof *p);
		snprintf(p->name, sizeof p->name, "user%u", i);
		p->score = 0;
		p->valid_moves = 0;
		p->invalid_moves = 0;
		p->x = px[i];
		p->y = py[i];
		p->blocked = false;
		p->pid = 0; /* se setea tras fork */

		/* marcar su celda inicial como capturada por -id (0..8). La celda inicial no otorga puntos. */
		int idx = (int) py[i] * (int) M->args.width + (int) px[i];
		M->state->board[idx] = -(int) i;

		pid_t pid = fork();
		if (pid < 0)
			die("fork(player): %s", strerror(errno));
		if (pid == 0) {
			/* hijo (jugador): su stdout debe ser el lado de escritura del pipe */
			if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
				perror("dup2(player)");
				_exit(127);
			}
			/* cerrar extremos no usados y no heredar otros pipes */
			for (unsigned k = 0; k < M->args.player_count; ++k) {
				if (M->players[k].pipe_rd >= 0)
					close(M->players[k].pipe_rd);
				if (M->players[k].pipe_wr >= 0 && M->players[k].pipe_wr != pipefd[1])
					close(M->players[k].pipe_wr);
			}
			if (M->view.pid > 0) { /* nada especial que cerrar del view */
			}

			/* ejecutar binario del jugador */
			char wbuf[32], hbuf[32];
			char player_path_copy[256];
			snprintf(wbuf, sizeof wbuf, "%u", M->args.width);
			snprintf(hbuf, sizeof hbuf, "%u", M->args.height);
			snprintf(player_path_copy, sizeof player_path_copy, "%s", M->args.player_paths[i]);
			char *argvp[] = {player_path_copy, wbuf, hbuf, NULL};
			execv(player_path_copy, argvp);
			perror("execv(player)");
			_exit(127);
		}
		/* padre: cerrar lado de escritura (lo mantiene para control si quiere) */
		M->players[i].pid = pid;
		M->state->players[i].pid = pid;
		/* el master nunca escribe al jugador; cerramos el WR para poder detectar EOF si el jugador muere */
		close(M->players[i].pipe_wr);
		M->players[i].pipe_wr = -1;

		/* hacer no bloqueante el read-end para usar select() seguro */
		int flags = fcntl(M->players[i].pipe_rd, F_GETFL, 0);
		(void) fcntl(M->players[i].pipe_rd, F_SETFL, flags | O_NONBLOCK);
	}
}

/* validacion y aplicacion de movimiento */
bool cell_in_bounds(unsigned w, unsigned h, int nx, int ny) {
	return nx >= 0 && ny >= 0 && (unsigned) nx < w && (unsigned) ny < h;
}

/* verifica si un jugador puede hacer algun movimiento valido */
bool player_can_move(const GameState *st, unsigned player_idx) {
	if (player_idx >= st->player_count)
		return false;

	unsigned short x = st->players[player_idx].x;
	unsigned short y = st->players[player_idx].y;

	/* revisar las 8 direcciones */
	for (int dir = 0; dir < 8; ++dir) {
		int nx = (int) x + DX[dir];
		int ny = (int) y + DY[dir];

		/* si esta dentro de bounds y la celda esta libre (>0) */
		if (cell_in_bounds(st->width, st->height, nx, ny)) {
			int32_t cell = st->board[(unsigned) ny * st->width + (unsigned) nx];
			if (cell > 0)
				return true; /* Puede moverse aquí */
		}
	}
	return false; /* no puede moverse a ningun lado */
}

unsigned count_players_that_can_move(const GameState *st) {
	unsigned can_move = 0;
	for (unsigned i = 0; i < st->player_count; ++i) {
		if (!st->players[i].blocked && player_can_move(st, i)) {
			can_move++;
		}
	}
	return can_move;
}

bool apply_move_locked(GameState *st, unsigned player_idx, unsigned char move) {
	if (move > 7) {
		st->players[player_idx].invalid_moves++;

		/* verificar si el jugador quedo bloqueado despues de un movimiento invalido */
		if (!player_can_move(st, player_idx)) {
			st->players[player_idx].blocked = true;
		}
		return false;
	}
	int dx = DX[move], dy = DY[move];
	int x = (int) st->players[player_idx].x;
	int y = (int) st->players[player_idx].y;
	int nx = x + dx, ny = y + dy;

	if (!cell_in_bounds(st->width, st->height, nx, ny)) {
		st->players[player_idx].invalid_moves++;

		/* verificar si el jugador quedó bloqueado */
		if (!player_can_move(st, player_idx)) {
			st->players[player_idx].blocked = true;
		}
		return false;
	}
	int32_t *cellp = &st->board[(unsigned) ny * st->width + (unsigned) nx];
	if (*cellp <= 0) {
		/* no libre: ya capturada (<=0) o jugador encima */
		st->players[player_idx].invalid_moves++;

		/* verificar si el jugador quedó bloqueado */
		if (!player_can_move(st, player_idx)) {
			st->players[player_idx].blocked = true;
		}
		return false;
	}

	/* valido: sumar recompensa, marcar celda capturada, actualizar pos/counters */
	st->players[player_idx].score += (unsigned) *cellp;
	st->players[player_idx].valid_moves++;
	*cellp = -(int) player_idx; /* capturada por este jugador */
	st->players[player_idx].x = (unsigned short) nx;
	st->players[player_idx].y = (unsigned short) ny;

	/* despues de un movimiento valido, verificar si el jugador quedo bloqueado en su nueva posicion */
	if (!player_can_move(st, player_idx)) {
		st->players[player_idx].blocked = true;
	}

	return true;
}

void notify_view_and_delay_if_any(Master *M) {
	if (M->view.pid > 0) {
		(void) sem_post(&M->sync->sem_master_to_view);
		while (sem_wait(&M->sync->sem_view_to_master) == -1 && errno == EINTR) {
		}
	}
	if (M->args.delay_ms > 0) {
		sleep_ms(M->args.delay_ms);
	}
}

void allow_next_send(Master *M, unsigned i) {
	(void) sem_post(&M->sync->sem_player_can_send[i]);
}

void set_finished_and_wake_all(Master *M) {
	writer_enter(M->sync);
	M->state->finished = true;
	writer_exit(M->sync);

	/* despertar a la vista para que haga el ultimo render */
	if (M->view.pid > 0) {
		(void) sem_post(&M->sync->sem_master_to_view);
		while (sem_wait(&M->sync->sem_view_to_master) == -1 && errno == EINTR) {
		}
	}

	/* despertar al menos una vez a cada jugador (si alguno esta en sem_wait) */
	for (unsigned i = 0; i < M->args.player_count; ++i) {
		(void) sem_post(&M->sync->sem_player_can_send[i]);
	}
}

int compare_players_for_podium(const void *a, const void *b) {
	const PlayerInfo *pa = (const PlayerInfo *) a;
	const PlayerInfo *pb = (const PlayerInfo *) b;

	/* mayor puntaje gana */
	if (pa->score > pb->score)
		return -1;
	if (pa->score < pb->score)
		return 1;

	/* en caso de empate, menor cantidad de movimientos validos gana */
	if (pa->valid_moves < pb->valid_moves)
		return -1;
	if (pa->valid_moves > pb->valid_moves)
		return 1;

	/* Si sigue empate, menor cantidad de movimientos invalidos gana */
	if (pa->invalid_moves < pb->invalid_moves)
		return -1;
	if (pa->invalid_moves > pb->invalid_moves)
		return 1;

	return 0;
}

void print_podium(const GameState *state) {
	PlayerInfo players_copy[MAX_PLAYERS];
	for (unsigned i = 0; i < state->player_count; ++i) {
		players_copy[i] = state->players[i];
	}

	qsort(players_copy, state->player_count, sizeof(PlayerInfo), compare_players_for_podium);

	printf("\n");
	printf("=== PODIO FINAL ===\n");
	printf("POS  JUGADOR      PUNTAJE  VALIDOS  INVALIDOS\n");
	printf("==========================================\n");

	for (unsigned i = 0; i < state->player_count; ++i) {
		const char *medal = "   ";
		if (i == 0)
			medal = "[1]";
		else if (i == 1)
			medal = "[2]";
		else if (i == 2)
			medal = "[3]";
		else
			medal = "   ";

		printf("%-3s  %-12s %-8u %-8u %-8u\n", medal, players_copy[i].name, players_copy[i].score,
			   players_copy[i].valid_moves, players_copy[i].invalid_moves);
	}
	printf("==========================================\n");

	/* mostrar criterios de desempate solo si hay empates */
	bool has_ties = false;
	for (unsigned i = 0; i < state->player_count - 1; ++i) {
		if (compare_players_for_podium(&players_copy[i], &players_copy[i + 1]) == 0) {
			has_ties = true;
			break;
		}
	}

	if (has_ties) {
		printf("\nCriterios de desempate:\n");
		printf("1. Mayor puntaje\n");
		printf("2. Menor cantidad de movimientos validos (eficiencia)\n");
		printf("3. Menor cantidad de movimientos invalidos\n");
		printf("4. Empate si todos los criterios son iguales\n");
	}

	printf("\n");
}

void finish_game_and_cleanup(Master *M, GameState *state, GameSync *sync, size_t state_bytes) {
	/*  esperar a que terminen vista y jugadores; loguear estado  */
	/* post extra a la vista por si quedo esperando */
	if (M->view.pid > 0)
		(void) sem_post(&M->sync->sem_master_to_view);

	if (M->view.pid > 0) {
		int status;
		while (waitpid(M->view.pid, &status, 0) == -1 && errno == EINTR) {
		}
		print_child_status(M->view.pid, status, "view", NULL);
	}

	printf("Juego terminado! \n");
	sleep_ms(1000);
	print_podium(state);

	int status;
	for (unsigned i = 0; i < M->args.player_count; ++i) {
		pid_t pid = M->players[i].pid;
		if (pid <= 0)
			continue;
		while (waitpid(pid, &status, 0) == -1 && errno == EINTR) {
		}
		print_child_status(pid, status, "player", &state->players[i]);
	}

	cleanup_master(M);

	/* limpia memoria compartida */
	shm_unmap(sync, sizeof(GameSync));
	shm_unmap(state, state_bytes);
	
	/* elimina archivos de memoria compartida del sistema */
	shm_delete("/game_sync");
	shm_delete("/game_state");
}

void cleanup_master(Master *M) {
	/* Cerrar todos los file descriptors de jugadores */
	for (unsigned i = 0; i < M->args.player_count; ++i) {
		if (M->players[i].pipe_rd >= 0) {
			close(M->players[i].pipe_rd);
			M->players[i].pipe_rd = -1;
		}
		if (M->players[i].pipe_wr >= 0) {
			close(M->players[i].pipe_wr);
			M->players[i].pipe_wr = -1;
		}
	}

	/* Cerrar file descriptors de la vista si existe */
	if (M->view.pipe_rd >= 0) {
		close(M->view.pipe_rd);
		M->view.pipe_rd = -1;
	}
	if (M->view.pipe_wr >= 0) {
		close(M->view.pipe_wr);
		M->view.pipe_wr = -1;
	}

	/* Destruir todos los semáforos */
	if (M->sync) {
		sem_destroy(&M->sync->sem_master_to_view);
		sem_destroy(&M->sync->sem_view_to_master);
		sem_destroy(&M->sync->sem_turnstile);
		sem_destroy(&M->sync->sem_state);
		sem_destroy(&M->sync->sem_reader_mutex);
		
		for (unsigned i = 0; i < 9; ++i) {
			sem_destroy(&M->sync->sem_player_can_send[i]);
		}
	}
}