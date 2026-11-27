#ifndef TKGPU_ABI_OBJECT_H
#define TKGPU_ABI_OBJECT_H
#include <stdint.h>
/* Tcl 8.6 public object-prefix contract, independently implemented from
   the documented C interface. This is not the complete Tcl ABI. */
typedef struct tg_object Tcl_Obj;
typedef struct TKGPUAbiObjectType Tcl_ObjType;
struct tg_interp;
struct TKGPUAbiObjectType {
    const char *name;
    void (*freeIntRepProc)(Tcl_Obj *);
    void (*dupIntRepProc)(Tcl_Obj *,Tcl_Obj *);
    void (*updateStringProc)(Tcl_Obj *);
    int (*setFromAnyProc)(struct tg_interp *,Tcl_Obj *);
};
typedef union TKGPUAbiInternalRep {
    long integer;
    double real;
    void *pointer;
    int64_t wide;
    struct { void *first,*second; } pointers;
    struct { void *pointer; unsigned long value; } pointerAndLong;
} TKGPUAbiInternalRep;
/* Keep this member sequence at offset zero of every runtime object. */
#define TKGPU_ABI_OBJECT_FIELDS \
    int refCount; \
    char *bytes; \
    int length; \
    const Tcl_ObjType *typePtr; \
    TKGPUAbiInternalRep internalRep
typedef struct TKGPUAbiObjectPrefix { TKGPU_ABI_OBJECT_FIELDS; } TKGPUAbiObjectPrefix;
#endif
