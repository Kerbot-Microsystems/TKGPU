#include "TKGPUCompat.H"
#include "TKGPUObject.H"
#include "../core/TKGPUCoreInternal.H"
#include <string.h>
int Tcl_GetBooleanFromObj(Tcl_Interp *i,Tcl_Obj *o,int *value) {
    size_t n; const char *s=tg_object_bytes(o,&n);
    if(!s||!value||memchr(s,0,n)) {
        if(i) tg_set_result(i,"invalid boolean object",TG_ERROR);
        return TG_ERROR;
    }
    return Tcl_GetBoolean(i,s,value);
}
