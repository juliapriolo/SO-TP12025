#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include "ipc.h"
#include "reader_shm.h"


//Cuantas filas imprimir:
static inline uint16_t clamp_rows(const GameState* st, uint16_t rows) {
    if (!st) return 0;
    return (rows > st->height) ? st->height : rows;
}

// Convierte una celda del tablero a un token de impresión.
static void cell_to_str(int32_t v, char out[4]) {
    if (v > 0) {
        // recompensa 1..9
        snprintf(out, 4, "%2d", v);
    } else if (v <= -1) {
        int owner = -v;                // -1 -> 1, -2 -> 2, etc.
        snprintf(out, 4, "P%d", owner);
    } else { 
        snprintf(out, 4, "%2d", v);
    }
}

//Imprimir un snapshot del estado del juego.
//Recibe el puntero a la estructura GameState
void reader_print_state_summary(const GameState* st) {
    if (!st) {
        fprintf(stderr, "[reader] GameState nulo\n");
        return;
    }

    printf("=== Estado del juego ===\n");
    printf("Tablero: %ux%u\n", (unsigned)st->width, (unsigned)st->height);
    printf("Jugadores: %u\n", (unsigned)st->players_count);
    printf("Terminado: %s\n", st->finished ? "si" : "no");

    // Listado de jugadores
    for (uint32_t i = 0; i < st->players_count && i < MAX_PLAYERS; i++) {
        const PlayerInfo* p = &st->players[i];
        printf("  J%u %-*s | pos=(%u,%u) | score=%u | v=%u inv=%u | %s\n",
               (unsigned)(i+1),
               MAX_NAME_LEN, p->name,
               (unsigned)p->x, (unsigned)p->y,
               (unsigned)p->score,
               (unsigned)p->valid_moves, (unsigned)p->invalid_moves,
               p->blocked ? "blocked" : "active");
    }
}

//Mapa visual del tablero
void reader_print_board_rows(const GameState* st, uint16_t rows) {
    if (!st) {
        fprintf(stderr, "[reader] GameState nulo\n");
        return;
    }
    if (st->width == 0 || st->height == 0) {
        printf("[reader] Tablero vacío (%ux%u)\n",
               (unsigned)st->width, (unsigned)st->height);
        return;
    }

    uint16_t r = clamp_rows(st, rows);

    printf("=== Tablero (primeras %u filas) ===\n", (unsigned)r);
    printf("    "); 
    for (uint16_t x = 0; x < st->width; x++) {
        printf("%2u ", (unsigned)x);
    }
    printf("\n");

    for (uint16_t y = 0; y < r; y++) {
        printf("%3u ", (unsigned)y); 
        for (uint16_t x = 0; x < st->width; x++) {
            int32_t v = st->board[y * st->width + x];
            char tok[4];
            cell_to_str(v, tok);
            printf("%2s ", tok);
        }
        printf("\n");
    }
}
