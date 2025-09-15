#ifndef SO_TP12025_GAME_INIT_H
#define SO_TP12025_GAME_INIT_H

#include "state.h"
#include "sync.h"
#include "master_utils.h"

typedef struct {
	GameState *state;
	GameSync *sync;
	size_t state_bytes;
} GameShmData;

// función para crear las shared memories del juego crea tanto el estado como la sincronización e inicializa ambos
GameShmData create_game_shm(unsigned width, unsigned height, unsigned player_count);

// función para inicializar el juego completo incluyendo la vista crea las posiciones iniciales de los jugadores y lanza la vista si es necesario
Master init_game_with_view(const Args *args, const GameShmData *shm_data);

// función para finalizar la inicialización del juego retorna true si el juego terminó temprano (todos bloqueados), false si continúa
bool finalize_game_setup(Master *M);

#endif // SO_TP12025_GAME_INIT_H
