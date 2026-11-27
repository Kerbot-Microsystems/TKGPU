#ifndef TKGPU_CLOCK_H
#define TKGPU_CLOCK_H
#include <stdint.h>
int tg_clock_now(uint64_t *ms);
void tg_clock_sleep(uint64_t ms);
int tg_timer_clock_now(uint64_t *ms);
uint64_t tg_timer_wait_ms(uint64_t ms);
#endif
