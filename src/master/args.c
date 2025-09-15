// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#define _POSIX_C_SOURCE 200809L
#include "args.h"
#include "timing.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

void usage(const char *argv0) {
	fprintf(stderr,
			"Uso: %s [-w width] [-h height] [-d delay_ms] [-t timeout_s] [-s seed] "
			"[-v view] -p player1 [player2 ... player9]\n",
			argv0);
}

void parse_args(int argc, char **argv, Args *out) {
	*out = (Args){.width = 10,
				  .height = 10,
				  .delay_ms = 200,
				  .timeout_s = 10,
				  .seed_set = 0,
				  .seed = 0,
				  .view_path = NULL,
				  .player_count = 0};

	int opt;
	while ((opt = getopt(argc, argv, "w:h:d:t:s:v:p:")) != -1) {
		switch (opt) {
			case 'w':
				out->width = (unsigned) atoi(optarg);
				break;
			case 'h':
				out->height = (unsigned) atoi(optarg);
				break;
			case 'd':
				out->delay_ms = strtol(optarg, NULL, 10);
				break;
			case 't':
				out->timeout_s = (unsigned) atoi(optarg);
				break;
			case 's':
				out->seed = (unsigned) strtoul(optarg, NULL, 10);
				out->seed_set = 1;
				break;
			case 'v':
				out->view_path = optarg;
				break;
			case 'p': {
				out->player_count = 0;
				out->player_paths[out->player_count++] = optarg;
				while (optind < argc && argv[optind][0] != '-') {
					if (out->player_count >= MAX_PLAYERS)
						die("Demasiados jugadores (max %d).", MAX_PLAYERS);
					out->player_paths[out->player_count++] = argv[optind++];
				}
				break;
			}
			default:
				usage(argv[0]);
				exit(EXIT_FAILURE);
		}
	}

	if (out->width < 10 || out->height < 10)
		die("width/height mínimo = 10");
	if (out->player_count == 0)
		die("Debe especificar al menos 1 jugador con -p");
	if (!out->seed_set)
		out->seed = (unsigned) time(NULL);
}
