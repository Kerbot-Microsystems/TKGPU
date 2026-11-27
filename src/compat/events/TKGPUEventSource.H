/* Ordered event-source registry; owner-thread operations only. Defer
   reclamation while a callback traverses the registry. */
#include <stdlib.h>
#include <stdint.h>
#ifndef SOURCE_API
#define SOURCE_API(n) Tcl_##n
#endif
typedef struct tg_event_source {
    struct tg_event_source *next;
    Tcl_EventSetupProc *setup;
    Tcl_EventCheckProc *check;
    void *data;
    int deleted;
} tg_event_source;
static void source_sweep(tg_notifier *s) {
    tg_event_source **p=&s->sources;
    while(*p) {
        tg_event_source *v=*p;
        if(v->deleted) {*p=v->next;free(v);} else p=&v->next;
    }
}
void tg_source_clear(tg_notifier *s) {
    while(s->sources) {tg_event_source *v=s->sources;s->sources=v->next;free(v);}
}
void SOURCE_API(CreateEventSource)(Tcl_EventSetupProc *setup,Tcl_EventCheckProc *check,void *data) {
    tg_notifier *s=tg_notifier_get(1);
    if(!s||(!setup&&!check))return;
    tg_event_source *v=malloc(sizeof *v);
    if(!v)Tcl_Panic("event source allocation failed");
    *v=(tg_event_source){s->sources,setup,check,data,0};s->sources=v;
}
void SOURCE_API(DeleteEventSource)(Tcl_EventSetupProc *setup,Tcl_EventCheckProc *check,void *data) {
    tg_notifier *s=tg_notifier_get(0);if(!s)return;
    for(tg_event_source *v=s->sources;v;v=v->next) {
        if(!v->deleted&&v->setup==setup&&v->check==check&&v->data==data) {v->deleted=1;break;}
    }
    if(!s->source_depth)source_sweep(s);
}
void SOURCE_API(SetMaxBlockTime)(const Tcl_Time *t) {
    if(!t)return;
    tg_notifier *s=tg_notifier_get(1);if(!s)return;
    int64_t ms;
    if(t->sec<0||t->usec<0)ms=0;
    else if((uint64_t)t->sec>(uint64_t)(INT64_MAX-1000)/1000)ms=INT64_MAX;
    else {
        uint64_t sec=(uint64_t)t->sec*1000;
        uint64_t us=(uint64_t)t->usec/1000+((uint64_t)t->usec%1000!=0);
        ms=us>(uint64_t)INT64_MAX-sec?INT64_MAX:(int64_t)(sec+us);
    }
    if(s->source_wait<0||ms<s->source_wait)s->source_wait=ms;
}
void tg_source_run(tg_notifier *s,int flags,int checking) {
    /* Tcl bypasses external sources for idle-only event service. */
    if((flags&~TKGPU_DONT_WAIT)==TKGPU_IDLE_EVENTS)return;
    ++s->source_depth;
    for(tg_event_source *v=s->sources;v;v=v->next) {
        if(v->deleted)continue;
        if(checking){if(v->check)v->check(v->data,flags);}
        else if(v->setup)v->setup(v->data,flags);
    }
    if(!--s->source_depth)source_sweep(s);
}
#undef SOURCE_API
