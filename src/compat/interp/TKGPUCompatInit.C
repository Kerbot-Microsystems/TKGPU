#include "TKGPUCompat.H"
#include "../../core/TKGPUCoreInternal.H"
#include "../../platform/process/TKGPUExecutable.H"
#include "../../platform/threads/TKGPUThreadNative.H"
#include <string.h>
#ifndef TKGPU_BUILD_VERSION
#define TKGPU_BUILD_VERSION "preview"
#endif
static char executable[MAX_WORD];
void Tcl_FindExecutable(const char *argv0) {
    char path[MAX_WORD];
    if(!tg_executable_path(path,sizeof path)) {
        size_t n=argv0?strlen(argv0):0;
        if(n>=sizeof path) return;
        if(n) memcpy(path,argv0,n);
        path[n]=0;
    }
    tg_native_guard_lock();
    memcpy(executable,path,strlen(path)+1);
    tg_native_guard_unlock();
}
/* The published path can be changed by FindExecutable. Take a protected
   snapshot in thread-local storage instead of exposing the mutable array.
   The returned string is borrowed until the next lookup in this thread. */
const char *Tcl_GetNameOfExecutable(void) {
    char path[MAX_WORD];
    static Tcl_ThreadDataKey key;
    tg_native_guard_lock();
    memcpy(path,executable,strlen(executable)+1);
    tg_native_guard_unlock();
    if(!path[0]) return NULL;
    char *snapshot=Tcl_GetThreadData(&key,MAX_WORD);
    memcpy(snapshot,path,strlen(path)+1);
    return snapshot;
}
int Tcl_Init(Tcl_Interp *i) {
    if(!i||i->depth||i->disposing) return TG_ERROR;
    if(i->tcl_initialized) return tg_set_result(i,"",TG_OK);
    char path[MAX_WORD];
    tg_native_guard_lock();
    memcpy(path,executable,strlen(executable)+1);
    tg_native_guard_unlock();
    if(!path[0]) (void)tg_executable_path(path,sizeof path);
    if(!Tcl_SetVar2(i,"tcl_version",NULL,"8.6",TKGPU_LEAVE_ERR_MSG)||
       !Tcl_SetVar2(i,"tkgpu_version",NULL,TKGPU_BUILD_VERSION,TKGPU_LEAVE_ERR_MSG)||
       !Tcl_SetVar2(i,"tcl_platform","threaded","1",TKGPU_LEAVE_ERR_MSG)||
       !Tcl_SetVar2(i,"tkgpu_executable",NULL,path,TKGPU_LEAVE_ERR_MSG)||
       !Tcl_SetVar2(i,"ttk::currentTheme",NULL,"clam",TKGPU_LEAVE_ERR_MSG))
        return tg_set_result(i,"preview initialization failed",TG_ERROR);
    i->tcl_initialized=1; return tg_set_result(i,"",TG_OK);
}
