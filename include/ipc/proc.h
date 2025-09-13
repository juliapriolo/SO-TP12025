#ifndef SO_TP12025_PROC_H
#define SO_TP12025_PROC_H

#include <sys/types.h>
#include "state.h"
#include "master_utils.h"  // trae Master, Args, Child
#include "timing.h"

pid_t spawn_view(Master *M);
void spawn_players(Master *M,
                   unsigned short px[MAX_PLAYERS],
                   unsigned short py[MAX_PLAYERS]);

void print_child_status(pid_t pid, int status,
                        const char *label,
                        const PlayerInfo *pinfo_or_null);

#endif // SO_TP12025_PROC_H
