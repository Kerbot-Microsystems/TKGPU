#include "TKGPUThreadEventQueue.H"
int tg_thread_event_dispatch(tg_notifier *s,int flags) {
    tg_native_mutex_lock(s->mailbox);
    Tcl_Event *p=s->head;
    size_t remaining=s->event_count;
    while(p&&remaining--) {
        Tcl_EventProc *proc=p->proc;
        if(!proc||tg_event_deleting(s,p)) { p=p->nextPtr; continue; }
        /* Null marks an event currently being serviced, matching Tcl's public
           event record. DeleteEvents may remove it during its callback. */
        p->proc=NULL; s->active_event=p; s->active_retired=0;
        tg_native_mutex_unlock(s->mailbox);
        int handled=proc(p,flags);
        tg_native_mutex_lock(s->mailbox);
        int retired=s->active_retired;
        s->active_event=NULL; s->active_retired=0;
        Tcl_Event *live=s->head;
        if(retired) { Tcl_Free((char *)p); live=NULL; }
        else while(live&&live!=p) live=live->nextPtr;
        if(handled) {
            if(live) (void)tg_thread_event_remove(s,p);
            tg_native_mutex_unlock(s->mailbox); return 1;
        }
        if(live) { live->proc=proc; p=live->nextPtr; }
        else p=s->head;
    }
    tg_native_mutex_unlock(s->mailbox); return 0;
}
