#include "TKGPUCompat.H"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(void) {
    Tcl_Interp *i=Tcl_CreateInterp(); CHECK(i);
    long n=0; int b=0;
    CHECK(Tcl_Eval(i,"set x 8")==0);
    CHECK(Tcl_ExprLong(i,"$x*2",&n)==0&&n==16);
    CHECK(Tcl_ExprBoolean(i,"0",&b)==0&&b==0);
    CHECK(Tcl_ExprBoolean(i,"-7",&b)==0&&b==1);
    CHECK(Tcl_ExprString(i,"-7/3")==0&&!strcmp(Tcl_GetStringResult(i),"-3"));
    CHECK(Tcl_ExprLong(i,"1/0",&n)!=0&&n==16);
    char *p=Tcl_AttemptAlloc(32); CHECK(p); memset(p,0,32); Tcl_Free(p);
    Tcl_Free(NULL);
    Tcl_SetObjResult(i,Tcl_NewStringObj("prior",-1));
    Tcl_Obj *saved=Tcl_GetObjResult(i);
    Tcl_Obj *formula=Tcl_NewStringObj("6*7",-1),*answer=NULL;
    CHECK(formula&&Tcl_ExprObj(i,formula,&answer)==0&&answer);
    CHECK(!strcmp(Tcl_GetString(answer),"42")&&Tcl_GetObjResult(i)==saved);
    TKGPU_ReleaseObject(formula); TKGPU_ReleaseObject(answer);
    formula=Tcl_NewStringObj("1/0",-1); answer=NULL;
    CHECK(Tcl_ExprObj(i,formula,&answer)!=0&&!answer);
    TKGPU_ReleaseObject(formula);
    Tcl_DeleteInterp(i); puts("compat expression checks passed"); return 0;
}
