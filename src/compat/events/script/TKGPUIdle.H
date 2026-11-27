#ifndef TKGPU_IDLE_H
#define TKGPU_IDLE_H
#include "../TKGPUNotifier.H"
uint64_t tg_idle_add(Tcl_TimerProc *,void *);
void tg_idle_cancel(uint64_t);
int tg_idle_dispatch(tg_notifier *);
void tg_idle_clear(tg_notifier *);
#endif
