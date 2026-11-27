#include "TKGPUTimer.H"
#include <stdio.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
typedef struct { tg_timers *q; int value,called; } state;
static void record(void *p) { state *s=p; s->called++; }
int main(void) {
    tg_timers *q=tg_timers_create(); CHECK(q);
    state a={q,1,0},b={q,2,0},c={q,3,0};
    tg_timer_id first=tg_timer_schedule(q,100,20,record,&a);
    tg_timer_id second=tg_timer_schedule(q,100,20,record,&b);
    tg_timer_id cancelled=tg_timer_schedule(q,100,5,record,&c);
    CHECK(first&&second&&cancelled);
    CHECK(tg_timer_cancel(q,cancelled)==TG_OK);
    CHECK(tg_timer_cancel(q,cancelled)==TG_ERROR);
    uint64_t deadline=0; CHECK(tg_timer_next(q,&deadline)&&deadline==120);
    CHECK(!tg_timer_dispatch_one(q,119));
    CHECK(tg_timer_dispatch_one(q,120)&&a.called==1&&b.called==0);
    CHECK(tg_timer_dispatch_one(q,120)&&b.called==1&&c.called==0);
    CHECK(!tg_timer_dispatch_one(q,120)&&!tg_timer_next(q,&deadline));
    CHECK(!tg_timer_schedule(q,UINT64_MAX,1,record,&a));
    for(int k=0;k<256;k++) CHECK(tg_timer_schedule(q,0,0,record,&a));
    CHECK(!tg_timer_schedule(q,0,0,record,&a));
    tg_timers_destroy(q);
    puts("timer queue checks passed"); return 0;
}
