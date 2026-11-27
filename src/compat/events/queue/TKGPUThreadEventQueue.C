#include "TKGPUThreadEventQueue.H"
void tg_thread_event_insert(tg_notifier *s,Tcl_Event *e,Tcl_QueuePosition pos) {
    if(pos==TKGPU_QUEUE_TAIL) {
        e->nextPtr=NULL;
        if(s->tail) s->tail->nextPtr=e; else s->head=e;
    } else if(pos==TKGPU_QUEUE_MARK&&s->marker) {
        e->nextPtr=s->marker->nextPtr; s->marker->nextPtr=e;
    } else {
        e->nextPtr=s->head; s->head=e;
    }
    if(!e->nextPtr) s->tail=e;
    if(pos==TKGPU_QUEUE_MARK) s->marker=e;
    s->event_count++;
}
void tg_thread_event_clear(tg_notifier *s) {
    while(s->head) {
        Tcl_Event *e=s->head; s->head=e->nextPtr;
        Tcl_Free((char *)e);
    }
    s->tail=NULL; s->marker=NULL; s->event_count=0;
}

int tg_thread_event_remove(tg_notifier *s,Tcl_Event *event) {
    Tcl_Event *prev=NULL,*p=s->head;
    while(p&&p!=event) { prev=p; p=p->nextPtr; }
    if(!p) return 0;
    if(prev) prev->nextPtr=p->nextPtr; else s->head=p->nextPtr;
    if(s->tail==p) s->tail=prev;
    if(s->marker==p) s->marker=prev;
    s->event_count--;
    if(p==s->active_event) s->active_retired=1;
    else Tcl_Free((char *)p);
    return 1;
}
