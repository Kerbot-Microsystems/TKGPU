#include "TKGPUHost.H"
#include "TKGPUCompat.H"
#include "../core/TKGPUCoreInternal.H"
#include "../platform/threads/TKGPUThreadNative.H"
#ifndef TKGPU_BUILD_VERSION
#define TKGPU_BUILD_VERSION "preview"
#endif
unsigned TKGPU_HostABI(void) { return TKGPU_HOST_ABI; }
unsigned TKGPU_HostABIRevision(void) { return TKGPU_HOST_ABI_REVISION; }
int TKGPU_HostIsOwner(const TKGPUHost *h) { return h&&h->host_owner==tg_native_thread_id(); }
const char *TKGPU_HostVersion(void) { return TKGPU_BUILD_VERSION; }
/* Reuse the internal runtime initialization; Tcl-named entry points are
   implementation details, not imports from a third-party binary. */
TKGPUHost *TKGPU_HostCreate(void) {
    TKGPUHost *h=Tcl_CreateInterp();
    if(h) h->host_owner=tg_native_thread_id();
    return h;
}
int TKGPU_HostInit(TKGPUHost *h,int gui) {
    if(!TKGPU_HostIsOwner(h)) return TKGPU_HOST_ERROR;
    return gui?Tk_Init(h):Tcl_Init(h);
}
int TKGPU_HostClose(TKGPUHost *h) {
    if(!TKGPU_HostIsOwner(h)||h->depth||h->disposing||h->compat_callback_depth||h->event_pump_depth)
        return TKGPU_HOST_ERROR;
    tg_destroy(h); return TKGPU_HOST_OK;
}
int TKGPU_HostCall(TKGPUHost *h,size_t n,const char *const v[]) { return TKGPU_HostIsOwner(h)?tg_call(h,n,v):TKGPU_HOST_ERROR; }
const char *TKGPU_HostCallResult(TKGPUHost *h,size_t n,const char *const v[],int *status) {
    if(!status) return NULL;
    *status=TKGPU_HOST_ERROR;
    if(!TKGPU_HostIsOwner(h)) return NULL;
    *status=tg_call(h,n,v);
    return tg_result(h);
}
const char *TKGPU_HostCallTagged(TKGPUHost *h,size_t n,const char *const v[],int *status,int *kind) {
    if(!status||!kind) return NULL;
    *kind=TKGPU_HOST_STRING;
    const char *result=TKGPU_HostCallResult(h,n,v,status);
    if(result&&*status==TKGPU_HOST_OK) *kind=h->result_kind;
    return result;
}
int TKGPU_HostEval(TKGPUHost *h,const char *s) { return TKGPU_HostIsOwner(h)?tg_eval(h,s):TKGPU_HOST_ERROR; }
int TKGPU_HostEvalFile(TKGPUHost *h,const char *s) { return TKGPU_HostIsOwner(h)?Tcl_EvalFile(h,s):TKGPU_HOST_ERROR; }
const char *TKGPU_HostResult(const TKGPUHost *h) { return TKGPU_HostIsOwner(h)?tg_result(h):NULL; }
int TKGPU_HostSetResult(TKGPUHost *h,const char *s) { return TKGPU_HostIsOwner(h)?tg_command_result(h,s):TKGPU_HOST_ERROR; }
int TKGPU_HostSplit(TKGPUHost *h,const char *s,int *n,const char ***v) {
    return TKGPU_HostIsOwner(h)?Tcl_SplitList(h,s,n,v):TKGPU_HOST_ERROR;
}
void TKGPU_HostFree(void *p) { Tcl_Free((char *)p); }
int TKGPU_HostEvent(int flags) { return Tcl_DoOneEvent(flags); }
int TKGPU_HostWindows(void) { return Tk_GetNumMainWindows(); }
