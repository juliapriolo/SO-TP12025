// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

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
	
	/* inicializar juego completo incluyendo vista */
	Master M = init_game_with_view(&args, &shm_data);
	
	GameState *state = M.state;
	GameSync *sync = M.sync;
	size_t state_bytes = M.state_bytes;

	/* finalizar inicialización del juego */
	if (finalize_game_setup(&M)) {
		finish_game_and_cleanup(&M, state, sync, state_bytes);
		return 0;
	}

	/*  bucle principal: select() + round-robin  */
	struct timeval tv_now;
	gettimeofday(&tv_now, NULL);
	uint64_t last_valid_ms = (uint64_t)tv_now.tv_sec * 1000ULL + (uint64_t)tv_now.tv_usec / 1000ULL;
	unsigned rr_next = 0; /* indice del proximo a intentar atender primero */

	/* transformar timeout_s a ms */
	const uint64_t timeout_ms = (uint64_t) args.timeout_s * 1000ULL;

	while (1) {
		/* armar fd_set con los pipes vivos */
		FdSetInfo fd_info = setup_fd_set(&M);

		if (fd_info.alive_count == 0) {
			set_finished_and_wake_all(&M);
			break;
		}

		/* calcular timeout para select */
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
			/* select timeout relativo: se chequea arriba en el loop */
			continue;
		}

		/* politica RR: buscamos el primer jugador con FD listo, empezando en rr_next */
		for (unsigned k = 0; k < args.player_count; ++k) {
			unsigned i = (rr_next + k) % args.player_count;
			int fd = M.players[i].pipe_rd;
			if (fd < 0)
				continue;
			if (!FD_ISSET(fd, &fd_info.rfds))
				continue;

			/* procesar movimiento del jugador */
			MoveProcessResult move_result = process_player_move(&M, i);
			
			/* actualizar timestamp si el movimiento fue válido */
			if (move_result.move_was_valid) {
				last_valid_ms = move_result.new_last_valid_ms;
			}

			/* verificar si el juego terminó */
			if (move_result.game_ended) {
				set_finished_and_wake_all(&M);
				finish_game_and_cleanup(&M, state, sync, state_bytes);
				return 0;
			}

			/* avanzar round-robin y salir */
			rr_next = (i + 1) % args.player_count;
			break;
		}
	}

	finish_game_and_cleanup(&M, state, sync, state_bytes);
	return 0;
}