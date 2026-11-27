#include "TKGPUTimerInternal.H"
#include <stdlib.h>
tg_timers *tg_timers_create(void) {
    tg_timers *q=calloc(1,sizeof(*q));
    if(q) q->next=1;
    return q;
}
void tg_timers_destroy(tg_timers *q) { if(q&&!q->dispatching) free(q); }
tg_timer_id tg_timer_schedule(tg_timers *q,uint64_t now,uint64_t delay,tg_timer_fn fn,void *context) {
    if(!q||!fn||q->count==TG_TIMER_LIMIT||!q->next||delay>UINT64_MAX-now) return 0;
    tg_timer_id id=q->next++;
    q->entries[q->count++]=(tg_timer){id,now+delay,fn,context};
    return id;
}
int tg_timer_cancel(tg_timers *q,tg_timer_id id) {
    if(!q||!id) return TG_ERROR;
    for(size_t k=0;k<q->count;k++) if(q->entries[k].id==id) {
        q->entries[k]=q->entries[--q->count]; return TG_OK;
    }
    return TG_ERROR;
}
size_t tg_timer_earliest(const tg_timers *q) {
    size_t best=0;
    for(size_t k=1;k<q->count;k++) {
        const tg_timer *a=&q->entries[k],*b=&q->entries[best];
        if(a->deadline<b->deadline||(a->deadline==b->deadline&&a->id<b->id)) best=k;
    }
    return best;
}
int tg_timer_next(const tg_timers *q,uint64_t *deadline) {
    if(!q||!q->count||!deadline) return 0;
    *deadline=q->entries[tg_timer_earliest(q)].deadline; return 1;
}
