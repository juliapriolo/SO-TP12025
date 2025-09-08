#ifndef SO_TP12025_CLEANUP_H
#define SO_TP12025_CLEANUP_H

#include <stddef.h>
#include "master_utils.h"
#include "state.h"
#include "sync.h"

void cleanup_master(Master *M);
void finish_game_and_cleanup(Master *M,
                             GameState *state,
                             GameSync *sync,
                             size_t state_bytes);

#endif // SO_TP12025_CLEANUP_H
