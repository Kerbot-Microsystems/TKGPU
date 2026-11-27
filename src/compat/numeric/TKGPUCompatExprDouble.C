#include "TKGPUCompat.H"
#include "../../core/TKGPUCoreInternal.H"
#include "../../core/numeric/TKGPURealExpr.H"
static const char *lookup(void *ctx,const char *name) {
    tg_interp *i=ctx; size_t k=tg_var_find(i,name);
    return k<i->count?i->vars[k].value:NULL;
}
int Tcl_ExprDouble(Tcl_Interp *i,const char *s,double *out) {
    if(!i||i->disposing||!s||!out) return TG_ERROR;
    int64_t integer;
    if(tg_eval_condition(i,s,&integer)) { *out=(double)integer; return TG_OK; }
    if(tg_expr_real(s,lookup,i,out)) return TG_OK;
    return tg_set_result(i,"unsupported or invalid real expression",TG_ERROR);
}
