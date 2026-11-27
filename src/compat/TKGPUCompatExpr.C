#include "TKGPUCompat.H"
#include "../core/TKGPUCoreInternal.H"
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
static int evaluate(Tcl_Interp *i,const char *s,int64_t *v) {
    if(!i||i->disposing||!s||!v) return TG_ERROR;
    if(!tg_eval_condition(i,s,v))
        return tg_set_result(i,"unsupported or invalid integer expression",TG_ERROR);
    return TG_OK;
}
int Tcl_ExprLong(Tcl_Interp *i,const char *s,long *v) {
    int64_t n;
    if(!v||evaluate(i,s,&n)!=TG_OK) return TG_ERROR;
    if(n<LONG_MIN||n>LONG_MAX) return tg_set_result(i,"long overflow",TG_ERROR);
    *v=(long)n; return TG_OK;
}
int Tcl_ExprBoolean(Tcl_Interp *i,const char *s,int *v) {
    int64_t n;
    if(!v||evaluate(i,s,&n)!=TG_OK) return TG_ERROR;
    *v=n!=0; return TG_OK;
}
int Tcl_ExprString(Tcl_Interp *i,const char *s) {
    if(!i||i->disposing||!s) return TG_ERROR;
    return tg_eval_numeric_result(i,s);
}

static const char *expression_text(Tcl_Interp *i,Tcl_Obj *o) {
    size_t n; const char *s=tg_object_bytes(o,&n);
    if(!s||n>=MAX_WORD||memchr(s,0,n)) {
        if(i) tg_set_result(i,"unsupported expression object",TG_ERROR);
        return NULL;
    }
    return s;
}
int Tcl_ExprLongObj(Tcl_Interp *i,Tcl_Obj *o,long *out) {
    const char *s=expression_text(i,o);
    return s?Tcl_ExprLong(i,s,out):TG_ERROR;
}
int Tcl_ExprDoubleObj(Tcl_Interp *i,Tcl_Obj *o,double *out) {
    const char *s=expression_text(i,o);
    return s?Tcl_ExprDouble(i,s,out):TG_ERROR;
}
int Tcl_ExprBooleanObj(Tcl_Interp *i,Tcl_Obj *o,int *out) {
    const char *s=expression_text(i,o);
    return s?Tcl_ExprBoolean(i,s,out):TG_ERROR;
}

/* ExprObj returns an owned reference and preserves the previous successful
   interpreter result. An evaluation failure instead retains its diagnostic. */
int Tcl_ExprObj(Tcl_Interp *i,Tcl_Obj *o,Tcl_Obj **out) {
    if(!i||i->disposing||!out) return TG_ERROR;
    const char *s=expression_text(i,o); if(!s) return TG_ERROR;
    char expression[MAX_WORD]; memcpy(expression,s,strlen(s)+1);
    Tcl_Obj *saved=TKGPU_RetainObject(Tcl_GetObjResult(i));
    if(!saved) return tg_set_result(i,"out of memory saving result",TG_ERROR);
    int kind=i->result_kind;
    int rc=tg_eval_numeric_result(i,expression);
    if(rc!=TG_OK) { TKGPU_ReleaseObject(saved); return rc; }
    Tcl_Obj *value=TKGPU_RetainObject(Tcl_GetObjResult(i));
    if(!value) {
        TKGPU_ReleaseObject(saved);
        return tg_set_result(i,"out of memory creating expression result",TG_ERROR);
    }
    Tcl_SetObjResult(i,saved); i->result_kind=kind;
    TKGPU_ReleaseObject(saved);
    *out=value; return TG_OK;
}
