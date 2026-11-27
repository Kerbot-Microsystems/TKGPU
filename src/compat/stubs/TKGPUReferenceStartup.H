/* The Linux reference adapter keeps startup-script state per thread. */
#include <pthread.h>
#include <stdlib.h>
#ifndef TKGPU_STARTUP_API
#define TKGPU_STARTUP_API(n) Tcl_##n
#endif
typedef struct { Tcl_Obj *path; char *encoding; } tg_startup_state;
static pthread_once_t startup_once=PTHREAD_ONCE_INIT;
static pthread_key_t startup_key;
static void startup_cleanup(void *value) {
    tg_startup_state *s=value;
    if(!s) return;
    if(s->path) Tcl_DecrRefCount(s->path);
    free(s->encoding); free(s);
}
static void startup_init(void) {
    if(pthread_key_create(&startup_key,startup_cleanup)!=0) Tcl_Panic("startup thread key allocation failed");
}
static tg_startup_state *startup_get(void) {
    if(pthread_once(&startup_once,startup_init)!=0) Tcl_Panic("startup thread key initialization failed");
    return pthread_getspecific(startup_key);
}
void TKGPU_STARTUP_API(SetStartupScript)(Tcl_Obj *path,const char *encoding) {
    tg_startup_state *s=startup_get();
    if(!path&&!encoding) {
        if(s) {
            if(pthread_setspecific(startup_key,NULL)!=0) Tcl_Panic("startup thread state clear failed");
            startup_cleanup(s);
        }
        return;
    }
    char *copy=NULL;
    if(encoding) {
        size_t n=strlen(encoding);
        copy=malloc(n+1);
        if(!copy) Tcl_Panic("startup encoding allocation failed");
        memcpy(copy,encoding,n+1);
    }
    if(!s) {
        s=calloc(1,sizeof *s);
        if(!s||pthread_setspecific(startup_key,s)!=0) Tcl_Panic("startup thread state allocation failed");
    }
    if(path) Tcl_IncrRefCount(path);
    Tcl_Obj *previous=s->path; char *old_encoding=s->encoding;
    s->path=path; s->encoding=copy;
    if(previous) Tcl_DecrRefCount(previous);
    free(old_encoding);
}
Tcl_Obj *TKGPU_STARTUP_API(GetStartupScript)(const char **encoding) {
    tg_startup_state *s=startup_get();
    if(encoding) *encoding=s?s->encoding:NULL;
    return s?s->path:NULL;
}
#undef TKGPU_STARTUP_API
