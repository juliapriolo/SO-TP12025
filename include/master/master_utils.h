#ifndef SO_TP12025_MASTER_UTILS_H
#define SO_TP12025_MASTER_UTILS_H

#include "state.h"
#include "sync.h"


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


#endif //SO_TP12025_MASTER_UTILS_H
