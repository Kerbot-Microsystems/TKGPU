#define _POSIX_C_SOURCE 200809L
#include "../TKGPUThreadNative.H"
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
struct tg_native_mutex { pthread_mutex_t value; };
struct tg_native_condition { pthread_cond_t value; clockid_t clock; };
static pthread_mutex_t guard=PTHREAD_MUTEX_INITIALIZER;
static void check(int rc) { if(rc) abort(); }
void tg_native_guard_lock(void) { check(pthread_mutex_lock(&guard)); }
void tg_native_guard_unlock(void) { check(pthread_mutex_unlock(&guard)); }
tg_native_mutex *tg_native_mutex_create(void) {
    tg_native_mutex *m=malloc(sizeof *m);
    if(!m) abort();
    check(pthread_mutex_init(&m->value,NULL)); return m;
}
void tg_native_mutex_destroy(tg_native_mutex *m) {
    if(m) { check(pthread_mutex_destroy(&m->value)); free(m); }
}
void tg_native_mutex_lock(tg_native_mutex *m) { check(pthread_mutex_lock(&m->value)); }
void tg_native_mutex_unlock(tg_native_mutex *m) { check(pthread_mutex_unlock(&m->value)); }
tg_native_condition *tg_native_condition_create(void) {
    tg_native_condition *c=malloc(sizeof *c);
    if(!c) abort();
    c->clock=CLOCK_REALTIME;
    pthread_condattr_t attr; check(pthread_condattr_init(&attr));
#if defined(__linux__)
    /* Relative waits must not jump if wall time is adjusted. */
    if(pthread_condattr_setclock(&attr,CLOCK_MONOTONIC)==0) c->clock=CLOCK_MONOTONIC;
#endif
    check(pthread_cond_init(&c->value,&attr));
    check(pthread_condattr_destroy(&attr)); return c;
}
void tg_native_condition_destroy(tg_native_condition *c) {
    if(c) { check(pthread_cond_destroy(&c->value)); free(c); }
}
void tg_native_condition_notify(tg_native_condition *c) { check(pthread_cond_broadcast(&c->value)); }
void tg_native_condition_wait(tg_native_condition *c,tg_native_mutex *m,int64_t ms) {
    if(ms<0) { check(pthread_cond_wait(&c->value,&m->value)); return; }
    struct timespec t;
    if(clock_gettime(c->clock,&t)!=0) abort();
    /* Bound one native wait to one day, avoiding time_t overflow on legacy targets. */
    if(ms>86400000) ms=86400000;
    if(t.tv_sec<0||(uint64_t)t.tv_sec>INT64_MAX-86401) abort();
    t.tv_sec+=(time_t)(ms/1000);
    t.tv_nsec+=(long)(ms%1000)*1000000L;
    if(t.tv_nsec>=1000000000L) { t.tv_sec++; t.tv_nsec-=1000000000L; }
    int rc=pthread_cond_timedwait(&c->value,&m->value,&t);
    if(rc!=ETIMEDOUT) check(rc);
}
