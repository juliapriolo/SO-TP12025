#ifndef SO_TP12025_SYNC_WRITER_H
#define SO_TP12025_SYNC_WRITER_H

#include "sync.h"
#include "sem_utils.h"


static inline void writer_enter(GameSync *sync) {
    sem_wait_intr(&sync->sem_turnstile); /* C */
    sem_wait_intr(&sync->sem_state);     /* D */
}

static inline void writer_exit(GameSync *sync) {
	sem_post(&sync->sem_state);		/* D */
	sem_post(&sync->sem_turnstile); /* C */
}

#endif /* SO_TP12025_SYNC_WRITER_H */
