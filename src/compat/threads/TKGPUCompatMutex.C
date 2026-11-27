#include "TKGPUCompatMutex.H"
#include <stdlib.h>
tg_native_mutex *tg_compat_mutex(Tcl_Mutex *key) {
    if(!key) abort();
    tg_native_guard_lock();
    if(!*key) *key=(Tcl_Mutex)tg_native_mutex_create();
    tg_native_mutex *m=(tg_native_mutex *)*key;
    tg_native_guard_unlock();
    return m;
}
void Tcl_MutexLock(Tcl_Mutex *key) { tg_native_mutex_lock(tg_compat_mutex(key)); }
void Tcl_MutexUnlock(Tcl_Mutex *key) {
    if(!key||!*key) abort();
    tg_native_mutex_unlock((tg_native_mutex *)*key);
}
/* Caller must ensure all users are quiescent before finalization. */
void Tcl_MutexFinalize(Tcl_Mutex *key) {
    if(!key) return;
    tg_native_guard_lock();
    tg_native_mutex *m=(tg_native_mutex *)*key; *key=NULL;
    tg_native_guard_unlock();
    tg_native_mutex_destroy(m);
}
