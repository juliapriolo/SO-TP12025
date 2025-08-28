// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#define _POSIX_C_SOURCE 200809L
#include "player_utils.h"
#include "state.h" 
#include <errno.h>
#include <stdint.h>
#include <unistd.h>

// Direcciones (0..7): 0=arriba y sentido horario
static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};

void sleep_ms(long ms) {
    struct timespec ts = {.tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000L};
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR) { /* retry */ }
}

int find_self_index(GameState *st) {
    pid_t me = getpid();
    for (unsigned i = 0; i < st->player_count && i < 9; ++i) {
        if (st->players[i].pid == me)
            return (int)i;
    }
    return -1;
}

int pick_direction(GameState *st, unsigned short x, unsigned short y) {
    int best_dir = -1;
    int best_reward = -1;
    for (int dir = 0; dir < 8; ++dir) {
        int nx = (int)x + dx[dir];
        int ny = (int)y + dy[dir];
        if (nx < 0 || ny < 0 || nx >= st->width || ny >= st->height)
            continue;
        int32_t cell = st->board[ny * st->width + nx];
        if (cell > 0 && cell > best_reward) {
            best_reward = cell;
            best_dir = dir;
        }
    }
    return best_dir;
}
