#include "TKGPUThreadEventQueue.H"
#include <stdlib.h>

typedef struct tg_event_delete_guard {
    struct tg_event_delete_guard *previous;
    Tcl_Event *event;
} tg_event_delete_guard;
int tg_event_deleting(tg_notifier *s,Tcl_Event *event) {
    for(tg_event_delete_guard *g=s->delete_guards;g;g=g->previous)
        if(g->event==event) return 1;
    return 0;
}
void Tcl_DeleteEvents(Tcl_EventDeleteProc *proc,void *data) {
    tg_notifier *s=tg_notifier_get(0);
    if(!s||!proc) return;
    tg_native_mutex_lock(s->mailbox);
    size_t count=s->event_count;
    if(!count) { tg_native_mutex_unlock(s->mailbox); return; }
    if(count>SIZE_MAX/sizeof(Tcl_Event *)) Tcl_Panic("event snapshot overflow");
    Tcl_Event **snapshot=malloc(count*sizeof *snapshot);
    if(!snapshot) Tcl_Panic("cannot snapshot queued events");
    size_t n=0;
    for(Tcl_Event *e=s->head;e&&n<count;e=e->nextPtr) snapshot[n++]=e;
    for(size_t k=0;k<n;k++) {
        Tcl_Event *event=snapshot[k],*live=s->head;
        while(live&&live!=event) live=live->nextPtr;
        if(!live||tg_event_deleting(s,event)) continue;
        tg_event_delete_guard guard={s->delete_guards,event};
        s->delete_guards=&guard;
        /* Application predicates may queue events, delete other events, or
           enter the event loop. Never run them under the mailbox mutex. */
        tg_native_mutex_unlock(s->mailbox);
        int remove=proc(event,data);
        tg_native_mutex_lock(s->mailbox);
        s->delete_guards=guard.previous;
        if(remove) (void)tg_thread_event_remove(s,event);
    }
    tg_native_mutex_unlock(s->mailbox);
    free(snapshot);
}
