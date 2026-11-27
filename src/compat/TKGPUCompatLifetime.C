#include "TKGPUCompat.H"
#include "../object/TKGPUObjectInternal.H"
/* Tcl's non-debug reference macros may invoke this function after the
   reference count reaches zero. Never decrement a second time. */
void TclFreeObj(Tcl_Obj *o) {
    if(o&&o->refCount<=0) tg_object_destroy(o);
}
Tcl_Obj *tg_compat_new_object(Tcl_Obj *o) {
    if(o) o->refCount=0;
    return o;
}
