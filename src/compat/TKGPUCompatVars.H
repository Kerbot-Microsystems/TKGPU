#ifndef TKGPU_COMPAT_VARS_H
#define TKGPU_COMPAT_VARS_H
#include "TKGPUCompat.H"
#include "../core/TKGPUCoreInternal.H"
/* Private facade helpers, not new public ABI entry points. */
int tg_compat_var_key(Tcl_Interp *,const char *,const char *,int,char *);
int tg_compat_var_store(Tcl_Interp *,const char *,const char *,tg_object *,int,var **);
#endif
