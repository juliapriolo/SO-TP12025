
// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/select.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#include "shm.h"
#include "config.h"
#include "state.h"
#include "sync.h"
#include "master_utils.h"
#include "sync_reader.h"
#include "sync_writer.h"

#include "args.h"
#include "cleanup.h"
#include "timing.h"
#include "sync_init.h"
#include "game.h"
#include "game_init.h"
#include "notify.h"
#include "proc.h"


int main(int argc, char **argv) {
	signal(SIGPIPE, SIG_IGN);

	Args args;
	parse_args(argc, argv, &args);

	srand(args.seed);

	GameShmData shm_data = create_game_shm(args.width, args.height, args.player_count);
	if (!shm_data.state || !shm_data.sync) {
		die("Failed to create game shared memory");
	}
	
	Master M = init_game_with_view(&args, &shm_data);
	
	GameState *state = M.state;
	GameSync *sync = M.sync;
	size_t state_bytes = M.state_bytes;

	if (finalize_game_setup(&M)) {
		finish_game_and_cleanup(&M, state, sync, state_bytes);
		return 0;
	}

	struct timeval tv_now;
	gettimeofday(&tv_now, NULL);
	uint64_t last_valid_ms = (uint64_t)tv_now.tv_sec * 1000ULL + (uint64_t)tv_now.tv_usec / 1000ULL;
	unsigned rr_next = 0;

	const uint64_t timeout_ms = (uint64_t) args.timeout_s * 1000ULL;

	while (1) {
		FdSetInfo fd_info = setup_fd_set(&M);

		if (fd_info.alive_count == 0) {
			set_finished_and_wake_all(&M);
			break;
		}

		TimeoutInfo timeout_info = calculate_timeout(last_valid_ms, timeout_ms);
		if (timeout_info.timeout_reached) {
			set_finished_and_wake_all(&M);
			break;
		}

		int ready = select(fd_info.maxfd + 1, &fd_info.rfds, NULL, NULL, &timeout_info.tv);
		if (ready < 0) {
			if (errno == EINTR)
				continue;
			perror("select");
			set_finished_and_wake_all(&M);
			break;
		}
		if (ready == 0) {
			continue;
		}

		for (unsigned k = 0; k < args.player_count; ++k) {
			unsigned i = (rr_next + k) % args.player_count;
			int fd = M.players[i].pipe_rd;
			if (fd < 0)
				continue;
			if (!FD_ISSET(fd, &fd_info.rfds))
				continue;

			MoveProcessResult move_result = process_player_move(&M, i);
			
			if (move_result.move_was_valid) {
				last_valid_ms = move_result.new_last_valid_ms;
			}

			if (move_result.game_ended) {
				set_finished_and_wake_all(&M);
				finish_game_and_cleanup(&M, state, sync, state_bytes);
				return 0;
			}

			rr_next = (i + 1) % args.player_count;
			break;
		}
	}

	finish_game_and_cleanup(&M, state, sync, state_bytes);
	return 0;
}