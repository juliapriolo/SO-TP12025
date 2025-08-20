#ifndef SO_TP12025_IPC_H
#define SO_TP12025_IPC_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <semaphore.h>
#include <stddef.h>

// -------------------------
// Nombres de objetos IPC
// -------------------------
#define SHM_STATE_NAME "/game_state"   // Estado del juego (tablero incluido)
#define SHM_SYNC_NAME  "/game_sync"    // Semáforos y contadores de sync

// -------------------------
// Parámetros del juego
// -------------------------
#define MAX_PLAYERS 9
#define MAX_NAME_LEN 16 // Longitud máxima del nombre de jugador

// Direcciones válidas de movimiento
typedef struct {
    int dx; // delta x
    int dy; // delta y
} Dir2D;

static const Dir2D DIRS8[8] = {
    { 0, -1}, { 1, -1}, { 1,  0}, { 1,  1},
    { 0,  1}, {-1,  1}, {-1,  0}, {-1, -1}
};

// -------------------------
// Estructuras en /game_state
// -------------------------
// Rango por celda: 1..9 = libre con recompensa; <=0 = capturada por jugador i
// con valor -i (i en 1..MAX_PLAYERS).
typedef struct {
    char      name[MAX_NAME_LEN];    // nombre del jugador
    uint32_t  score;         // puntaje
    uint32_t  invalid_moves; // solicitudes inválidas
    uint32_t  valid_moves;   // solicitudes válidas
    uint16_t  x, y;          // Coordenadas x e y en el tablero
    pid_t     pid;           // PID del proceso jugador
    bool      blocked;       // true si quedó sin movimientos o EOF
} PlayerInfo;

typedef struct {
    uint16_t  width;         // Ancho del tablero
    uint16_t  height;        // Alto del tablero
    uint32_t  players_count; // Cantidad de jugadores
    PlayerInfo players[MAX_PLAYERS];
    bool      finished;      // Indica si el juego terminó
    int32_t   board[]; // Puntero al comienzo del tablero. fila-0, fila-1, ..., fila-n-1
} GameState;

// -------------------------
// Estructuras en /game_sync
// -------------------------
// Semáforos anónimos:
// A: master -> view    (hay cambios)
// B: view   -> master  (impreso)
// C/D/E/F: lectores-escritor sin inanición del escritor
// G[i]: permiso/ack por jugador (un envío por vez)

typedef struct {
    sem_t notify_view;   // El máster le indica a la vista que hay cambios por imprimir
    sem_t view_done;     // La vista le indica al máster que terminó de imprimir

    sem_t writer_gate;   // Mutex para evitar inanición del máster al acceder al estado
    sem_t state_mutex;   // Mutex para el estado del juego
    sem_t read_count_mx; // Mutex para la siguiente variable
    uint32_t read_count; // Cantidad de jugadores leyendo el estado

    sem_t player_turn[MAX_PLAYERS]; // Le indican a cada jugador que puede enviar 1 movimiento
} SyncState;

#endif //SO_TP12025_IPC_H