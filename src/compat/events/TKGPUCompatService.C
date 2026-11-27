#include "TKGPUNotifier.H"
#include "queue/TKGPUThreadEventQueue.H"
int Tcl_GetServiceMode(void) { tg_notifier *s=tg_notifier_get(1); return s?s->service_mode:0; }
int Tcl_SetServiceMode(int mode) { tg_notifier *s=tg_notifier_get(1); if(!s)return 0; int old=s->service_mode; s->service_mode=mode; return old; }
int Tcl_ServiceEvent(int flags) {
    tg_notifier *s=tg_notifier_get(1); if(!s||s->busy)return 0;
    if(!(flags&~TKGPU_DONT_WAIT)) flags|=~TKGPU_DONT_WAIT;
    s->busy=1; int rc=tg_thread_event_dispatch(s,flags); s->busy=0; return rc;
}
int Tcl_ServiceAll(void) {
    tg_notifier *s=tg_notifier_get(1); if(!s||!s->service_mode||s->busy)return 0;
    s->service_mode=0; int handled=0;
    while(tg_do_one_event(TKGPU_DONT_WAIT,~TKGPU_DONT_WAIT)) handled=1;
    s->service_mode=1; return handled;
}
