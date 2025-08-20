#ifndef SO_TP12025_SHM_H
#define SO_TP12025_SHM_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>  // mode_t
#include <sys/mman.h>   // PROT_READ/WRITE
#include <fcntl.h>     // shm_open, O_CREAT, O_EXCL, O_RDWR
#include <unistd.h>    // close, ftruncate, munmap
#include <sys/stat.h>  // shm_open, mode_t
#include <errno.h>  // MAP_FAILED

#include "ipc.h"        // GameState / SyncState, nombres de shm

// =======================================================
// Capa genérica (syscalls de shm, sin saber de GameState)
// =======================================================

// Crea un objeto shm (O_CREAT|O_EXCL|O_RDWR), lo trunca a 'size' y devuelve fd.
// mode típico: 0600. Retorna -1 en error.
int   shm_create_fd(const char* name, size_t size, mode_t mode);

// Abre un objeto shm existente con las flags dadas (O_RDONLY u O_RDWR).
// Retorna -1 en error.
int   shm_open_fd(const char* name, int oflag);

// Mapea 'size' bytes con 'prot' (PROT_READ o PROT_READ|PROT_WRITE), siempre MAP_SHARED.
// Devuelve puntero o MAP_FAILED.
void* shm_map(int fd, size_t size, int prot);

// Desmapea y cierra fd. Retorna 0 si ambos pasos fueron OK, -1 si falló alguno.
int   shm_unmap_and_close(void* addr, size_t size, int fd);

// Unlink “seguro”: intenta shm_unlink y devuelve 0 si ok, -1 si falla.
int   shm_unlink_safe(const char* name);

// =======================================================
// Capa específica de /game_state (tamaño variable por FAM)
// =======================================================

// Tamaño total a mapear para GameState con tablero W×H.
static inline size_t game_state_size(uint16_t w, uint16_t h) {
    return sizeof(GameState) + (size_t)w * (size_t)h * sizeof(int32_t);
}

// Crea /game_state, lo trunca al tamaño FAM y lo mapea RW.
// Devuelve puntero y deja el fd en out_fd (si no es NULL).
GameState* shm_state_create(uint16_t w, uint16_t h, int* out_fd);

// Abre /game_state existente en RW (p.ej. para master).
GameState* shm_state_open_rw(int* out_fd, size_t* out_size);

// Abre /game_state existente en RO (p.ej. para view/player o reader_shm).
const GameState* shm_state_open_ro(int* out_fd, size_t* out_size);

// Desmapea/cierra /game_state (recibe size para el munmap).
int shm_state_close(const void* addr, size_t total_size, int fd);

// =======================================================
// Capa específica de /game_sync (tamaño fijo + sem_init)
// =======================================================

// Crea /game_sync, lo trunca a sizeof(SyncState), lo mapea RW e inicializa
// TODOS los semáforos (A..G) y read_count. Devuelve puntero y out_fd si aplica.
SyncState* shm_sync_create(int* out_fd);

// Abre /game_sync existente (normalmente RW).
SyncState* shm_sync_open_rw(int* out_fd);

// Destruye semáforos (sem_destroy) — solo lo hace el “dueño” (master) al final.
int shm_sync_destroy(SyncState* s);

// Desmapea/cierra /game_sync.
int shm_sync_close(SyncState* s, int fd);

#endif //SO_TP12025_SHM_H