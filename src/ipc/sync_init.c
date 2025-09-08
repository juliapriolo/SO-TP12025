// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <string.h>
#include <semaphore.h>
#include "sync_init.h"
#include "state.h"
#include "timing.h"

void init_sync(GameSync *sync, unsigned n_players) {
    const int pshared = 1;

    /* handshake master <-> vista */
    if (sem_init(&sync->sem_master_to_view, pshared, 0) == -1)
        die("sem_init(sem_master_to_view): %s", strerror(errno));
    if (sem_init(&sync->sem_view_to_master, pshared, 0) == -1)
        die("sem_init(sem_view_to_master): %s", strerror(errno));

    /* lectores-escritores con prioridad al escritor (C, D, E, F) */
    if (sem_init(&sync->sem_turnstile, pshared, 1) == -1)
        die("sem_init(sem_turnstile): %s", strerror(errno));
    if (sem_init(&sync->sem_state, pshared, 1) == -1)
        die("sem_init(sem_state): %s", strerror(errno));
    if (sem_init(&sync->sem_reader_mutex, pshared, 1) == -1)
        die("sem_init(sem_reader_mutex): %s", strerror(errno));
    sync->readers_count = 0;

    /* G[i]: una “ventana” por jugador. dejalo en 1 para habilitar el primer envio */
    for (unsigned i = 0; i < MAX_PLAYERS; ++i) {
        unsigned init_val = (i < n_players) ? 1u : 0u;
        if (sem_init(&sync->sem_player_can_send[i], pshared, init_val) == -1)
            die("sem_init(sem_player_can_send[%u]): %s", i, strerror(errno));
    }
}
