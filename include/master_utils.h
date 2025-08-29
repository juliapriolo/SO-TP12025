#ifndef SO_TP12025_MASTER_UTILS_H
#define SO_TP12025_MASTER_UTILS_H

#include "state.h"
#include "sync_writer.h"

#include <string.h>
#include <unistd.h>

#define MAX_PLAYERS 9

/* parseo simple de argumentos */
typedef struct {
	unsigned width, height;
	long delay_ms;
	unsigned timeout_s;
	unsigned seed_set;
	unsigned seed;
	const char *view_path; /* opcional */
	unsigned player_count;
	const char *player_paths[MAX_PLAYERS];
} Args;

/* proceso de spawn */
typedef struct {
	pid_t pid;
	int pipe_rd; /* lado de lectura que usa el master */
	int pipe_wr; /* solo para cerrar en el master */
	const char *path;
} Child;

typedef struct {
	Args args;
	GameState *state;
	GameSync *sync;
	size_t state_bytes;
	Child view; /* opcional: pid != 0 si lanzado */
	Child players[MAX_PLAYERS];
} Master;

void die(const char *fmt, ...);

void sleep_ms(long ms);

uint64_t now_ms_monotonic(void);

/*  colocacion determinista de jugadores en una grilla 3x3  */
void initial_positions(unsigned w, unsigned h, unsigned n, unsigned short xs[MAX_PLAYERS],
							  unsigned short ys[MAX_PLAYERS]);

/* impresion del estado final de procesos */
void print_child_status(pid_t pid, int status, const char *label, const PlayerInfo *pinfo_or_null);

void usage(const char *argv0);

void parse_args(int argc, char **argv, Args *out);

/* Inicialización de shm estado y sync */
void init_board(GameState *st, unsigned w, unsigned h);

void init_sync(GameSync *sync, unsigned n_players);

pid_t spawn_view(Master *M);

void spawn_players(Master *M, unsigned short px[MAX_PLAYERS], unsigned short py[MAX_PLAYERS]);

bool cell_in_bounds(unsigned w, unsigned h, int nx, int ny);

/* verifica si un jugador puede hacer algun movimiento valido */
bool player_can_move(const GameState *st, unsigned player_idx);

/* cuenta cuantos jugadores pueden moverse */
unsigned count_players_that_can_move(const GameState *st);

/* retorna true si el movimiento fue valido y modifico el estado */
bool apply_move_locked(GameState *st, unsigned player_idx, unsigned char move);

/*  notificar vista si existe  */
void notify_view_and_delay_if_any(Master *M);

/*  senializar a un jugador que puede enviar el proximo movimiento  */
void allow_next_send(Master *M, unsigned i);

/*  fase de finalizacion  */
void set_finished_and_wake_all(Master *M);

/* comparacion de jugadores para el podio */
int compare_players_for_podium(const void *a, const void *b);

/* imprime el podio de jugadores */
void print_podium(const GameState *state);

#endif //SO_TP12025_MASTER_UTILS_H