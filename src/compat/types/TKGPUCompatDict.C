#include "TKGPUCompat.H"
#include "../../object/TKGPUObjectInternal.H"
#include "TKGPUCompatTypes.H"
#define Tcl_IncrRefCount(o) do { if(!tg_object_retain(o)) Tcl_Panic("dictionary reference overflow"); } while(0)
#define Tcl_DecrRefCount(o) tg_object_release(o)
#include "TKGPUCompatDict.H"
