#ifndef TKGPU_TICK_EXTEND_H
#define TKGPU_TICK_EXTEND_H
#include <stdint.h>
typedef struct { uint32_t last; uint64_t elapsed; int initialized; } tg_tick_state;
/* Caller serializes access; sample at least once per 2^32 milliseconds. */
uint64_t tg_tick_extend(tg_tick_state *state,uint32_t raw);
#endif
