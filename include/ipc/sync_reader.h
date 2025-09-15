#ifndef SO_TP12025_SYNC_READER_H
#define SO_TP12025_SYNC_READER_H

#include "sync.h"
#include <semaphore.h>

static inline void reader_enter(GameSync *s) {
	sem_wait(&s->sem_turnstile);
	sem_post(&s->sem_turnstile);

	sem_wait(&s->sem_reader_mutex);
	if (s->readers_count++ == 0)
		sem_wait(&s->sem_state);
	sem_post(&s->sem_reader_mutex);
}

static inline void reader_exit(GameSync *s) {
	sem_wait(&s->sem_reader_mutex);
	if (--s->readers_count == 0)
		sem_post(&s->sem_state);
	sem_post(&s->sem_reader_mutex);
}

#endif // SO_TP12025_SYNC_READER_H
