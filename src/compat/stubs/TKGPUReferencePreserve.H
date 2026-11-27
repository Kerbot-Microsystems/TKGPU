/* Preserve records guarded by a mutex. User callbacks run after unlinking. */
#include <pthread.h>
#include <limits.h>
#include <stdlib.h>
#include <stdint.h>
#ifndef TKGPU_PRESERVE_API
#define TKGPU_PRESERVE_API(n) Tcl_##n
#endif
typedef struct tg_preserve_record {
    struct tg_preserve_record *next;
    void *data;
    unsigned count;
    Tcl_FreeProc *pending;
} tg_preserve_record;
static pthread_mutex_t preserve_lock=PTHREAD_MUTEX_INITIALIZER;
enum { PRESERVE_BUCKETS=256 };
static tg_preserve_record *preserve_records[PRESERVE_BUCKETS];
static tg_preserve_record **preserve_find(void *data) {
    uintptr_t key=(uintptr_t)data;
    key^=key>>4; key^=key>>9;
    tg_preserve_record **link=&preserve_records[key&(PRESERVE_BUCKETS-1)];
    while(*link&&(*link)->data!=data) link=&(*link)->next;
    return link;
}
static void preserve_free(void *data,Tcl_FreeProc *proc) {
    if(proc==TCL_DYNAMIC) Tcl_Free(data);
    else if(proc&&proc!=TCL_VOLATILE) proc(data);
    else Tcl_Panic("invalid eventual free callback");
}
void TKGPU_PRESERVE_API(Preserve)(void *data) {
    pthread_mutex_lock(&preserve_lock);
    tg_preserve_record **link=preserve_find(data),*r=*link;
    if(!r) {
        r=calloc(1,sizeof *r);
        if(!r) Tcl_Panic("preserve allocation failed");
        r->data=data; *link=r;
    }
    if(r->count==UINT_MAX) Tcl_Panic("preserve count overflow");
    ++r->count;
    pthread_mutex_unlock(&preserve_lock);
}
void TKGPU_PRESERVE_API(Release)(void *data) {
    pthread_mutex_lock(&preserve_lock);
    tg_preserve_record **link=preserve_find(data),*r=*link;
    if(!r||!r->count) Tcl_Panic("release without preserve");
    Tcl_FreeProc *proc=NULL;
    if(!--r->count) { *link=r->next; proc=r->pending; free(r); }
    pthread_mutex_unlock(&preserve_lock);
    if(proc) preserve_free(data,proc);
}
void TKGPU_PRESERVE_API(EventuallyFree)(void *data,Tcl_FreeProc *proc) {
    if(!proc||proc==TCL_VOLATILE) Tcl_Panic("invalid eventual free callback");
    pthread_mutex_lock(&preserve_lock);
    tg_preserve_record **link=preserve_find(data),*r=*link;
    int deferred=r!=NULL;
    if(deferred) {
        if(r->pending) Tcl_Panic("duplicate eventual free");
        r->pending=proc;
    }
    pthread_mutex_unlock(&preserve_lock);
    if(!deferred) preserve_free(data,proc);
}
#undef TKGPU_PRESERVE_API
