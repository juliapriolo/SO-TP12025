#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ipc.h"
#include "writer_shm.h"
#include "shm.h"

int main(int argc, char** argv) {
    WriterConfig cfg;
    if (writer_parse_args(argc, argv, &cfg) != 0) {
        return EXIT_FAILURE; // Error en argumentos
    }

    size_t total_bytes = sizeof(GameState)
                    + (size_t)cfg.width * (size_t)cfg.height * sizeof(int32_t);

    int fd = shm_create_fd(SHM_STATE_NAME, total_bytes, 0600);
    if (fd == -1) {
        perror("shm_create_fd");
        return EXIT_FAILURE;
    }
    GameState* st = (GameState*)shm_map(fd, total_bytes, PROT_READ | PROT_WRITE);
    if (st == MAP_FAILED) {
        perror("shm_map");
        close(fd); // en error, cerrar el fd abierto
        shm_unlink_safe(SHM_STATE_NAME);
        return EXIT_FAILURE;
    }

    // Inicializa el header del GameState
    writer_init_header(st, cfg.width, cfg.height);

    // Llena el tablero con valores aleatorios
    writer_fill_board_random(st, cfg.seed);

    // Imprime resumen del estado
    writer_print_summary(st, total_bytes, 10);

    if (shm_unmap_and_close(st, total_bytes, fd) == -1) {
        perror("shm_unmap_and_close");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

// Funciones auxiliares:
static void print_usage(const char* prog) {
    fprintf(stderr,
        "Uso: %s -w <width>=10 -h <height>=10 [-s <seed>=time(NULL)]\n"
        "  -w, -h: dimensiones del tablero (min 10x10)\n"
        "  -s    : semilla para tablero aleatorio (entero sin signo)\n",
        prog ? prog : "master");
}

static int parse_uint16(const char* s, uint16_t* out) {
    char* end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0' || v < 0 || v > 65535) return -1;
    *out = (uint16_t)v;
    return 0;
}

static int parse_uint(const char* s, unsigned int* out) {
    char* end = NULL;
    unsigned long v = strtoul(s, &end, 10);
    if (end == s || *end != '\0') return -1;
    *out = (unsigned int)v;
    return 0;
}


int writer_parse_args(int argc, char** argv, WriterConfig* cfg) {
    if (!cfg) {
        fprintf(stderr, "writer_parse_args: cfg nulo\n");
        return -1;
    }

    // Defaults
    cfg->width  = 10;
    cfg->height = 10;
    cfg->seed   = (unsigned int)time(NULL);

    // Parse simple estilo "-k value"
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-w") == 0) {
            if (i + 1 >= argc || parse_uint16(argv[i+1], &cfg->width) != 0) {
                fprintf(stderr, "Error en -w (ancho)\n");
                print_usage(argv[0]);
                return -1;
            }
            i++;
        } else if (strcmp(argv[i], "-h") == 0) {
            if (i + 1 >= argc || parse_uint16(argv[i+1], &cfg->height) != 0) {
                fprintf(stderr, "Error en -h (alto)\n");
                print_usage(argv[0]);
                return -1;
            }
            i++;
        } else if (strcmp(argv[i], "-s") == 0) {
            if (i + 1 >= argc || parse_uint(argv[i+1], &cfg->seed) != 0) {
                fprintf(stderr, "Error en -s (seed)\n");
                print_usage(argv[0]);
                return -1;
            }
            i++;
        } else {
            fprintf(stderr, "Flag desconocida: %s\n", argv[i]);
            print_usage(argv[0]);
            return -1;
        }
    }

    // Validaciones del enunciado
    if (cfg->width < 10 || cfg->height < 10) {
        fprintf(stderr, "Dimensiones mínimas: 10x10 (recibido %ux%u)\n",
                (unsigned)cfg->width, (unsigned)cfg->height);
        print_usage(argv[0]);
        return -1;
    }
    return 0;
}

void writer_init_header(GameState* st, uint16_t w, uint16_t h) {
    if (!st) return;
    st->width         = w;
    st->height        = h;
    st->players_count = 0;      // el master la seteará luego
    st->finished      = false;
    memset(st->players, 0, sizeof st->players);
}

void writer_fill_board_random(GameState* st, unsigned int seed) {
    if (!st) return;
    const uint32_t W = st->width;
    const uint32_t H = st->height;
    if (W == 0 || H == 0) return;

    srand(seed);

    // Llena todas las celdas con 1..9 
    for (uint32_t y = 0; y < H; y++) {
        for (uint32_t x = 0; x < W; x++) {
            st->board[y * W + x] = (int32_t)((rand() % 9) + 1);
        }
    }
}

void writer_print_summary(const GameState* st, size_t total_bytes, unsigned int sample_count) {
    if (!st) {
        fprintf(stderr, "[writer] GameState nulo\n");
        return;
    }

    const uint32_t W = st->width;
    const uint32_t H = st->height;

    printf("=== Resumen GameState ===\n");
    printf("Dimensiones: %ux%u\n", (unsigned)W, (unsigned)H);
    printf("Jugadores  : %u\n", (unsigned)st->players_count);
    printf("Finished   : %s\n", st->finished ? "si" : "no");
    printf("Memoria    : %zu bytes totales\n", total_bytes);

    // Muestra algunas celdas para verificar llenado
    if (W > 0 && H > 0 && st->board) {
        printf("Muestreo de %u celdas: ", sample_count);
        unsigned int shown = 0;
        for (uint32_t i = 0; i < W*H && shown < sample_count; i++) {
            printf("%d ", st->board[i]);
            shown++;
        }
        if (shown == 0) printf("(sin celdas)");
        printf("\n");
    }
}
