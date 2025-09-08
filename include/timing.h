#ifndef SO_TP12025_TIMING_H
#define SO_TP12025_TIMING_H

#include <stdint.h>

void die(const char *fmt, ...);
void sleep_ms(long ms);
uint64_t now_ms_monotonic(void);

#endif // SO_TP12025_TIMING_H
