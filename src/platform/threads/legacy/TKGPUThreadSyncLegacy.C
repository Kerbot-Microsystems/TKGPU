#include <windows.h>
#include "../TKGPUThreadNative.H"
#include <stdlib.h>
struct tg_native_mutex { CRITICAL_SECTION value; };
typedef struct waiter { HANDLE event; struct waiter *next; } waiter;
struct tg_native_condition { CRITICAL_SECTION lock; waiter *head; };
static volatile LONG guard;
void tg_native_guard_lock(void) {
    while(InterlockedExchange(&guard,1)!=0) Sleep(1);
}
void tg_native_guard_unlock(void) { (void)InterlockedExchange(&guard,0); }
tg_native_mutex *tg_native_mutex_create(void) {
    tg_native_mutex *m=malloc(sizeof *m); if(!m) abort();
    InitializeCriticalSection(&m->value); return m;
}
void tg_native_mutex_destroy(tg_native_mutex *m) {
    if(m) { DeleteCriticalSection(&m->value); free(m); }
}
void tg_native_mutex_lock(tg_native_mutex *m) { EnterCriticalSection(&m->value); }
void tg_native_mutex_unlock(tg_native_mutex *m) { LeaveCriticalSection(&m->value); }
tg_native_condition *tg_native_condition_create(void) {
    tg_native_condition *c=calloc(1,sizeof *c); if(!c) abort();
    InitializeCriticalSection(&c->lock); return c;
}
void tg_native_condition_destroy(tg_native_condition *c) {
    if(!c) return;
    /* Finalization requires quiescent users, just as the modern adapter. */
    if(c->head) abort();
    DeleteCriticalSection(&c->lock); free(c);
}
void tg_native_condition_notify(tg_native_condition *c) {
    EnterCriticalSection(&c->lock);
    for(waiter *p=c->head;p;p=p->next) if(!SetEvent(p->event)) abort();
    LeaveCriticalSection(&c->lock);
}
void tg_native_condition_wait(tg_native_condition *c,tg_native_mutex *m,int64_t ms) {
    waiter node={0}; node.event=CreateEventA(NULL,FALSE,FALSE,NULL);
    if(!node.event) abort();
    /* Publish waiter before releasing caller's mutex. Each waiter owns its
       event, avoiding shared reset races during broadcast and timeout. */
    EnterCriticalSection(&c->lock); node.next=c->head; c->head=&node; LeaveCriticalSection(&c->lock);
    tg_native_mutex_unlock(m);
    DWORD wait=ms<0?INFINITE:(DWORD)(ms>86400000?86400000:ms);
    DWORD rc=WaitForSingleObject(node.event,wait);
    EnterCriticalSection(&c->lock);
    waiter **link=&c->head;
    while(*link&&*link!=&node) link=&(*link)->next;
    if(!*link) abort();
    *link=node.next;
    LeaveCriticalSection(&c->lock);
    CloseHandle(node.event); tg_native_mutex_lock(m);
    if(rc!=WAIT_OBJECT_0&&rc!=WAIT_TIMEOUT) abort();
}
