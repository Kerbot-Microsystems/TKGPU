#include "TKGPUIdle.H"
#include <stdlib.h>
uint64_t tg_idle_add(Tcl_TimerProc *fn,void *context) {
    tg_notifier *s=tg_notifier_get(1);
    if(!s||!fn||s->idle_next==UINT64_MAX) return 0;
    tg_notifier_idle *task=malloc(sizeof *task);
    if(!task) return 0;
    *task=(tg_notifier_idle){NULL,++s->idle_next,fn,context};
    if(s->idle_tail) s->idle_tail->next=task; else s->idle_head=task;
    s->idle_tail=task; return task->id;
}
static void idle_remove(tg_notifier *s,tg_notifier_idle *prev,tg_notifier_idle *task) {
    if(prev) prev->next=task->next; else s->idle_head=task->next;
    if(s->idle_tail==task) s->idle_tail=prev;
    free(task);
}
void tg_idle_cancel(uint64_t id) {
    tg_notifier *s=tg_notifier_get(0);
    if(!s||!id) return;
    tg_notifier_idle *prev=NULL;
    for(tg_notifier_idle *p=s->idle_head;p;prev=p,p=p->next) if(p->id==id) {
        idle_remove(s,prev,p); return;
    }
}
void Tcl_DoWhenIdle(Tcl_IdleProc *fn,void *context) {
    if(!tg_idle_add(fn,context)) Tcl_Panic("idle callback allocation failed");
}
void Tcl_CancelIdleCall(Tcl_IdleProc *fn,void *context) {
    tg_notifier *s=tg_notifier_get(0);
    if(!s||!fn) return;
    tg_notifier_idle *prev=NULL,*p=s->idle_head;
    while(p) {
        tg_notifier_idle *next=p->next;
        if(p->fn==fn&&p->context==context) idle_remove(s,prev,p); else prev=p;
        p=next;
    }
}
void tg_idle_clear(tg_notifier *s) {
    while(s->idle_head) {
        tg_notifier_idle *task=s->idle_head; s->idle_head=task->next; free(task);
    }
    s->idle_tail=NULL;
}
int tg_idle_dispatch(tg_notifier *s) {
    /* A callback can append or cancel any pending task. Detach before
       invoking it, and defer newly appended tasks to the next idle pass. */
    uint64_t boundary=s->idle_next;
    int handled=0;
    while(s->idle_head&&s->idle_head->id<=boundary) {
        tg_notifier_idle *task=s->idle_head;
        Tcl_TimerProc *fn=task->fn; void *context=task->context;
        idle_remove(s,NULL,task);
        handled=1; fn(context);
    }
    return handled;
}
