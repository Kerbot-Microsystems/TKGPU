#include <windows.h>
#include "../TKGPUThreadNative.H"
#include <stdlib.h>
struct tg_native_tls { unsigned char identity; };
typedef struct slot { tg_native_tls *key; void *value; struct slot *next; } slot;
static DWORD index_key=TLS_OUT_OF_INDEXES;
static DWORD get_index(void) {
    tg_native_guard_lock();
    if(index_key==TLS_OUT_OF_INDEXES) index_key=TlsAlloc();
    DWORD key=index_key;
    tg_native_guard_unlock();
    if(key==TLS_OUT_OF_INDEXES) abort();
    return key;
}
tg_native_tls *tg_native_tls_create(void) {
    /* Called while the compatibility global guard is held. Do not acquire
       it here; allocate only a stable process-lifetime key identity. */
    tg_native_tls *key=calloc(1,sizeof *key); if(!key) abort();
    return key;
}
void *tg_native_tls_get(tg_native_tls *key) {
    for(slot *p=TlsGetValue(get_index());p;p=p->next)
        if(p->key==key) return p->value;
    return NULL;
}
int tg_native_tls_set(tg_native_tls *key,void *value) {
    DWORD index=get_index(); slot *head=TlsGetValue(index);
    for(slot *p=head;p;p=p->next) if(p->key==key) { p->value=value; return 1; }
    slot *p=malloc(sizeof *p); if(!p) return 0;
    *p=(slot){key,value,head};
    if(!TlsSetValue(index,p)) { free(p); return 0; }
    return 1;
}
void tg_native_tls_finalize_thread(void) {
    DWORD index=get_index(); slot *head=TlsGetValue(index);
    if(!TlsSetValue(index,NULL)) abort();
    while(head) { slot *p=head; head=p->next; free(p->value); free(p); }
}
void *tg_native_thread_id(void) { return (void *)(uintptr_t)GetCurrentThreadId(); }
