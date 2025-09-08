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
#include "notify.h"
#include "proc.h"


int main(int argc, char **argv) {
	signal(SIGPIPE, SIG_IGN);

	Args args;
	parse_args(argc, argv, &args);

	srand(args.seed);

	/* crear shm estado y sync */
	size_t state_bytes = gamestate_bytes((uint16_t) args.width, (uint16_t) args.height);
    GameState *state = (GameState *) shm_create(SHM_STATE_NAME, state_bytes, O_RDWR);
	if (!state)
		die("shm_create(/game_state) failed");

    GameSync *sync = (GameSync *) shm_create(SHM_SYNC_NAME, sizeof(GameSync), O_RDWR);
	if (!sync)
		die("shm_create(/game_sync) failed");

	init_sync(sync, args.player_count);

	/* inicializar estructuras */
	writer_enter(sync);
	memset(state, 0, state_bytes);
	state->width = (unsigned short) args.width;
	state->height = (unsigned short) args.height;
	state->player_count = args.player_count;
	state->finished = false;
	init_board(state, args.width, args.height);
	writer_exit(sync);

	/* colocar jugadores en posiciones "justas" */
	unsigned short px[MAX_PLAYERS], py[MAX_PLAYERS];
	initial_positions(args.width, args.height, args.player_count, px, py);

	/* lanzar vista (si hay), luego jugadores */
	Master M = {.args = args,
				.state = state,
				.sync = sync,
				.state_bytes = state_bytes,
				.view = {.pid = 0, .pipe_rd = -1, .pipe_wr = -1, .path = args.view_path}};

	if (args.view_path) {
		pid_t vpid = spawn_view(&M);
		M.view.pid = vpid;
	}

	spawn_players(&M, px, py);

	writer_enter(sync);
	for (unsigned i = 0; i < args.player_count; ++i) {
		if (!player_can_move(state, i)) {
			state->players[i].blocked = true;
		}
	}

	/* si todos empiezan bloqueados, terminar inmediatamente */
	unsigned initial_can_move = count_players_that_can_move(state);
	if (initial_can_move == 0) {
		state->finished = true;
	}
	writer_exit(sync);

	/* primera notif a la vista para dibujar estado inicial */
	notify_view_and_delay_if_any(&M);

	if (state->finished) {
		set_finished_and_wake_all(&M);
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

	for (;;) {
		/* armar fd_set con los pipes vivos */
		fd_set rfds;
		FD_ZERO(&rfds);
		int maxfd = -1;
		unsigned alive = 0;

		for (unsigned i = 0; i < args.player_count; ++i) {
			int fd = M.players[i].pipe_rd;
			if (fd >= 0) {
				FD_SET(fd, &rfds);
				if (fd > maxfd)
					maxfd = fd;
				alive++;
			}
		}

		/* si ya no hay pipes vivos, finalizamos */
		if (alive == 0) {
			/* termino todo: nadie puede enviar mas */
			set_finished_and_wake_all(&M);
			break;
		}

		/* calcular cuánto falta para el timeout relativo a la ultima jugada valida */
		gettimeofday(&tv_now, NULL);
		uint64_t now = (uint64_t)tv_now.tv_sec * 1000ULL + (uint64_t)tv_now.tv_usec / 1000ULL;
		uint64_t elapsed = now - last_valid_ms;
		if (elapsed >= timeout_ms) {
			set_finished_and_wake_all(&M);
			break;
		}
		uint64_t remain_ms = timeout_ms - elapsed;
		struct timeval tv;
		tv.tv_sec = (time_t) (remain_ms / 1000ULL);
		tv.tv_usec = (suseconds_t) ((remain_ms % 1000ULL) * 1000ULL);

		int ready = select(maxfd + 1, &rfds, NULL, NULL, &tv);
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
		bool processed_one = false;
		for (unsigned k = 0; k < args.player_count; ++k) {
			unsigned i = (rr_next + k) % args.player_count;
			int fd = M.players[i].pipe_rd;
			if (fd < 0)
				continue;
			if (!FD_ISSET(fd, &rfds))
				continue;

			/* Leer exactamente 1 byte si hay, detectar EOF */
			unsigned char move;
			ssize_t rd = read(fd, &move, 1);
			if (rd == 0) {
				/* EOF -> jugador bloqueado */
				writer_enter(sync);
				state->players[i].blocked = true;
				writer_exit(sync);
				close(M.players[i].pipe_rd);
				M.players[i].pipe_rd = -1;
				/* no hay "allow_next_send" porque ya no vive */
				continue;
			}
			else if (rd < 0) {
				if (errno == EAGAIN || errno == EWOULDBLOCK)
					continue;
				perror("read(pipe)");
				/* tratamos como bloqueado igualmente */
				writer_enter(sync);
				state->players[i].blocked = true;
				writer_exit(sync);
				close(M.players[i].pipe_rd);
				M.players[i].pipe_rd = -1;
				continue;
			}

			/* tenemos un byte: aplicar movimiento bajo lock de escritor */
			bool was_valid;
			bool all_blocked = false;
			writer_enter(sync);
			was_valid = apply_move_locked(state, i, move);
			unsigned players_that_can_move = count_players_that_can_move(state);
			all_blocked = (players_that_can_move == 0);
			writer_exit(sync);

			/* notificar a vista solo si hubo cambio de estado */
			if (was_valid) {
				notify_view_and_delay_if_any(&M);
				gettimeofday(&tv_now, NULL);
				last_valid_ms = (uint64_t)tv_now.tv_sec * 1000ULL + (uint64_t)tv_now.tv_usec / 1000ULL;
			}

			if (all_blocked) {
				set_finished_and_wake_all(&M);
				finish_game_and_cleanup(&M, state, sync, state_bytes);
				return 0;
			}

			/* desbloquear al jugador para que pueda mandar otro movimiento */
			allow_next_send(&M, i);

			/* avanzar round-robin y salir (una sola solicitud por iteracion) */
			rr_next = (i + 1) % args.player_count;
			processed_one = true;
			break;
		}

		/* si no procesamos ninguno (por ej los listos dieron error/EOF), volvemos al select */
		(void) processed_one;
	}

	/* Si llegamos aquí, el juego terminó normalmente*/
	finish_game_and_cleanup(&M, state, sync, state_bytes);
	return 0;
}
