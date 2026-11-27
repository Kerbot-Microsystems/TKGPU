#include "TKGPUNotifier.H"
#include "queue/TKGPUThreadEventQueue.H"
#include "script/TKGPUIdle.H"
#include <stdlib.h>
#if defined(_WIN32) && defined(TKGPU_LEGACY_WINDOWS)
#include <windows.h>
static DWORD key=TLS_OUT_OF_INDEXES;
static int ready(void) {
    tg_native_guard_lock();
    if(key==TLS_OUT_OF_INDEXES) key=TlsAlloc();
    int ok=key!=TLS_OUT_OF_INDEXES;
    tg_native_guard_unlock(); return ok;
}
static void *get(void) { return TlsGetValue(key); }
static int put(void *p) { return TlsSetValue(key,p)!=0; }
#elif defined(_WIN32)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <windows.h>
static INIT_ONCE once=INIT_ONCE_STATIC_INIT;
static DWORD key=FLS_OUT_OF_INDEXES;
static VOID CALLBACK cleanup(PVOID p) { tg_notifier_destroy(p); }
static BOOL CALLBACK init(PINIT_ONCE o,PVOID p,PVOID *c) {
    (void)o; (void)p; (void)c;
    key=FlsAlloc(cleanup); return key!=FLS_OUT_OF_INDEXES;
}
static int ready(void) { return InitOnceExecuteOnce(&once,init,NULL,NULL)!=0; }
static void *get(void) { return FlsGetValue(key); }
static int put(void *p) { return FlsSetValue(key,p)!=0; }
#else
#include <pthread.h>
static pthread_once_t once=PTHREAD_ONCE_INIT;
static pthread_key_t key;
static int valid;
static void init(void) { valid=pthread_key_create(&key,tg_notifier_destroy)==0; }
static int ready(void) { return pthread_once(&once,init)==0&&valid; }
static void *get(void) { return pthread_getspecific(key); }
static int put(void *p) { return pthread_setspecific(key,p)==0; }
#endif
void tg_notifier_destroy(void *state) {
    tg_notifier *s=state;
    if(s) {
        tg_notifier_unregister(s);
        tg_thread_event_clear(s);
        if(s->active_retired) Tcl_Free((char *)s->active_event);
        tg_idle_clear(s);
        tg_source_clear(s);
        tg_native_condition_destroy(s->wake);
        tg_native_mutex_destroy(s->mailbox);
        tg_timers_destroy(s->queue); free(s);
    }
}
tg_notifier *tg_notifier_get(int create) {
    if(!ready()) return NULL;
    tg_notifier *s=get();
    if(s||!create) return s;
    s=calloc(1,sizeof(*s));
    if(!s) return NULL;
    s->queue=tg_timers_create(); s->next=1; s->source_wait=-1;
    s->owner=(Tcl_ThreadId)tg_native_thread_id();
    s->mailbox=tg_native_mutex_create();
    s->wake=tg_native_condition_create();
    if(!s->queue||!s->mailbox||!s->wake||!put(s)) { tg_notifier_destroy(s); return NULL; }
    tg_notifier_register(s);
    return s;
}

int tg_notifier_finalize_current(void) {
    if(!ready()) return 0;
    tg_notifier *s=get();
    if(s&&(s->busy||s->delete_guards)) return 0;
    if(s) {
        if(!put(NULL)) return 0;
        tg_notifier_destroy(s);
    }
    return 1;
}
