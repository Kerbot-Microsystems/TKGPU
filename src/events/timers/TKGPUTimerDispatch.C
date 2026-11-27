#include "TKGPUTimerInternal.H"
int tg_timer_dispatch_one(tg_timers *q,uint64_t now) {
    if(!q||!q->count||q->dispatching>=16) return 0;
    size_t k=tg_timer_earliest(q);
    tg_timer event=q->entries[k];
    if(event.deadline>now) return 0;
    q->entries[k]=q->entries[--q->count];
    q->dispatching++;
    event.fn(event.context);
    q->dispatching--;
    return 1;
}
