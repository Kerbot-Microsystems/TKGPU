#include "TKGPUNotifier.H"
#include "../../platform/time/TKGPUClock.H"
static void fire(void *p) {
    tg_notifier_timer *slot=p;
    Tcl_TimerProc *fn=slot->fn; void *ctx=slot->context;
    slot->id=0; slot->token=0; slot->fn=NULL; slot->context=NULL;
    fn(ctx);
}
Tcl_TimerToken Tcl_CreateTimerHandler(int ms,Tcl_TimerProc *fn,void *context) {
    if(!fn) return NULL;
    tg_notifier *s=tg_notifier_get(1); uint64_t now;
    if(!s||!s->next||!tg_timer_clock_now(&now)) return NULL;
    for(size_t k=0;k<TG_NOTIFIER_LIMIT;k++) if(!s->slots[k].id) {
        tg_notifier_timer *slot=&s->slots[k];
        tg_timer_id id=tg_timer_schedule(s->queue,now,ms<0?0:(uint64_t)ms,fire,slot);
        if(!id) return NULL;
        *slot=(tg_notifier_timer){id,fn,context,s->next++};
        return (Tcl_TimerToken)slot->token;
    }
    return NULL;
}
void Tcl_DeleteTimerHandler(Tcl_TimerToken token) {
    if(!token) return;
    tg_notifier *s=tg_notifier_get(0);
    if(!s) return;
    for(size_t k=0;k<TG_NOTIFIER_LIMIT;k++) {
        tg_notifier_timer *slot=&s->slots[k];
        if(slot->id&&slot->token==(uintptr_t)token) {
            if(tg_timer_cancel(s->queue,slot->id)==TG_OK) {
                slot->id=0; slot->token=0; slot->fn=NULL; slot->context=NULL;
            }
            return;
        }
    }
}
