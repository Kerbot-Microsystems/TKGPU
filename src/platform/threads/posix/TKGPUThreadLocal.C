#include "../TKGPUThreadNative.H"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
struct tg_native_tls { pthread_key_t key; };
tg_native_tls *tg_native_tls_create(void) {
    tg_native_tls *p=malloc(sizeof *p); if(!p) abort();
    if(pthread_key_create(&p->key,free)!=0) abort();
    return p;
}
void *tg_native_tls_get(tg_native_tls *p) { return pthread_getspecific(p->key); }
int tg_native_tls_set(tg_native_tls *p,void *v) { return pthread_setspecific(p->key,v)==0; }
void *tg_native_thread_id(void) {
    pthread_t self=pthread_self();
    uintptr_t id=0;
    if(sizeof self>sizeof id) abort();
    memcpy(&id,&self,sizeof self);
    return (void *)id;
}
