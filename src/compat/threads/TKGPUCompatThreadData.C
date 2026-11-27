#include "TKGPUCompat.H"
#include "../events/TKGPUNotifier.H"
#include "../../platform/threads/TKGPUThreadNative.H"
#include <stdlib.h>
typedef struct { tg_native_tls *native; int size; } data_key;
Tcl_ThreadId Tcl_GetCurrentThread(void) {
    tg_notifier *s=tg_notifier_get(1);
    return s?s->owner:(Tcl_ThreadId)tg_native_thread_id();
}
void *Tcl_GetThreadData(Tcl_ThreadDataKey *key,int size) {
    if(!key||size<=0) abort();
    tg_native_guard_lock();
    data_key *k=(data_key *)*key;
    if(!k) {
        k=malloc(sizeof *k); if(!k) abort();
        k->native=tg_native_tls_create(); k->size=size;
        *key=(Tcl_ThreadDataKey)k;
    }
    if(size>k->size) abort();
    tg_native_guard_unlock();
    void *p=tg_native_tls_get(k->native);
    if(!p) {
        p=calloc(1,(size_t)k->size);
        if(!p||!tg_native_tls_set(k->native,p)) abort();
    }
    return p;
}
