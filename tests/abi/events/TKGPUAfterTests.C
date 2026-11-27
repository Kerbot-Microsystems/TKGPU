#include "TKGPUCompat.H"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
static int eq(Tcl_Interp *i,const char *script,const char *expected) {
    return Tcl_Eval(i,script)==0&&!strcmp(Tcl_GetStringResult(i),expected);
}
int main(void) {
    Tcl_Interp *i=Tcl_CreateInterp(); CHECK(i);
    CHECK(eq(i,"set x {}; set t [after 0 {append x T}]; set j [after idle {append x I}]; after info $t","{append x T} timer"));
    CHECK(eq(i,"after info $j","{append x I} idle"));
    CHECK(eq(i,"update idletasks; set x","I"));
    CHECK(eq(i,"update; set x","IT"));
    CHECK(eq(i,"after info",""));
    CHECK(eq(i,"set t [after 0 {append x X}]; after cancel $t; after idle {append x Y}; after cancel {append x Y}; update; set x","IT"));
    CHECK(eq(i,"after 2; set x","IT"));
    CHECK(eq(i,"after idle {append x A; after idle {append x B}}; update idletasks; set x","ITAB"));
    CHECK(eq(i,"after 10000 {error orphan}; after idle {error orphan}; set x","ITAB"));
    Tcl_DeleteInterp(i); CHECK(!Tcl_DoOneEvent(TKGPU_DONT_WAIT));
    puts("after timer/idle, cancellation, info, nested scheduling and cleanup passed"); return 0;
}
