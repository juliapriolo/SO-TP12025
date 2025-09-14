#ifndef SO_TP12025_MASTER_UTILS_H
#define SO_TP12025_MASTER_UTILS_H

#include "state.h"
#include "sync.h"


typedef struct {
	unsigned width, height;
	long delay_ms;
	unsigned timeout_s;
	unsigned seed_set;
	unsigned seed;
	const char *view_path; 
	unsigned player_count;
	const char *player_paths[MAX_PLAYERS];
} Args;

typedef struct {
	pid_t pid;
	int pipe_rd; 
	int pipe_wr; 
	const char *path;
} Child;

typedef struct {
	Args args;
	GameState *state;
	GameSync *sync;
	size_t state_bytes;
	Child view; 
	Child players[MAX_PLAYERS];
} Master;


#endif //SO_TP12025_MASTER_UTILS_H
