#include "TKGPUCompat.H"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(void) {
    Tcl_Interp *i=Tcl_CreateInterp(); CHECK(i);
    const char *path="tkgpu-eval-file-fixture.tcl";
    FILE *f=fopen(path,"wb"); CHECK(f);
    CHECK(fputs("set x 40; incr x 2\n",f)>=0&&fclose(f)==0);
    CHECK(Tcl_EvalFile(i,path)==0&&!strcmp(Tcl_GetStringResult(i),"42"));
    CHECK(remove(path)==0);
    CHECK(Tcl_EvalFile(i,path)!=0);
    CHECK(Tcl_Eval(i,"error failed")==1);
    Tcl_AddErrorInfo(i," context");
    CHECK(!strcmp(Tcl_GetStringResult(i),"failed"));
    CHECK(!strcmp(Tcl_GetVar2(i,"errorInfo",NULL,0),"failed context"));
    Tcl_Obj *previous=Tcl_GetObjResult(i);
    Tcl_AddObjErrorInfo(i," extra ignored",6);
    CHECK(!strcmp(Tcl_GetVar2(i,"errorInfo",NULL,0),"failed context extra"));
    CHECK(Tcl_GetObjResult(i)==previous);
    Tcl_SetErrorCode(i,"TKGPU","VALUE","bad input",(char *)NULL);
    CHECK(!strcmp(Tcl_GetVar(i,"errorCode",0),"TKGPU VALUE {bad input}"));
    CHECK(Tcl_GetObjResult(i)==previous);
    Tcl_Obj *code=Tcl_NewStringObj("CUSTOM CODE",-1); CHECK(code); TKGPU_RetainObject(code);
    Tcl_SetObjErrorCode(i,code);
    CHECK(Tcl_GetVar2Ex(i,"errorCode",NULL,0)==code);
    TKGPU_ReleaseObject(code);
    Tcl_Obj *command=Tcl_NewStringObj("example",-1); CHECK(command);
    Tcl_WrongNumArgs(i,1,&command,"value ?option?");
    CHECK(!strcmp(Tcl_GetStringResult(i),"wrong # args: should be \"example value ?option?\""));
    TKGPU_ReleaseObject(command);
    Tcl_DeleteInterp(i); puts("file evaluation and error context passed"); return 0;
}
