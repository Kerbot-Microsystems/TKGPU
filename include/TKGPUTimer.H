#ifndef TKGPU_TIMER_H
#define TKGPU_TIMER_H
#include "TKGPURuntime.H"
typedef struct tg_timers tg_timers;
typedef uint64_t tg_timer_id;
typedef void (*tg_timer_fn)(void *);
tg_timers *tg_timers_create(void);
void tg_timers_destroy(tg_timers *q);
/* Caller supplies monotonic milliseconds. Single-threaded queue. */
tg_timer_id tg_timer_schedule(tg_timers *q,uint64_t now,uint64_t delay,tg_timer_fn fn,void *context);
int tg_timer_cancel(tg_timers *q,tg_timer_id id);
int tg_timer_next(const tg_timers *q,uint64_t *deadline);
int tg_timer_dispatch_one(tg_timers *q,uint64_t now);
#endif
