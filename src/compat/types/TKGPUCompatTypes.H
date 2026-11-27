#ifndef TKGPU_COMPAT_TYPES_H
#define TKGPU_COMPAT_TYPES_H
#include "TKGPUAbiObject.H"
extern const Tcl_ObjType tg_type_int,tg_type_wide,tg_type_double,tg_type_list,tg_type_bytes,tg_type_bignum,tg_type_dict;
void tg_bytearray_update_string(Tcl_Obj *);
#endif
