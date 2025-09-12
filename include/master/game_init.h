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

// Función para crear las shared memories del juego
// Crea tanto el estado como la sincronización e inicializa ambos
GameShmData create_game_shm(unsigned width, unsigned height, unsigned player_count);

// Función para inicializar el juego completo incluyendo la vista
// Crea las posiciones iniciales de los jugadores y lanza la vista si es necesario
Master init_game_with_view(const Args *args, const GameShmData *shm_data);

// Función para finalizar la inicialización del juego
// Retorna true si el juego terminó temprano (todos bloqueados), false si continúa
bool finalize_game_setup(Master *M);

#endif // SO_TP12025_GAME_INIT_H
