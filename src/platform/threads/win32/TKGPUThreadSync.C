#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <windows.h>
#include "../TKGPUThreadNative.H"
#include <stdlib.h>
struct tg_native_mutex { SRWLOCK value; };
struct tg_native_condition { CONDITION_VARIABLE value; };
static SRWLOCK guard=SRWLOCK_INIT;
void tg_native_guard_lock(void) { AcquireSRWLockExclusive(&guard); }
void tg_native_guard_unlock(void) { ReleaseSRWLockExclusive(&guard); }
tg_native_mutex *tg_native_mutex_create(void) {
    tg_native_mutex *m=malloc(sizeof *m); if(!m) abort();
    InitializeSRWLock(&m->value); return m;
}
void tg_native_mutex_destroy(tg_native_mutex *m) { free(m); }
void tg_native_mutex_lock(tg_native_mutex *m) { AcquireSRWLockExclusive(&m->value); }
void tg_native_mutex_unlock(tg_native_mutex *m) { ReleaseSRWLockExclusive(&m->value); }
tg_native_condition *tg_native_condition_create(void) {
    tg_native_condition *c=malloc(sizeof *c); if(!c) abort();
    InitializeConditionVariable(&c->value); return c;
}
void tg_native_condition_destroy(tg_native_condition *c) { free(c); }
void tg_native_condition_notify(tg_native_condition *c) { WakeAllConditionVariable(&c->value); }
void tg_native_condition_wait(tg_native_condition *c,tg_native_mutex *m,int64_t ms) {
    DWORD wait=ms<0?INFINITE:(DWORD)(ms>86400000?86400000:ms);
    if(!SleepConditionVariableSRW(&c->value,&m->value,wait,0)&&GetLastError()!=ERROR_TIMEOUT) abort();
}
