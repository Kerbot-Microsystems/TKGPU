#include "TKGPUCompat.H"
#include "../../core/TKGPUCoreInternal.H"
#include "../../core/history/TKGPUHistory.H"
#include <stdlib.h>
#include <string.h>
int Tcl_RecordAndEval(Tcl_Interp *i,const char *s,int flags) {
    if(!i||i->depth||i->disposing||!s||(flags&~(TKGPU_NO_EVAL|TKGPU_EVAL_GLOBAL))) return TG_ERROR;
    /* Copy borrowed result storage before recording or clearing it. */
    char *copy=tg_copy_string(s); if(!copy) return tg_set_result(i,"out of memory",TG_ERROR);
    if(!i->history) i->history=tg_history_create();
    int rc=TG_ERROR;
    if(!i->history||tg_history_record(i->history,copy)!=TG_OK)
        rc=tg_set_result(i,"history capacity or allocation failure",TG_ERROR);
    else if(flags&TKGPU_NO_EVAL) rc=tg_set_result(i,"",TG_OK);
    else rc=Tcl_Eval(i,copy);
    free(copy); return rc;
}

/* Reuse the existing history and execution path, with an explicit
   rejection of embedded NUL instead of silently recording a prefix. */
int Tcl_RecordAndEvalObj(Tcl_Interp *i,Tcl_Obj *obj,int flags) {
    if(!i||i->disposing||!obj) return TG_ERROR;
    int n=0; const char *script=Tcl_GetStringFromObj(obj,&n);
    if(!script||memchr(script,0,(size_t)n))
        return tg_set_result(i,"unsupported recorded script object",TG_ERROR);
    return Tcl_RecordAndEval(i,script,flags);
}
