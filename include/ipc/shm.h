#ifndef SO_TP12025_SHM_H
#define SO_TP12025_SHM_H

#include <stddef.h>

// Crea una shared memory con nombre y tamaño.
// Devuelve puntero a la memoria o NULL si falla.
void *shm_create(const char *name, size_t size, int mode);

// Se conecta a una shared memory ya existente.
// Devuelve puntero a la memoria o NULL si falla.
void *shm_connect(const char *name, size_t size, int mode);

// Hace unmap de la memoria (puntero y tamaño).
int shm_unmap(void *addr, size_t size);

// Elimina la shared memory.
int shm_delete(const char *name);

#endif // SO_TP12025_SHM_H
