#ifndef SO_TP12025_SYNC_WRITER_H
#define SO_TP12025_SYNC_WRITER_H

#include "sync.h"
#include <errno.h>
#include <semaphore.h>

/* Helper: esperar manejando EINTR */
static inline void sem_wait_intr(sem_t *s) {
	while (sem_wait(s) == -1 && errno == EINTR) { /* retry */
	}
}

/*
 * writer_enter:
 *  - C: tomamos el molinete (turnstile) para bloquear nuevos lectores.
 *  - D: tomamos el lock del estado (exclusión de escritor).
 *
 * Al mantener tomado turnstile mientras escribimos, ningún lector nuevo
 * puede “colarse” hasta que liberemos en writer_exit().
 */
static inline void writer_enter(GameSync *sync) {
	sem_wait_intr(&sync->sem_turnstile); /* C */
	sem_wait_intr(&sync->sem_state);	 /* D */
}

/*
 * writer_exit:
 *  - Liberamos el estado (D) y luego el molinete (C) para reabrir el paso
 *    a lectores y a otros escritores.
 */
static inline void writer_exit(GameSync *sync) {
	sem_post(&sync->sem_state);		/* D */
	sem_post(&sync->sem_turnstile); /* C */
}

#endif /* SO_TP12025_SYNC_WRITER_H */
