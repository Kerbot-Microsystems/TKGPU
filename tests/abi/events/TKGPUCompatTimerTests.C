#include "TKGPUCompat.H"
#include <stdio.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
typedef struct { int count; int nested; } state;
static void fire(void *p) {
    state *s=p; s->count++;
    s->nested=Tcl_DoOneEvent(TKGPU_DONT_WAIT);
}
int main(void) {
    state a={0},b={0};
    Tcl_TimerToken t=Tcl_CreateTimerHandler(0,fire,&a);
    Tcl_TimerToken c=Tcl_CreateTimerHandler(0,fire,&b);
    CHECK(t&&c&&t!=c);
    Tcl_DeleteTimerHandler(c); Tcl_DeleteTimerHandler(c);
    CHECK(Tcl_DoOneEvent(TKGPU_DONT_WAIT|TKGPU_FILE_EVENTS)==0);
    CHECK(Tcl_DoOneEvent(TKGPU_DONT_WAIT|TKGPU_TIMER_EVENTS)==1);
    CHECK(a.count==1&&a.nested==0&&b.count==0);
    CHECK(Tcl_DoOneEvent(TKGPU_DONT_WAIT)==0);
    Tcl_DeleteTimerHandler(t);
    t=Tcl_CreateTimerHandler(10,fire,&a); CHECK(t);
    CHECK(Tcl_DoOneEvent(TKGPU_TIMER_EVENTS)==1&&a.count==2);
    Tcl_DeleteTimerHandler(t);
    CHECK(Tcl_DoOneEvent(TKGPU_DONT_WAIT)==0);
    puts("timer notifier checks passed"); return 0;
}
