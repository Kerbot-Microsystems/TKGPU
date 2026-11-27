#include "TKGPUCompat.H"
#include "TKGPUObject.H"
#include "../core/TKGPUCoreInternal.H"
#include <string.h>
#ifdef TKGPU_REFERENCE_STUBS
extern int Tcl_AsyncReady(void);
extern int Tcl_AsyncInvoke(Tcl_Interp *,int);
#endif
int Tcl_EvalObjv(Tcl_Interp *i,int count,Tcl_Obj *const objects[],int flags) {
    (void)flags;
    if(!i||count<0) return TG_ERROR;
    if(i->deleted&&!i->disposing) return tg_set_result(i,"attempt to call eval in deleted interpreter",TG_ERROR);
    if(!count) return tg_set_result(i,"",TG_OK);
#ifdef TKGPU_REFERENCE_STUBS
    tg_compat_lifetime_enter(i);
    int rc=tg_call_objects(i,(size_t)count,objects);
    if(!i->deleted&&!i->disposing&&Tcl_AsyncReady()) rc=Tcl_AsyncInvoke(i,rc);
    tg_compat_lifetime_leave(i);
    return rc;
#else
    return tg_call_objects(i,(size_t)count,objects);
#endif
}
Tcl_Obj *TKGPU_CopyResultObject(Tcl_Interp *i) {
    if(!i) return NULL;
    const char *s=tg_result(i);
    return tg_object_string(s,strlen(s));
}
int TKGPU_SetResultObject(Tcl_Interp *i,Tcl_Obj *o) {
    size_t n; const char *s=tg_object_bytes(o,&n);
    if(!i||i->disposing||!s||n>=MAX_WORD||memchr(s,0,n)) return TG_ERROR;
    return tg_set_result(i,s,TG_OK);
}
