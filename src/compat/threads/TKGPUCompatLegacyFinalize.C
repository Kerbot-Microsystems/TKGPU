#include "TKGPUCompat.H"
#include "../events/TKGPUNotifier.H"
#include "../../platform/threads/TKGPUThreadNative.H"
int TKGPU_FinalizeLegacyThread(void) {
    /* Caller must have destroyed all interpreters on the current thread. */
    if(Tk_GetNumMainWindows()!=0) return 1;
    if(!tg_notifier_finalize_current()) return 1;
    tg_native_tls_finalize_thread(); return 0;
}
