#include "TKGPUNotifier.H"
#include "../../platform/time/TKGPUClock.H"
#include "../tk/TKGPUWindow.H"
#include <limits.h>

void *Tcl_InitNotifier(void) { return tg_notifier_get(1); }
void Tcl_FinalizeNotifier(void *token) {
    if(token&&token==tg_notifier_get(0)) (void)tg_notifier_finalize_current();
}
static uint64_t delay_ms(const Tcl_Time *time) {
    if(time->sec<0||time->usec<0) return 0;
    if((uint64_t)time->sec>(UINT64_MAX-1000)/1000) return UINT64_MAX;
    uint64_t sec=(uint64_t)time->sec*1000;
    uint64_t fraction=(uint64_t)time->usec/1000+((uint64_t)time->usec%1000!=0);
    return fraction>UINT64_MAX-sec?UINT64_MAX:sec+fraction;
}
void Tcl_SetTimer(const Tcl_Time *time) {
    tg_notifier *s=tg_notifier_get(1);
    if(!s) return;
    s->timer_set=time!=NULL;
    if(time) {
        uint64_t now;
        if(!tg_clock_now(&now)) { s->timer_set=0; return; }
        uint64_t delay=delay_ms(time);
        s->notifier_deadline=delay>UINT64_MAX-now?UINT64_MAX:now+delay;
    }
}
static void shorten(int64_t *wait,uint64_t delay) {
    int64_t bounded=delay>INT64_MAX?INT64_MAX:(int64_t)delay;
    if(*wait<0||bounded<*wait) *wait=bounded;
}
int Tcl_WaitForEvent(const Tcl_Time *time) {
    tg_notifier *s=tg_notifier_get(1);
    if(!s) return -1;
    int64_t wait=-1;
    if(time) shorten(&wait,delay_ms(time));
    if(s->timer_set) {
        uint64_t now;
        if(!tg_clock_now(&now)) return -1;
        shorten(&wait,s->notifier_deadline>now?s->notifier_deadline-now:0);
    }
    /* Native messages/file readiness are polled by their event sources.
       Waiting must not dispatch an application callback recursively. */
    if(tg_window_pending()) shorten(&wait,10);
#ifndef _WIN32
    if(s->file_count) shorten(&wait,10);
#endif
    tg_native_mutex_lock(s->mailbox);
    if(!s->head&&!s->alerted&&wait!=0)
        tg_native_condition_wait(s->wake,s->mailbox,wait);
    s->alerted=0;
    tg_native_mutex_unlock(s->mailbox);
    if(s->timer_set) {
        uint64_t now;
        if(tg_clock_now(&now)&&now>=s->notifier_deadline) s->timer_set=0;
    }
    return 0;
}
