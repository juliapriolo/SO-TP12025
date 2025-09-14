// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#define _POSIX_C_SOURCE 200809L
#include "player_utils.h"
#include "state.h" 
#include "game.h"
#include "directions.h"
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include "timing.h"

// Función auxiliar simple para contar movimientos futuros
static int count_future_moves(GameState *st, unsigned short x, unsigned short y) {
    static const int dx[] = {0, 1, 1, 1, 0, -1, -1, -1};
    static const int dy[] = {-1, -1, 0, 1, 1, 1, 0, -1};
    
    int count = 0;
    for (int i = 0; i < 8; ++i) {
        int nx = (int)x + dx[i];
        int ny = (int)y + dy[i];
        
        if (nx >= 0 && ny >= 0 && nx < (int)st->width && ny < (int)st->height) {
            int idx = ny * (int)st->width + nx;
            if (st->board[idx] > 0) count++;
        }
    }
    return count;
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
    static const int dx[] = {0, 1, 1, 1, 0, -1, -1, -1};
    static const int dy[] = {-1, -1, 0, 1, 1, 1, 0, -1};
    
    int best_dir = -1;
    int best_value = -1;
    int fallback_dir = -1;      // Cualquier movimiento válido
    int fallback_value = -1;
    
    // Buscar el mejor movimiento, evitando callejones sin salida
    for (int dir = 0; dir < 8; ++dir) {
        int nx = (int)x + dx[dir];
        int ny = (int)y + dy[dir];
        
        // Verificar límites
        if (nx < 0 || ny < 0 || nx >= (int)st->width || ny >= (int)st->height)
            continue;
            
        int idx = ny * (int)st->width + nx;
        
        // Solo considerar celdas libres (valor > 0)
        if (st->board[idx] > 0) {
            // Guardar cualquier movimiento válido como fallback
            if (fallback_dir == -1 || st->board[idx] > fallback_value) {
                fallback_dir = dir;
                fallback_value = st->board[idx];
            }
            
            // Verificar si este movimiento llevaría a un callejón sin salida
            int future_moves = count_future_moves(st, (unsigned short)nx, (unsigned short)ny);
            
            // Si el movimiento lleva a un callejón sin salida, saltarlo
            // PERO solo si tenemos otras opciones disponibles
            if (future_moves == 0) {
                continue; // Evitar este movimiento por ahora
            }
            
            // Tomar el movimiento con mayor valor que no sea callejón sin salida
            if (st->board[idx] > best_value) {
                best_value = st->board[idx];
                best_dir = dir;
            }
        }
    }
    
    // Si encontramos un buen movimiento (no callejón sin salida), usarlo
    if (best_dir != -1) {
        return best_dir;
    }
    
    // Si no encontramos movimientos "seguros", usar cualquier movimiento válido
    // (Es mejor moverse a un callejón sin salida que no moverse)
    return fallback_dir;
}