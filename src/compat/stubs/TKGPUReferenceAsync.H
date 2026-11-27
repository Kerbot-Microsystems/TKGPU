/* Linux reference async handlers. Mark uses atomic integer stores;
   creation/deletion/invocation belong to the creating thread. */
#include <pthread.h>
#include <stdlib.h>
#include <signal.h>
#ifndef ASYNC_API
#define ASYNC_API(n) Tcl_##n
#endif
typedef struct tg_async_state tg_async_state;
struct Tcl_AsyncHandler_ {
    struct Tcl_AsyncHandler_ *next;
    tg_async_state *owner;
    Tcl_AsyncProc *proc;
    void *data;
    int marked;
};
struct tg_async_state {
    Tcl_AsyncHandler first,last;
    int invoking,ready;
};
static pthread_key_t async_key;
static pthread_once_t async_once=PTHREAD_ONCE_INIT;
static void async_clear(void *p) {
    tg_async_state *s=p;
    while(s->first){Tcl_AsyncHandler h=s->first;s->first=h->next;free(h);}
    free(s);
}
static void async_init(void) {if(pthread_key_create(&async_key,async_clear)!=0)Tcl_Panic("async TLS initialization failed");}
static tg_async_state *async_state(int create) {
    if(pthread_once(&async_once,async_init)!=0)Tcl_Panic("async initialization failed");
    tg_async_state *s=pthread_getspecific(async_key);
    if(!s&&create){s=calloc(1,sizeof *s);if(!s||pthread_setspecific(async_key,s)!=0)Tcl_Panic("async state allocation failed");}
    return s;
}
Tcl_AsyncHandler ASYNC_API(AsyncCreate)(Tcl_AsyncProc *proc,void *data) {
    if(!proc)return NULL;
    tg_async_state *s=async_state(1);Tcl_AsyncHandler h=calloc(1,sizeof *h);
    if(!h)Tcl_Panic("async handler allocation failed");
    h->owner=s;h->proc=proc;h->data=data;
    if(s->last)s->last->next=h;else s->first=h;
    s->last=h;return h;
}
void ASYNC_API(AsyncDelete)(Tcl_AsyncHandler h) {
    tg_async_state *s=async_state(0);
    if(!h||!s||h->owner!=s)Tcl_Panic("async deletion on wrong thread");
    Tcl_AsyncHandler prev=NULL,p=s->first;
    while(p&&p!=h){prev=p;p=p->next;}
    if(!p)Tcl_Panic("unknown async handler");
    if(prev)prev->next=p->next;else s->first=p->next;
    if(s->last==p)s->last=prev;
    free(p);
}
void ASYNC_API(AsyncMark)(Tcl_AsyncHandler h) {
    /* No allocation, mutex, or TLS access in this path. Caller keeps h alive. */
    if(h){__atomic_store_n(&h->marked,1,__ATOMIC_RELEASE);__atomic_store_n(&h->owner->ready,1,__ATOMIC_RELEASE);}
}
int ASYNC_API(AsyncReady)(void) {
    tg_async_state *s=async_state(0);if(!s||s->invoking)return 0;
    return __atomic_load_n(&s->ready,__ATOMIC_ACQUIRE)!=0;
}
int ASYNC_API(AsyncInvoke)(Tcl_Interp *i,int code) {
    tg_async_state *s=async_state(0);if(!s||s->invoking)return code;
    if(!ASYNC_API(AsyncReady)())return code;
    if(!i)code=0;
    s->invoking=1;
    for(;;){
        __atomic_store_n(&s->ready,0,__ATOMIC_RELEASE);
        Tcl_AsyncHandler p=s->first;
        while(p&&!__atomic_exchange_n(&p->marked,0,__ATOMIC_ACQ_REL))p=p->next;
        if(!p)break;
        Tcl_AsyncProc *proc=p->proc;void *data=p->data;
        /* Restart at the head: callbacks may delete/re-mark any handler. */
        code=proc(data,i,code);
    }
    s->invoking=0;return code;
}
#undef ASYNC_API
