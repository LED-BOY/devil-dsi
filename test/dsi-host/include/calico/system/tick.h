#pragma once
#include <stdint.h>
#define TICK_FREQ 1000000
extern uint64_t host_ticks;
static inline uint64_t tickGetCount(void) { return host_ticks; }
