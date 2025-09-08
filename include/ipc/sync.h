#ifndef SO_TP12025_SYNC_H
#define SO_TP12025_SYNC_H

#define _POSIX_C_SOURCE 200809L
#include <semaphore.h>

typedef struct {
	sem_t sem_master_to_view;	  // A
	sem_t sem_view_to_master;	  // B
	sem_t sem_turnstile;		  // C: bloqueo de nuevos lectores si escritor espera
	sem_t sem_state;			  // D: exclusión mutua sobre el estado
	sem_t sem_reader_mutex;		  // E: protege readers_count
	unsigned int readers_count;	  // F
	sem_t sem_player_can_send[9]; // G[i]
} GameSync;

#endif // SO_TP12025_SYNC_H
