#include "TKGPUCompat.H"
#include <stdio.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(void) {
    Tcl_Interp *i=Tcl_CreateInterp(); CHECK(i); double d;
    CHECK(Tcl_ExprDouble(i,"3/2",&d)==0&&d==1);
    CHECK(Tcl_ExprDouble(i,"3/2+0.5",&d)==0&&d==1.5);
    CHECK(Tcl_SetVar2(i,"x",NULL,"2.5",0));
    CHECK(Tcl_ExprDouble(i,"$x*2",&d)==0&&d==5);
    CHECK(Tcl_ExprDouble(i,"1e+",&d)!=0);
    Tcl_DeleteInterp(i); puts("ExprDouble facade passed"); return 0;
}
