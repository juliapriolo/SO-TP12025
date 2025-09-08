#ifndef SO_TP12025_NOTIFY_H
#define SO_TP12025_NOTIFY_H

#include "master_utils.h"  

void notify_view_and_delay_if_any(Master *M);
void allow_next_send(Master *M, unsigned i);
void set_finished_and_wake_all(Master *M);

#endif // SO_TP12025_NOTIFY_H
