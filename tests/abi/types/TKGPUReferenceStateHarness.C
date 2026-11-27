#include <tcl.h>
#include <stdlib.h>
#define STATE_API(n) candidate_##n
#define STATE_GET_ERROR(i,k) Tcl_GetVar2Ex(i,k,NULL,1)
#define STATE_RESTORE_ERROR(i,k,v) do {if(v)Tcl_SetVar2Ex(i,k,NULL,v,1);}while(0)
#include "../../../src/compat/interp/TKGPUCompatState.H"
/* One channel-state fixture per thread, separate from stock Tcl storage. */
static __thread Tcl_Obj *pending;
#define CHANNEL_ERROR_API(n) candidate_##n
#define CHANNEL_ERROR_FIELD(i) pending
#include "../../../src/compat/interp/TKGPUCompatChannelError.H"
