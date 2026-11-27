/* Development-only stock Tcl 8.6 stub archive consumer, not Tcl runtime. */
#define USE_TCL_STUBS
#include <tcl.h>
#include <stdio.h>
#include <string.h>
#undef Tcl_CreateInterp
#undef Tcl_DeleteInterp
#undef Tcl_GetStringResult
#undef Tcl_ResetResult
extern Tcl_Interp *Tcl_CreateInterp(void);
extern void Tcl_DeleteInterp(Tcl_Interp *);
extern const char *Tcl_GetStringResult(Tcl_Interp *);
extern void Tcl_ResetResult(Tcl_Interp *);
int main(void) {
    Tcl_Interp *i=Tcl_CreateInterp();
    if(!i) return 1;
    const char *version=Tcl_InitStubs(i,"8.6",0);
    const char *error=Tcl_GetStringResult(i);
    int ok=!version&&error&&strstr(error,"stubs");
    Tcl_ResetResult(i);
    ok=ok&&!strcmp(Tcl_GetStringResult(i),"");
    Tcl_DeleteInterp(i);
    if(!ok) return 1;
    puts("Unsupported stock Tcl 8.6 stub bootstrap refused safely");
    return 0;
}
