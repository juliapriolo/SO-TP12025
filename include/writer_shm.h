#ifndef SO_TP12025_WRITER_SHM_H
#define SO_TP12025_WRITER_SHM_H


#include <stdint.h>
#include <stddef.h>
#include "ipc.h"

// Configuración del writer (M0)
typedef struct {
    uint16_t width;      // >= 10
    uint16_t height;     // >= 10
    unsigned int seed;   // por defecto time(NULL)
} WriterConfig;

// Parseo de CLI: -w <width> -h <height> -s <seed>.
// Devuelve 0 si OK, -1 si error (y deja usage impreso).
int writer_parse_args(int argc, char** argv, WriterConfig* cfg);

// Inicializa el header mínimo de GameState con width/height/players_count/finished.
// No llena tablero.
void writer_init_header(GameState* st, uint16_t w, uint16_t h);

// Llena el tablero con recompensas 1..9 usando la seed dada (determinístico).
void writer_fill_board_random(GameState* st, unsigned int seed);

// Mensaje de verificación: dimensiones, bytes totales y primeras celdas.
void writer_print_summary(const GameState* st, size_t total_bytes, unsigned int sample_count);

#endif //SO_TP12025_WRITER_SHM_H