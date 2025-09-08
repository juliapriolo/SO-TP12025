#ifndef SO_TP12025_SEM_UTILS_H
#define SO_TP12025_SEM_UTILS_H

#include <errno.h>
#include <semaphore.h>

static inline void sem_wait_intr(sem_t *s) {
    while (sem_wait(s) == -1 && errno == EINTR) { /* retry */ }
}

static inline void sem_post_n(sem_t *s, unsigned n) {
    for (unsigned i = 0; i < n; ++i) {
        (void) sem_post(s);
    }
}

#endif // SO_TP12025_SEM_UTILS_H

