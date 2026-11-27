#ifndef TKGPU_COMPAT_MUTEX_H
#define TKGPU_COMPAT_MUTEX_H
#include "TKGPUCompat.H"
#include "../../platform/threads/TKGPUThreadNative.H"
tg_native_mutex *tg_compat_mutex(Tcl_Mutex *key);
#endif
