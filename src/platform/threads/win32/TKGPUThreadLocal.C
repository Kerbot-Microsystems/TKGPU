#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <windows.h>
#include "../TKGPUThreadNative.H"
#include <stdlib.h>
struct tg_native_tls { DWORD key; };
static VOID CALLBACK destroy(PVOID p) { free(p); }
tg_native_tls *tg_native_tls_create(void) {
    tg_native_tls *p=malloc(sizeof *p); if(!p) abort();
    p->key=FlsAlloc(destroy); if(p->key==FLS_OUT_OF_INDEXES) abort();
    return p;
}
void *tg_native_tls_get(tg_native_tls *p) { return FlsGetValue(p->key); }
int tg_native_tls_set(tg_native_tls *p,void *v) { return FlsSetValue(p->key,v)!=0; }
void *tg_native_thread_id(void) { return (void *)(uintptr_t)GetCurrentThreadId(); }
