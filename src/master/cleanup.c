// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#define _POSIX_C_SOURCE 200809L
#include "cleanup.h"
#include "config.h"
#include "game.h"
#include "proc.h"
#include "shm.h"
#include "timing.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static pid_t waitpid_blocking(pid_t pid, int *status) {
	pid_t result;
	do {
		result = waitpid(pid, status, 0);
	} while (result == -1 && errno == EINTR);
	return result;
}

static void close_fd_if_open(int *fd) {
	if (*fd >= 0) {
		close(*fd);
		*fd = -1;
	}
}

void finish_game_and_cleanup(Master *M, GameState *state, GameSync *sync, size_t state_bytes) {
	if (M->view.pid > 0)
		(void) sem_post(&M->sync->sem_master_to_view);

	if (M->view.pid > 0) {
		int status;
		waitpid_blocking(M->view.pid, &status);
		print_child_status(M->view.pid, status, "view", NULL);
	}

	printf("Juego terminado! \n");
	sleep_ms(1000);
	print_podium(state);

	int status;
	for (unsigned i = 0; i < M->args.player_count; ++i) {
		pid_t pid = M->players[i].pid;
		if (pid <= 0)
			continue;
		waitpid_blocking(pid, &status);
		print_child_status(pid, status, "player", &state->players[i]);
	}

	cleanup_master(M);

	shm_unmap(sync, sizeof(GameSync));
	shm_unmap(state, state_bytes);

	shm_delete(SHM_SYNC_NAME);
	shm_delete(SHM_STATE_NAME);
}

void cleanup_master(Master *M) {
	// cerrar todos los file descriptors de jugadores
	for (unsigned i = 0; i < M->args.player_count; ++i) {
		close_fd_if_open(&M->players[i].pipe_rd);
		close_fd_if_open(&M->players[i].pipe_wr);
	}

	// cerrar file descriptors de la vista
	close_fd_if_open(&M->view.pipe_rd);
	close_fd_if_open(&M->view.pipe_wr);

	// destruir todos los semáforos 
	if (M->sync) {
		sem_destroy(&M->sync->sem_master_to_view);
		sem_destroy(&M->sync->sem_view_to_master);
		sem_destroy(&M->sync->sem_turnstile);
		sem_destroy(&M->sync->sem_state);
		sem_destroy(&M->sync->sem_reader_mutex);
		for (unsigned i = 0; i < MAX_PLAYERS; ++i) {
			sem_destroy(&M->sync->sem_player_can_send[i]);
		}
	}
}
