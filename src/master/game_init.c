// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#define _POSIX_C_SOURCE 200809L

#include "game_init.h"
#include "config.h"
#include "game.h"
#include "notify.h"
#include "proc.h"
#include "shm.h"
#include "sync_init.h"
#include "sync_writer.h"
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

GameShmData create_game_shm(unsigned width, unsigned height, unsigned player_count) {
	GameShmData result = {0};

	result.state_bytes = gamestate_bytes((uint16_t) width, (uint16_t) height);
	result.state = (GameState *) shm_create(SHM_STATE_NAME, result.state_bytes, O_RDWR);
	if (!result.state) {
		fprintf(stderr, "shm_create(/game_state) failed\n");
		return result;
	}

	result.sync = (GameSync *) shm_create(SHM_SYNC_NAME, sizeof(GameSync), O_RDWR);
	if (!result.sync) {
		fprintf(stderr, "shm_create(/game_sync) failed\n");
		shm_unmap(result.state, result.state_bytes);
		result.state = NULL;
		return result;
	}

	init_sync(result.sync, player_count);

	writer_enter(result.sync);
	memset(result.state, 0, result.state_bytes);
	result.state->width = (unsigned short) width;
	result.state->height = (unsigned short) height;
	result.state->player_count = player_count;
	result.state->finished = false;
	init_board(result.state, width, height);
	writer_exit(result.sync);

	return result;
}

Master init_game_with_view(const Args *args, const GameShmData *shm_data) {
	unsigned short px[MAX_PLAYERS], py[MAX_PLAYERS];
	initial_positions(args->width, args->height, args->player_count, px, py);

	Master M = {.args = *args,
				.state = shm_data->state,
				.sync = shm_data->sync,
				.state_bytes = shm_data->state_bytes,
				.view = {.pid = 0, .pipe_rd = -1, .pipe_wr = -1, .path = args->view_path}};

	if (args->view_path) {
		pid_t vpid = spawn_view(&M);
		M.view.pid = vpid;
	}

	spawn_players(&M, px, py);

	return M;
}

// Función para finalizar la inicialización del juego
// Retorna true si el juego terminó temprano (todos bloqueados), false si continúa
bool finalize_game_setup(Master *M) {
	writer_enter(M->sync);
	for (unsigned i = 0; i < M->args.player_count; ++i) {
		if (!player_can_move(M->state, i)) {
			M->state->players[i].blocked = true;
		}
	}

	unsigned initial_can_move = count_players_that_can_move(M->state);
	if (initial_can_move == 0) {
		M->state->finished = true;
	}
	writer_exit(M->sync);

	notify_view_and_delay_if_any(M);

	if (M->state->finished) {
		set_finished_and_wake_all(M);
		return true;
	}
	return false;
}
