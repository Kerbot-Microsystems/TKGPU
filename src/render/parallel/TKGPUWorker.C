#if defined(__linux__)
#define _GNU_SOURCE
#elif !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif
#include "TKGPUWorker.H"
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
struct tg_worker { HANDLE thread,work,done; tg_worker_fn fn; void *arg; int stop; };
static void await(HANDLE h) { if(WaitForSingleObject(h,INFINITE)!=WAIT_OBJECT_0) abort(); }
static void signal(HANDLE h) { if(!SetEvent(h)) abort(); }
static DWORD WINAPI entry(LPVOID p) {
    tg_worker *w=p;
    for(;;) {
        await(w->work); if(w->stop) return 0;
        w->fn(w->arg); signal(w->done);
    }
}
tg_worker *tg_worker_create(void) {
    tg_worker *w=calloc(1,sizeof *w); if(!w) return NULL;
    w->work=CreateEventA(NULL,FALSE,FALSE,NULL);
    w->done=CreateEventA(NULL,FALSE,FALSE,NULL);
    if(w->work&&w->done) w->thread=CreateThread(NULL,0,entry,w,0,NULL);
    if(w->thread) return w;
    if(w->work) CloseHandle(w->work);
    if(w->done) CloseHandle(w->done);
    free(w); return NULL;
}
void tg_worker_submit(tg_worker *w,tg_worker_fn fn,void *p) { w->fn=fn; w->arg=p; signal(w->work); }
void tg_worker_wait(tg_worker *w) { await(w->done); }
int tg_worker_try_wait(tg_worker *w) {
    DWORD rc=WaitForSingleObject(w->done,0);
    if(rc==WAIT_OBJECT_0) return 1;
    if(rc!=WAIT_TIMEOUT) abort();
    return 0;
}
void tg_worker_destroy(tg_worker *w) {
    w->stop=1; signal(w->work); await(w->thread);
    CloseHandle(w->thread); CloseHandle(w->work); CloseHandle(w->done); free(w);
}
unsigned tg_worker_cpu_count(void) { SYSTEM_INFO s; GetSystemInfo(&s); return s.dwNumberOfProcessors?s.dwNumberOfProcessors:1u; }
#else
#include <pthread.h>
#include <unistd.h>
#ifdef __linux__
#include <sched.h>
#endif
struct tg_worker { pthread_t thread; pthread_mutex_t lock; pthread_cond_t wake; tg_worker_fn fn; void *arg; int state; };
static void check(int rc) { if(rc) abort(); }
static void *entry(void *p) {
    tg_worker *w=p; check(pthread_mutex_lock(&w->lock));
    for(;;) {
        while(w->state==0||w->state==2) check(pthread_cond_wait(&w->wake,&w->lock));
        if(w->state==3) break;
        tg_worker_fn fn=w->fn; void *arg=w->arg;
        check(pthread_mutex_unlock(&w->lock)); fn(arg);
        check(pthread_mutex_lock(&w->lock)); w->state=2; check(pthread_cond_signal(&w->wake));
    }
    check(pthread_mutex_unlock(&w->lock)); return NULL;
}
tg_worker *tg_worker_create(void) {
    tg_worker *w=calloc(1,sizeof *w); if(!w) return NULL;
    if(pthread_mutex_init(&w->lock,NULL)) { free(w); return NULL; }
    if(pthread_cond_init(&w->wake,NULL)) { pthread_mutex_destroy(&w->lock); free(w); return NULL; }
    if(pthread_create(&w->thread,NULL,entry,w)) {
        pthread_cond_destroy(&w->wake); pthread_mutex_destroy(&w->lock); free(w); return NULL;
    }
    return w;
}
void tg_worker_submit(tg_worker *w,tg_worker_fn fn,void *p) {
    check(pthread_mutex_lock(&w->lock));
    if(w->state!=0) abort();
    w->fn=fn; w->arg=p; w->state=1; check(pthread_cond_signal(&w->wake));
    check(pthread_mutex_unlock(&w->lock));
}
void tg_worker_wait(tg_worker *w) {
    check(pthread_mutex_lock(&w->lock));
    while(w->state!=2) check(pthread_cond_wait(&w->wake,&w->lock));
    w->state=0; check(pthread_mutex_unlock(&w->lock));
}
int tg_worker_try_wait(tg_worker *w) {
    check(pthread_mutex_lock(&w->lock));
    int ready=w->state==2; if(ready) w->state=0;
    check(pthread_mutex_unlock(&w->lock)); return ready;
}
void tg_worker_destroy(tg_worker *w) {
    check(pthread_mutex_lock(&w->lock)); if(w->state!=0) abort();
    w->state=3; check(pthread_cond_signal(&w->wake)); check(pthread_mutex_unlock(&w->lock));
    check(pthread_join(w->thread,NULL)); check(pthread_cond_destroy(&w->wake)); check(pthread_mutex_destroy(&w->lock)); free(w);
}
unsigned tg_worker_cpu_count(void) {
#ifdef __linux__
    cpu_set_t set;
    if(sched_getaffinity(0,sizeof set,&set)==0) { int n=CPU_COUNT(&set); if(n>0) return (unsigned)n; }
#endif
    long n=sysconf(_SC_NPROCESSORS_ONLN); return n>0?(unsigned)n:1u;
}
#endif
