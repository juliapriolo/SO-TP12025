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


static void exec_child_with_dims(const char *path, unsigned w, unsigned h) {
    char wbuf[32], hbuf[32], pathcpy[256];
    snprintf(wbuf, sizeof wbuf, "%u", w);
    snprintf(hbuf, sizeof hbuf, "%u", h);
    snprintf(pathcpy, sizeof pathcpy, "%s", path);
    char *argvv[] = {pathcpy, wbuf, hbuf, NULL};
    execv(pathcpy, argvv);
    perror("execv");
    _exit(127);
}

static void close_all_other_pipes_in_child(const Master *M, unsigned current_player) {
    for (unsigned k = 0; k < M->args.player_count; ++k) {
        if (k == current_player) continue; 
        
        if (M->players[k].pipe_rd >= 3)
            close(M->players[k].pipe_rd);
        if (M->players[k].pipe_wr >= 3)
            close(M->players[k].pipe_wr);
    }
}

static void init_player_info(Master *M, unsigned i, unsigned short x, unsigned short y) {
    PlayerInfo *p = &M->state->players[i];
    memset(p, 0, sizeof *p);
    snprintf(p->name, sizeof p->name, "user%u", i);
    p->score = 0;
    p->valid_moves = 0;
    p->invalid_moves = 0;
    p->x = x;
    p->y = y;
    p->blocked = false;
    p->pid = 0; 

    int idx = (int) y * (int) M->args.width + (int) x;
    M->state->board[idx] = -(int) i;
}

pid_t spawn_view(Master *M) {
    if (!M->args.view_path)
        return 0;

    pid_t pid = fork();
    if (pid < 0)
        die("fork(view): %s", strerror(errno));
    if (pid == 0) {
        exec_child_with_dims(M->args.view_path, M->args.width, M->args.height);
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

        init_player_info(M, i, px[i], py[i]);

        pid_t pid = fork();
        if (pid < 0)
            die("fork(player): %s", strerror(errno));
        if (pid == 0) {
            if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
                perror("dup2(player)");
                _exit(127);
            }
            close(pipefd[1]);
            close(pipefd[0]);

            close_all_other_pipes_in_child(M, i);
            exec_child_with_dims(M->args.player_paths[i], M->args.width, M->args.height);
        }
        M->players[i].pid = pid;
        M->state->players[i].pid = pid;
        close(M->players[i].pipe_wr);
        M->players[i].pipe_wr = -1;

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
