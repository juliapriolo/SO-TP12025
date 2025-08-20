#include <sys/stat.h>  // fstat
#include <semaphore.h> // sem_init, sem_destroy
#include <string.h>    // memset
#include "shm.h"

// =======================================================
// Capa genérica (syscalls de shm, sin saber de GameState)
// =======================================================

// Crea un objeto shm (O_CREAT|O_EXCL|O_RDWR), lo trunca a 'size' y devuelve fd.
// mode típico: 0600. Retorna -1 en error.
int shm_create_fd(const char* name, size_t size, mode_t mode){
    if (name == NULL || size == 0) {
        errno = EINVAL; // Argumentos inválidos
        return -1;
    }

    int fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, mode);
    if (fd == -1) {
        return -1;
    }

    if (ftruncate(fd, size) == -1) {
        int saved_errno = errno;
        close(fd);
        shm_unlink(name);
        errno = saved_errno;
        return -1;
    }
    return fd;
}

// Abre un objeto shm existente con las flags dadas (O_RDONLY u O_RDWR).
// Retorna -1 en error.
int shm_open_fd(const char* name, int oflag){
    int fd = shm_open(name, oflag, 0);
    if (fd == -1) {
        return -1;
    }
    return fd;
}

// Mapea 'size' bytes con 'prot' (PROT_READ o PROT_READ|PROT_WRITE), siempre MAP_SHARED.
// Devuelve puntero o MAP_FAILED.
void* shm_map(int fd, size_t size, int prot){
    if (fd < 0 || size == 0) {
        errno = EINVAL;
        return MAP_FAILED;
    }
    return mmap(NULL, size, prot, MAP_SHARED, fd, 0);
}

// Desmapea y cierra fd. Retorna 0 si ambos pasos fueron OK, -1 si falló alguno.
int shm_unmap_and_close(void* addr, size_t size, int fd){
    int rc = 0, saved_errno = 0;
    if (munmap(addr, size) == -1) {
        rc = -1;
        saved_errno = errno;
    }

    if (close(fd) == -1) {
        rc = -1;
        if (saved_errno == 0) {
            saved_errno = errno;
        }
    }

    if (rc) {
        errno = saved_errno;
    }
    return rc;
}

// Unlink “seguro”: intenta shm_unlink y devuelve 0 si ok, -1 si falla.
int shm_unlink_safe(const char* name){
    if (shm_unlink(name) == -1 && errno != ENOENT) {
        return -1;
    }
    return 0;
}


// =======================================================
// Capa específica de /game_state (tamaño variable por FAM)
// =======================================================

GameState* shm_state_create(uint16_t w, uint16_t h, int* out_fd) {
    const size_t total = game_state_size(w, h);
    int fd = shm_create_fd(SHM_STATE_NAME, total, 0600);
    if (fd == -1) return NULL;

    void* addr = shm_map(fd, total, PROT_READ | PROT_WRITE);
    if (addr == MAP_FAILED) {
        int saved = errno;
        close(fd);
        shm_unlink(SHM_STATE_NAME);
        errno = saved;
        return NULL;
    }

    if (out_fd) *out_fd = fd;
    // Limpieza completa por seguridad
    memset(addr, 0, total);
    return (GameState*)addr;
}

static inline int fd_size(int fd, size_t* out) {
    struct stat st;
    if (fstat(fd, &st) == -1) return -1;
    if (out) *out = (size_t)st.st_size;
    return 0;
}

GameState* shm_state_open_rw(int* out_fd, size_t* out_size) {
    int fd = shm_open_fd(SHM_STATE_NAME, O_RDWR);
    if (fd == -1) return NULL;

    size_t total = 0;
    if (fd_size(fd, &total) == -1 || total < sizeof(GameState)) {
        int saved = errno ? errno : EINVAL;
        close(fd);
        errno = saved;
        return NULL;
    }

    void* addr = shm_map(fd, total, PROT_READ | PROT_WRITE);
    if (addr == MAP_FAILED) {
        int saved = errno;
        close(fd);
        errno = saved;
        return NULL;
    }

    if (out_fd)   *out_fd   = fd;
    if (out_size) *out_size = total;
    return (GameState*)addr;
}

const GameState* shm_state_open_ro(int* out_fd, size_t* out_size) {
    int fd = shm_open_fd(SHM_STATE_NAME, O_RDONLY);
    if (fd == -1) return NULL;

    size_t total = 0;
    if (fd_size(fd, &total) == -1 || total < sizeof(GameState)) {
        int saved = errno ? errno : EINVAL;
        close(fd);
        errno = saved;
        return NULL;
    }

    void* addr = shm_map(fd, total, PROT_READ);
    if (addr == MAP_FAILED) {
        int saved = errno;
        close(fd);
        errno = saved;
        return NULL;
    }

    if (out_fd)   *out_fd   = fd;
    if (out_size) *out_size = total;
    return (const GameState*)addr;
}

int shm_state_close(const void* addr, size_t total_size, int fd) {
    return shm_unmap_and_close((void*)addr, total_size, fd);
}


// =======================================================
// Capa específica de /game_sync (tamaño fijo + sem_init)
// =======================================================


static void syncstate_init_all(SyncState* s) {
    // Semáforos de master <-> view
    sem_init(&s->notify_view, 1, 0);
    sem_init(&s->view_done,   1, 0);

    // Lectores-escritor (evita inanición del escritor)
    sem_init(&s->writer_gate,  1, 1);
    sem_init(&s->state_mutex,  1, 1);
    sem_init(&s->read_count_mx,1, 1);
    s->read_count = 0;

    // Turno por jugador: todos bloqueados al inicio
    for (int i = 0; i < MAX_PLAYERS; i++) {
        sem_init(&s->player_turn[i], 1, 0);
    }
}

SyncState* shm_sync_create(int* out_fd) {
    const size_t total = sizeof(SyncState);
    int fd = shm_create_fd(SHM_SYNC_NAME, total, 0600);
    if (fd == -1) return NULL;

    void* addr = shm_map(fd, total, PROT_READ | PROT_WRITE);
    if (addr == MAP_FAILED) {
        int saved = errno;
        close(fd);
        shm_unlink(SHM_SYNC_NAME);
        errno = saved;
        return NULL;
    }

    // Limpio y luego inicializo semáforos
    memset(addr, 0, total);
    syncstate_init_all((SyncState*)addr);

    if (out_fd) *out_fd = fd;
    return (SyncState*)addr;
}

SyncState* shm_sync_open_rw(int* out_fd) {
    int fd = shm_open_fd(SHM_SYNC_NAME, O_RDWR);
    if (fd == -1) return NULL;

    const size_t total = sizeof(SyncState);
    void* addr = shm_map(fd, total, PROT_READ | PROT_WRITE);
    if (addr == MAP_FAILED) {
        int saved = errno;
        close(fd);
        errno = saved;
        return NULL;
    }

    if (out_fd) *out_fd = fd;
    return (SyncState*)addr;
}

// Importante: llamar a esto SOLO cuando ya no haya procesos usando los semáforos.
int shm_sync_destroy(SyncState* s) {
    if (!s) return 0;
    int rc = 0, saved = 0;

    // Destruyo todos los semáforos
    if (sem_destroy(&s->notify_view)   == -1) { rc = -1; saved = saved ? saved : errno; }
    if (sem_destroy(&s->view_done)     == -1) { rc = -1; saved = saved ? saved : errno; }
    if (sem_destroy(&s->writer_gate)   == -1) { rc = -1; saved = saved ? saved : errno; }
    if (sem_destroy(&s->state_mutex)   == -1) { rc = -1; saved = saved ? saved : errno; }
    if (sem_destroy(&s->read_count_mx) == -1) { rc = -1; saved = saved ? saved : errno; }
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (sem_destroy(&s->player_turn[i]) == -1) { rc = -1; saved = saved ? saved : errno; }
    }
    if (rc) errno = saved;
    return rc;
}

int shm_sync_close(SyncState* s, int fd) {
    return shm_unmap_and_close((void*)s, sizeof(SyncState), fd);
}
