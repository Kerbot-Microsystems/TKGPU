#include "TKGPUCompat.H"
#include "TKGPUCommand.H"
#include "../core/TKGPUCoreInternal.H"
#include "events/script/TKGPUAfter.H"
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdarg.h>
int tg_compat_eval_check(Tcl_Interp *);
Tcl_Interp *Tcl_CreateInterp(void) {
    Tcl_Interp *i=tg_create();
    if(i) {
        i->eval_check=tg_compat_eval_check;
        i->event_command=tg_after_command;
        i->event_dispose=tg_after_dispose;
        tg_var_store(i,"tcl_platform(threaded)","1");
        tg_var_store(i,"ttk::currentTheme","clam");
        tg_set_result(i,"",TG_OK);
    }
    return i;
}
#include "interp/TKGPUCompatInterpLifetime.H"
const char *Tcl_GetStringResult(Tcl_Interp *interp) {
    if(interp&&interp->compat_result) interp->compat_legacy_lookup=1;
    return tg_result(interp);
}
void Tcl_ResetResult(Tcl_Interp *interp) { (void)tg_command_result(interp,""); }

#ifdef TKGPU_REFERENCE_STUBS
const void *tg_reference_stubs_table(const char *const *,size_t);
#endif
int TKGPU_EnableReferenceStubs(Tcl_Interp *i,const char *const *required,size_t count) {
    if(!i||i->disposing||i->depth) return TG_ERROR;
#ifdef TKGPU_REFERENCE_STUBS
    const void *table=tg_reference_stubs_table(required,count);
    if(!table) return tg_set_result(i,"unsupported reference Stubs slot manifest",TG_ERROR);
    i->legacy_stub_table=table;
    return tg_set_result(i,"",TG_OK);
#else
    (void)required; (void)count;
    return tg_set_result(i,"reference Stubs preview not built",TG_ERROR);
#endif
}

int Tcl_InterpActive(Tcl_Interp *i) { return i&&i->depth>0; }

int Tcl_GetErrorLine(Tcl_Interp *i) { return i?i->legacy_error_line:0; }
void Tcl_SetErrorLine(Tcl_Interp *i,int line) { if(i) i->legacy_error_line=line; }

/* Explicit-length evaluation with the same restrictions as the core
   interpreter. Never silently truncate a script at an embedded NUL. */
int Tcl_EvalEx(Tcl_Interp *i,const char *script,int length,int flags) {
    if(!i||i->disposing) return TG_ERROR;
    if(!script||length< -1||(flags&~(TKGPU_EVAL_GLOBAL|TKGPU_EVAL_DIRECT)))
        return tg_set_result(i,"unsupported EvalEx arguments or flags",TG_ERROR);
    size_t n=length<0?strlen(script):(size_t)length;
    if(memchr(script,0,n)) return tg_set_result(i,"embedded NUL in script is unsupported",TG_ERROR);
    char *owned=malloc(n+1);
    if(!owned) return tg_set_result(i,"out of memory",TG_ERROR);
    memcpy(owned,script,n); owned[n]=0;
    int rc=Tcl_Eval(i,owned); free(owned); return rc;
}
int Tcl_EvalObjEx(Tcl_Interp *i,Tcl_Obj *o,int flags) {
    if(!i||i->disposing) return TG_ERROR;
    int n=0; const char *s=Tcl_GetStringFromObj(o,&n);
    if(!s) return tg_set_result(i,"invalid script object",TG_ERROR);
    return Tcl_EvalEx(i,s,n,flags);
}

void Tcl_SetAssocData(Tcl_Interp *i,const char *key,Tcl_InterpDeleteProc *proc,void *data) {
    if(!i||i->disposing||!key) return;
    if(tg_assoc_set(i,key,proc,data)!=TG_OK) Tcl_Panic("association allocation failed");
}
void *Tcl_GetAssocData(Tcl_Interp *i,const char *key,Tcl_InterpDeleteProc **proc) {
    return tg_assoc_get(i,key,proc);
}
void Tcl_DeleteAssocData(Tcl_Interp *i,const char *key) {
#ifdef TKGPU_REFERENCE_STUBS
    if(!i) return;
    tg_compat_lifetime_enter(i);
    tg_assoc_delete(i,key);
    tg_compat_lifetime_leave(i);
#else
    tg_assoc_delete(i,key);
#endif
}
void Tcl_CallWhenDeleted(Tcl_Interp *i,Tcl_InterpDeleteProc *proc,void *data) {
    if(!i||i->disposing||!proc) return;
    if(tg_delete_hook_add(i,proc,data)!=TG_OK) Tcl_Panic("delete callback allocation failed");
}
void Tcl_DontCallWhenDeleted(Tcl_Interp *i,Tcl_InterpDeleteProc *proc,void *data) {
    tg_delete_hook_remove(i,proc,data);
}

/* This interpreter has only a global variable frame. Reuse its evaluation
   path rather than inventing a second parser or silently losing arguments. */
int Tcl_GlobalEval(Tcl_Interp *i,const char *script) {
    return Tcl_EvalEx(i,script,-1,TKGPU_EVAL_GLOBAL);
}
int Tcl_GlobalEvalObj(Tcl_Interp *i,Tcl_Obj *script) {
    return Tcl_EvalObjEx(i,script,TKGPU_EVAL_GLOBAL);
}
int Tcl_VarEvalVA(Tcl_Interp *i,va_list source) {
    if(!i||i->disposing) return TG_ERROR;
    va_list args; va_copy(args,source);
    size_t total=0; const char *part;
    while((part=va_arg(args,const char *))!=NULL) {
        size_t n=strlen(part);
        if(n>(size_t)INT_MAX-total) {
            va_end(args); return tg_set_result(i,"script length overflow",TG_ERROR);
        }
        total+=n;
    }
    va_end(args);
    char *script=malloc(total+1);
    if(!script) return tg_set_result(i,"out of memory",TG_ERROR);
    va_copy(args,source); size_t used=0;
    while((part=va_arg(args,const char *))!=NULL) {
        size_t n=strlen(part); memcpy(script+used,part,n); used+=n;
    }
    va_end(args); script[used]=0;
    int rc=Tcl_Eval(i,script); free(script); return rc;
}
int Tcl_VarEval(Tcl_Interp *i,...) {
    va_list args; va_start(args,i);
    int rc=Tcl_VarEvalVA(i,args); va_end(args); return rc;
}

/* Legacy object evaluation uses the same bounded evaluator as EvalObjEx. */
int Tcl_EvalObj(Tcl_Interp *i,Tcl_Obj *o) { return Tcl_EvalObjEx(i,o,0); }

/* Non-positive requests query the existing limit, as in Tcl. The default
   stays conservative for this runtime's native C evaluation stack. */
int Tcl_SetRecursionLimit(Tcl_Interp *i,int depth) {
    if(!i) return 0;
    int previous=(int)i->recursion_limit;
    if(depth>0) i->recursion_limit=(unsigned)depth;
    return previous;
}
