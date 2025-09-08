// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "master_utils.h"
#include "shm.h"
#include <sys/wait.h>

void finish_game_and_cleanup(Master *M, GameState *state, GameSync *sync, size_t state_bytes) {
    if (M->view.pid > 0)
        (void) sem_post(&M->sync->sem_master_to_view);

    if (M->view.pid > 0) {
        int status;
        while (waitpid(M->view.pid, &status, 0) == -1 && errno == EINTR) { }
        print_child_status(M->view.pid, status, "view", NULL);
    }

    printf("Juego terminado! \n");
    sleep_ms(1000);
    print_podium(state);

    int status;
    for (unsigned i = 0; i < M->args.player_count; ++i) {
        pid_t pid = M->players[i].pid;
        if (pid <= 0) continue;
        while (waitpid(pid, &status, 0) == -1 && errno == EINTR) { }
        print_child_status(pid, status, "player", &state->players[i]);
    }

    cleanup_master(M);

    /* limpia memoria compartida */
    shm_unmap(sync, sizeof(GameSync));
    shm_unmap(state, state_bytes);

    /* elimina archivos de memoria compartida del sistema */
    shm_delete("/game_sync");
    shm_delete("/game_state");
}

void cleanup_master(Master *M) {
    /* Cerrar todos los file descriptors de jugadores */
    for (unsigned i = 0; i < M->args.player_count; ++i) {
        if (M->players[i].pipe_rd >= 0) {
            close(M->players[i].pipe_rd);
            M->players[i].pipe_rd = -1;
        }
        if (M->players[i].pipe_wr >= 0) {
            close(M->players[i].pipe_wr);
            M->players[i].pipe_wr = -1;
        }
    }

    /* Cerrar file descriptors de la vista si existe */
    if (M->view.pipe_rd >= 0) {
        close(M->view.pipe_rd);
        M->view.pipe_rd = -1;
    }
    if (M->view.pipe_wr >= 0) {
        close(M->view.pipe_wr);
        M->view.pipe_wr = -1;
    }

    /* Destruir todos los semáforos */
    if (M->sync) {
        sem_destroy(&M->sync->sem_master_to_view);
        sem_destroy(&M->sync->sem_view_to_master);
        sem_destroy(&M->sync->sem_turnstile);
        sem_destroy(&M->sync->sem_state);
        sem_destroy(&M->sync->sem_reader_mutex);
        for (unsigned i = 0; i < 9; ++i) {
            sem_destroy(&M->sync->sem_player_can_send[i]);
        }
    }
}
