#include "TKGPUThreadEventQueue.H"
static tg_notifier *registry;
void tg_notifier_register(tg_notifier *s) {
    tg_native_guard_lock();
    s->registry_next=registry; registry=s;
    tg_native_guard_unlock();
}
void tg_notifier_unregister(tg_notifier *s) {
    tg_native_guard_lock();
    tg_notifier **p=&registry;
    while(*p&&*p!=s) p=&(*p)->registry_next;
    if(*p) *p=s->registry_next;
    tg_native_guard_unlock();
}
static tg_notifier *lookup(Tcl_ThreadId id) {
    tg_notifier *s=registry;
    while(s&&s->owner!=id) s=s->registry_next;
    return s;
}
void Tcl_ThreadQueueEvent(Tcl_ThreadId id,Tcl_Event *e,Tcl_QueuePosition pos) {
    if(!e) return;
    if(!e->proc||pos<TKGPU_QUEUE_TAIL||pos>TKGPU_QUEUE_MARK) { Tcl_Free((char *)e); return; }
    tg_native_guard_lock();
    tg_notifier *s=lookup(id);
    if(s) {
        tg_native_mutex_lock(s->mailbox);
        tg_thread_event_insert(s,e,pos);
        s->generation++;
        tg_native_condition_notify(s->wake);
        tg_native_mutex_unlock(s->mailbox);
    } else Tcl_Free((char *)e);
    tg_native_guard_unlock();
}
void Tcl_ThreadAlert(Tcl_ThreadId id) {
    tg_native_guard_lock();
    tg_notifier *s=lookup(id);
    if(s) {
        tg_native_mutex_lock(s->mailbox);
        s->alerted=1; s->generation++;
        tg_native_condition_notify(s->wake);
        tg_native_mutex_unlock(s->mailbox);
    }
    tg_native_guard_unlock();
}
void Tcl_QueueEvent(Tcl_Event *e,Tcl_QueuePosition pos) {
    tg_notifier *s=tg_notifier_get(1);
    if(!s) { Tcl_Free((char *)e); return; }
    Tcl_ThreadQueueEvent(s->owner,e,pos);
}

void Tcl_AlertNotifier(void *token) {
    /* Validate membership while holding the registry guard, before touching
       a token that may race with owner-thread notifier finalization. */
    tg_native_guard_lock();
    tg_notifier *s=registry;
    while(s&&(void *)s!=token) s=s->registry_next;
    if(s) {
        tg_native_mutex_lock(s->mailbox);
        s->alerted=1; ++s->generation;
        tg_native_condition_notify(s->wake);
        tg_native_mutex_unlock(s->mailbox);
    }
    tg_native_guard_unlock();
}
