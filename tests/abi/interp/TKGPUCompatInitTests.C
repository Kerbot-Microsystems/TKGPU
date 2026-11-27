#include "TKGPUCompat.H"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(int argc,char **argv) {
    (void)argc; Tcl_FindExecutable(argv[0]);
    Tcl_Interp *i=Tcl_CreateInterp(); CHECK(i);
    CHECK(Tcl_Init(i)==0&&Tcl_Init(i)==0);
    CHECK(!strcmp(Tcl_GetVar2(i,"tcl_version",NULL,0),"8.6"));
    CHECK(Tcl_GetVar2(i,"tkgpu_executable",NULL,0));
    CHECK(Tcl_RecordAndEval(i,"set x 1",0)==0);
    CHECK(Tcl_RecordAndEval(i,"set x 2",TKGPU_NO_EVAL)==0);
    CHECK(Tcl_Eval(i,"set x")==0&&!strcmp(Tcl_GetStringResult(i),"1"));
    CHECK(Tcl_Eval(i,"history event 0")==0&&!strcmp(Tcl_GetStringResult(i),"set x 2"));
    CHECK(Tcl_Eval(i,"history nextid")==0&&!strcmp(Tcl_GetStringResult(i),"3"));
    CHECK(Tcl_RecordAndEval(i,"error demo",0)==1);
    CHECK(Tcl_Eval(i,"history event 0")==0&&!strcmp(Tcl_GetStringResult(i),"error demo"));
    Tcl_DeleteInterp(i); puts("initialization and recorded evaluation passed"); return 0;
}
