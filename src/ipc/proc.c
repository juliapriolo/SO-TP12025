// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "proc.h"
#include "timing.h"

pid_t spawn_view(Master *M) {
    if (!M->args.view_path)
        return 0;

    pid_t pid = fork();
    if (pid < 0)
        die("fork(view): %s", strerror(errno));
    if (pid == 0) {
        /* hijo: exec view */
        char wbuf[32], hbuf[32];
        char view_path_copy[256];
        snprintf(wbuf, sizeof wbuf, "%u", M->args.width);
        snprintf(hbuf, sizeof hbuf, "%u", M->args.height);
        snprintf(view_path_copy, sizeof view_path_copy, "%s", M->args.view_path);
        char *argvv[] = {view_path_copy, wbuf, hbuf, NULL};
        execv(view_path_copy, argvv);
        perror("execv(view)");
        _exit(127);
    }
    return pid;
}

void spawn_players(Master *M, unsigned short px[MAX_PLAYERS], unsigned short py[MAX_PLAYERS]) {
    for (unsigned k = 0; k < MAX_PLAYERS; ++k) {
        M->players[k].pipe_rd = -1;
        M->players[k].pipe_wr = -1;
    }

    for (unsigned i = 0; i < M->args.player_count; ++i) {
        int pipefd[2] = {-1, -1};
        if (pipe(pipefd) == -1)
            die("pipe(): %s", strerror(errno));

        M->players[i].pipe_rd = pipefd[0];
        M->players[i].pipe_wr = pipefd[1];
        M->players[i].path = M->args.player_paths[i];

        /* inicializar jugador en el estado (nombre, pos, etc) */
        PlayerInfo *p = &M->state->players[i];
        memset(p, 0, sizeof *p);
        snprintf(p->name, sizeof p->name, "user%u", i);
        p->score = 0;
        p->valid_moves = 0;
        p->invalid_moves = 0;
        p->x = px[i];
        p->y = py[i];
        p->blocked = false;
        p->pid = 0; /* se setea tras fork */

        /* marcar su celda inicial como capturada por -id (0..8). La celda inicial no otorga puntos. */
        int idx = (int) py[i] * (int) M->args.width + (int) px[i];
        M->state->board[idx] = -(int) i;

        pid_t pid = fork();
        if (pid < 0)
            die("fork(player): %s", strerror(errno));
        if (pid == 0) {
            /* hijo (jugador): su stdout debe ser el lado de escritura del pipe */
            if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
                perror("dup2(player)");
                _exit(127);
            }
            close(pipefd[1]);
            close(pipefd[0]);

            for (unsigned k = 0; k < M->args.player_count; ++k) {
                if (M->players[k].pipe_rd >= 3)
                    close(M->players[k].pipe_rd);
                if (M->players[k].pipe_wr >= 3)
                    close(M->players[k].pipe_wr);
            }
            if (M->view.pid > 0) { /* nada especial que cerrar del view */ }

            /* ejecutar binario del jugador */
            char wbuf[32], hbuf[32];
            char player_path_copy[256];
            snprintf(wbuf, sizeof wbuf, "%u", M->args.width);
            snprintf(hbuf, sizeof hbuf, "%u", M->args.height);
            snprintf(player_path_copy, sizeof player_path_copy, "%s", M->args.player_paths[i]);
            char *argvp[] = {player_path_copy, wbuf, hbuf, NULL};
            execv(player_path_copy, argvp);
            perror("execv(player)");
            _exit(127);
        }
        /* padre */
        M->players[i].pid = pid;
        M->state->players[i].pid = pid;
        /* el master nunca escribe al jugador; cerramos el WR para poder detectar EOF si el jugador muere */
        close(M->players[i].pipe_wr);
        M->players[i].pipe_wr = -1;

        /* hacer no bloqueante el read-end para usar select() seguro */
        int flags = fcntl(M->players[i].pipe_rd, F_GETFL, 0);
        (void) fcntl(M->players[i].pipe_rd, F_SETFL, flags | O_NONBLOCK);
    }
}

void print_child_status(pid_t pid, int status, const char *label, const PlayerInfo *pinfo_or_null) {
    if (WIFEXITED(status)) {
        int code = WEXITSTATUS(status);
        if (pinfo_or_null) {
            fprintf(stderr, "%s pid=%d exit=%d score=%u\n", label, (int) pid, code, pinfo_or_null->score);
        } else {
            fprintf(stderr, "%s pid=%d exit=%d\n", label, (int) pid, code);
        }
    } else if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        if (pinfo_or_null) {
            fprintf(stderr, "%s pid=%d signal=%d score=%u\n", label, (int) pid, sig, pinfo_or_null->score);
        } else {
            fprintf(stderr, "%s pid=%d signal=%d\n", label, (int) pid, sig);
        }
    } else {
        if (pinfo_or_null) {
            fprintf(stderr, "%s pid=%d (status=0x%x) score=%u\n", label, (int) pid, status, pinfo_or_null->score);
        } else {
            fprintf(stderr, "%s pid=%d (status=0x%x)\n", label, (int) pid, status);
        }
    }
}
