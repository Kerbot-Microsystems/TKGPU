#ifndef TKGPU_FACADE_TEST_H
#define TKGPU_FACADE_TEST_H
/* Compile the same behavioral tests against the independent facade and an
   actual extension-style consumer of the target Tcl 8.6 Stubs layout. */
#ifdef TKGPU_TEST_REFERENCE_STUBS
#define USE_TCL_STUBS
#include <tcl.h>
#include <stdio.h>
#include <stdlib.h>
#undef Tcl_CreateInterp
#undef Tcl_DeleteInterp
extern Tcl_Interp *Tcl_CreateInterp(void);
extern void Tcl_DeleteInterp(Tcl_Interp *);
extern int TKGPU_EnableReferenceStubs(Tcl_Interp *,const char *const *,size_t);
static Tcl_Interp *tg_test_create_interp(void) {
    static const char *const required[]={"Tcl_PkgRequireEx","TclFreeObj",
#define TKGPU_SLOT(name) "Tcl_" #name,
#include "../../src/compat/stubs/TKGPUReferenceSlots.H"
#undef TKGPU_SLOT
    };
    Tcl_Interp *i=Tcl_CreateInterp();
    if(!i) return NULL;
    if(TKGPU_EnableReferenceStubs(i,required,sizeof required/sizeof required[0])!=TCL_OK||
       !Tcl_InitStubs(i,"8.6",0)) {
        fputs("Cannot initialize explicitly audited test Stubs consumer\n",stderr);
        exit(1);
    }
    return i;
}
#define Tcl_CreateInterp() tg_test_create_interp()
#define TKGPU_TEST_PREPARE() do { Tcl_Interp *bootstrap=tg_test_create_interp(); Tcl_DeleteInterp(bootstrap); } while(0)
static Tcl_Obj *tg_test_retain(Tcl_Obj *o) { if(o) Tcl_IncrRefCount(o); return o; }
#define TKGPU_RetainObject(o) tg_test_retain(o)
#define TKGPU_ReleaseObject(o) Tcl_DecrRefCount(o)
#define TKGPU_DONT_WAIT TCL_DONT_WAIT
#define TKGPU_TIMER_EVENTS TCL_TIMER_EVENTS
#define TKGPU_QUEUE_TAIL TCL_QUEUE_TAIL
#define TKGPU_QUEUE_MARK TCL_QUEUE_MARK
#define TKGPU_LOAD_GLOBAL TCL_LOAD_GLOBAL
#define TKGPU_LOAD_LAZY TCL_LOAD_LAZY
#define TKGPU_FINAL_RELEASE TCL_FINAL_RELEASE
#define TKGPU_CANCEL_UNWIND TCL_CANCEL_UNWIND
#define TKGPU_LEAVE_ERR_MSG TCL_LEAVE_ERR_MSG
#define TKGPU_GLOBAL_ONLY TCL_GLOBAL_ONLY
#define TKGPU_TRACE_READS TCL_TRACE_READS
#define TKGPU_TRACE_WRITES TCL_TRACE_WRITES
#define TKGPU_TRACE_UNSETS TCL_TRACE_UNSETS
#define TKGPU_TRACE_RENAME TCL_TRACE_RENAME
#define TKGPU_TRACE_DELETE TCL_TRACE_DELETE
#define TKGPU_LINK_INT TCL_LINK_INT
#define TKGPU_LINK_DOUBLE TCL_LINK_DOUBLE
#define TKGPU_LINK_BOOLEAN TCL_LINK_BOOLEAN
#else
#include "TKGPUCompat.H"
#define TKGPU_TEST_PREPARE() do {} while(0)
#endif
#endif
