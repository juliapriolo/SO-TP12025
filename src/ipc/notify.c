// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <semaphore.h>
#include "master_utils.h"
#include "sync_reader.h"
#include "sync_writer.h"

void notify_view_and_delay_if_any(Master *M) {
    if (M->view.pid > 0) {
        (void) sem_post(&M->sync->sem_master_to_view);
        while (sem_wait(&M->sync->sem_view_to_master) == -1 && errno == EINTR) { }
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
        while (sem_wait(&M->sync->sem_view_to_master) == -1 && errno == EINTR) { }
    }

    /* despertar al menos una vez a cada jugador (si alguno esta en sem_wait) */
    for (unsigned i = 0; i < M->args.player_count; ++i) {
        (void) sem_post(&M->sync->sem_player_can_send[i]);
    }
}
