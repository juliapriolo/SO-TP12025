// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <semaphore.h>
#include "notify.h"
#include "timing.h"
#include "sync_reader.h"
#include "sync_writer.h"


static void handshake_with_view(Master *M) {
    if (M->view.pid > 0) {
        (void) sem_post(&M->sync->sem_master_to_view);
        sem_wait_intr(&M->sync->sem_view_to_master);
    }
}

void notify_view_and_delay_if_any(Master *M) {
    handshake_with_view(M);
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

    handshake_with_view(M);

    for (unsigned i = 0; i < M->args.player_count; ++i) {
        (void) sem_post(&M->sync->sem_player_can_send[i]);
    }
}
