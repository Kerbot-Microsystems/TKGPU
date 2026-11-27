#ifndef TKGPU_TIMER_INTERNAL_H
#define TKGPU_TIMER_INTERNAL_H
#include "TKGPUTimer.H"
enum { TG_TIMER_LIMIT=256 };
typedef struct { tg_timer_id id; uint64_t deadline; tg_timer_fn fn; void *context; } tg_timer;
struct tg_timers { tg_timer entries[TG_TIMER_LIMIT]; size_t count; tg_timer_id next; int dispatching; };
size_t tg_timer_earliest(const tg_timers *q);
#endif
