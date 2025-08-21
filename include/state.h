#ifndef SO_TP12025_STATE_H
#define SO_TP12025_STATE_H

#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

typedef struct {
	char name[16];
	unsigned int score;
	unsigned int invalid_moves;
	unsigned int valid_moves;
	unsigned short x, y;
	pid_t pid;
	bool blocked;
} PlayerInfo;

typedef struct {
	unsigned short width;
	unsigned short height;
	unsigned int player_count;
	PlayerInfo players[9];
	bool finished;
	int board[];
} GameState;

// Helper para calcular bytes totales necesarios para mapear GameState
static inline size_t gamestate_bytes(uint16_t w, uint16_t h) {
	return sizeof(GameState) + (size_t) w * (size_t) h * sizeof(int32_t);
}

#endif // SO_TP12025_STATE_H