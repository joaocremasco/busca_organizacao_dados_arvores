#define _POSIX_C_SOURCE 200809L
#include "timer.h"

#include <time.h>

double timer_now_ms(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 0.0;
    return (double)now.tv_sec * 1000.0 + (double)now.tv_nsec / 1000000.0;
}
