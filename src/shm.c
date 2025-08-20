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