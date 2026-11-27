#include "TKGPUCompat.H"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(void) {
    Tcl_Interp *i=Tcl_CreateInterp(); CHECK(i);
    CHECK(Tcl_Eval(i,"set x first")==0);
    Tcl_Obj *r=Tcl_GetObjResult(i); CHECK(r&&!strcmp(Tcl_GetString(r),"first"));
    CHECK(Tcl_GetObjResult(i)==r);
    CHECK(TKGPU_RetainObject(r)==r);
    CHECK(Tcl_Eval(i,"set x second")==0);
    Tcl_Obj *next=Tcl_GetObjResult(i); CHECK(next&&!strcmp(Tcl_GetString(next),"second"));
    CHECK(!strcmp(Tcl_GetString(r),"first"));
    Tcl_SetObjResult(i,r); CHECK(!strcmp(Tcl_GetStringResult(i),"first"));
    CHECK(Tcl_GetObjResult(i)==r);
    TKGPU_ReleaseObject(r);
    Tcl_Obj *v=Tcl_GetVar2Ex(i,"x",NULL,0);
    CHECK(v&&!strcmp(Tcl_GetString(v),"second"));
    CHECK(TKGPU_RetainObject(v)==v);
    Tcl_Obj *one=Tcl_NewIntObj(1); CHECK(one&&TKGPU_RetainObject(one)==one);
    next=Tcl_SetVar2Ex(i,"x",NULL,one,0); CHECK(next==one&&!strcmp(Tcl_GetString(next),"1"));
    CHECK(Tcl_GetVar2Ex(i,"x",NULL,0)==one);
    CHECK(Tcl_SetVar2Ex(i,"array","key",one,0)==one);
    CHECK(Tcl_GetVar2Ex(i,"::array","key",0)==one);
    CHECK(Tcl_GetVar2Ex(i,"x",NULL,0)==one);
    CHECK(Tcl_SetVar2Ex(i,"x",NULL,one,0)==one);
    CHECK(Tcl_SetVar2(i,"x",NULL,"plain",0));
    CHECK(Tcl_GetVar2Ex(i,"x",NULL,0)!=one);
    CHECK(Tcl_GetVar2Ex(i,"array","key",0)==one);
    CHECK(Tcl_UnsetVar2(i,"array",NULL,0)==0);
    CHECK(!strcmp(Tcl_GetString(one),"1"));
    int b=0; CHECK(Tcl_GetBooleanFromObj(i,next,&b)==0&&b==1);
    CHECK(!strcmp(Tcl_GetString(v),"second"));
    TKGPU_ReleaseObject(v); TKGPU_ReleaseObject(one);
    Tcl_DeleteInterp(i); puts("compat result and object variable checks passed"); return 0;
}
