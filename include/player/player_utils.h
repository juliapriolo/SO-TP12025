#ifndef PLAYER_UTILS_H
#define PLAYER_UTILS_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>  
#include <unistd.h> 
#include "state.h"


/* Duerme la cantidad de milisegundos indicada. Reintenta si es interrumpido por señales.*/
void sleep_ms(long ms);

/* Devuelve el índice de este proceso (getpid()) dentro de st->players, o -1 si no está.
   Limita la búsqueda a los primeros 9 jugadores como en la implementación original. */
int find_self_index(GameState *st);

/* Elige la mejor dirección (0..7) desde (x,y) hacia una celda libre (>0).
  Devuelve -1 si no hay movimientos válidos. */
int pick_direction(GameState *st, unsigned short x, unsigned short y);

#endif // PLAYER_UTILS_H
