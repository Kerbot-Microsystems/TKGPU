#include "TKGPUCompatBignum.H"
#include "../../object/TKGPUObjectInternal.H"
#include "../../core/TKGPUCoreInternal.H"
#include "../types/TKGPUCompatTypes.H"
#include <stdlib.h>
#include <string.h>
#include <math.h>
Tcl_Obj *Tcl_NewBignumObj(mp_int *a) {
    char *s=tg_bn_decimal(a);
    if(!s) return NULL;
    Tcl_Obj *o=tg_object_string(s,strlen(s)); free(s);
    if(!o) return NULL;
    long long small;
    if(Tcl_GetWideIntFromObj(NULL,o,&small)==TG_OK) {
        tg_object_release(o); o=Tcl_NewWideIntObj(small);
        if(!o) return NULL;
    } else {
        o->typePtr=&tg_type_bignum;
        o=tg_compat_new_object(o);
    }
    TclBN_mp_clear(a); return o;
}
int Tcl_GetBignumFromObj(Tcl_Interp *i,Tcl_Obj *o,mp_int *out) {
    size_t n; const char *s=tg_object_bytes(o,&n);
    if(!out||!s) goto error;
    mp_int a;
    if(TclBN_mp_init(&a)!=TKGPU_MP_OKAY) goto error;
    if(tg_bn_read_tcl(&a,s,n)!=TKGPU_MP_OKAY) {
        TclBN_mp_clear(&a); goto error;
    }
    *out=a; return TG_OK;
error:
    if(i) tg_set_result(i,"expected supported integer object",TG_ERROR);
    return TG_ERROR;
}

Tcl_Obj *Tcl_DbNewBignumObj(mp_int *a,const char *file,int line) {
    (void)file; (void)line; return Tcl_NewBignumObj(a);
}
/* The preview's bignum representation is string-backed, so extraction
   already allocates independent limbs. No internal limb buffer can be stolen. */
int Tcl_TakeBignumFromObj(Tcl_Interp *i,Tcl_Obj *o,mp_int *out) {
    return Tcl_GetBignumFromObj(i,o,out);
}
#include "TKGPUCompatBnDouble.H"
