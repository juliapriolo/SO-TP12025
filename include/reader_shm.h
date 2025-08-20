#ifndef SO_TP12025_READER_SHM_H
#define SO_TP12025_READER_SHM_H

#include <stdint.h>
#include <stddef.h>
#include "ipc.h"

// Configuración del reader (M0)
typedef struct {
    uint16_t preview_rows; // cuántas filas imprimir (ej. min(3, height))
} ReaderConfig;

// (Opcional) fija defaults de config (p.ej., preview_rows = 3).
static inline void reader_default_config(ReaderConfig* c) {
    c->preview_rows = 3;
}

// Imprime resumen de estado (WxH, players_count, finished).
void reader_print_state_summary(const GameState* st);

// Imprime 'rows' filas del tablero (clipeado a height).
void reader_print_board_rows(const GameState* st, uint16_t rows);

#endif //SO_TP12025_READER_SHM_H