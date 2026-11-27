#include <tcl.h>
#include <stdlib.h>
static Tcl_Namespace *g;static Tcl_Obj *exports;
#define NS_GLOBAL(i) g
#define NS_EXPORTS(i) exports
#define NS_API(n) candidate_##n
#include "../../../src/compat/commands/TKGPUCompatNamespace.H"
void candidate_clear(void){free(g);g=NULL;if(exports)Tcl_DecrRefCount(exports);exports=NULL;}
