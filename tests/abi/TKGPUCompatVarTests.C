#include "TKGPUCompat.H"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
static int remove_on_write(void *data,Tcl_Interp *i,int n,Tcl_Obj *const v[]) {
    (void)data; (void)n; (void)v;
    return Tcl_UnsetVar2(i,"victim",NULL,0);
}
int main(void) {
    Tcl_Interp *i=Tcl_CreateInterp(); CHECK(i);
    CHECK(Tcl_Eval(i,"set sentinel retained")==0);
    CHECK(!Tcl_GetVar2(i,"missing",NULL,0));
    CHECK(!strcmp(Tcl_GetStringResult(i),"retained"));
    const char *v=Tcl_SetVar2(i,"x",NULL,"42",TKGPU_GLOBAL_ONLY);
    CHECK(v&&!strcmp(v,"42"));
    CHECK(Tcl_SetVar2(i,"x",NULL,v,0));
    CHECK(!strcmp(Tcl_GetVar2(i,"x",NULL,0),"42"));
    CHECK(!strcmp(Tcl_GetStringResult(i),"retained"));
    CHECK(!Tcl_GetVar2(i,"x","index",TKGPU_LEAVE_ERR_MSG));
    CHECK(Tcl_UnsetVar2(i,"x",NULL,0)==0);
    CHECK(Tcl_SetVar2(i,"::global",NULL,"hello",TKGPU_GLOBAL_ONLY));
    CHECK(!strcmp(Tcl_GetVar2(i,"global",NULL,0),"hello"));
    CHECK(Tcl_Eval(i,"set ::global")==0&&!strcmp(Tcl_GetStringResult(i),"hello"));
    CHECK(Tcl_Eval(i,"set alias $::global")==0&&!strcmp(Tcl_GetStringResult(i),"hello"));
    CHECK(Tcl_SetVar2(i,"settings","theme","dark",0));
    CHECK(!strcmp(Tcl_GetVar2(i,"::settings","theme",0),"dark"));
    CHECK(Tcl_Eval(i,"set value $settings(theme)")==0&&!strcmp(Tcl_GetStringResult(i),"dark"));
    CHECK(!Tcl_SetVar2(i,"settings",NULL,"scalar",0));
    CHECK(Tcl_SetVar2(i,"settings","mode","gpu",0));
    CHECK(Tcl_UnsetVar2(i,"settings",NULL,0)==0);
    CHECK(!Tcl_GetVar2(i,"settings","theme",0));
    CHECK(Tcl_SetVar2(i,"settings",NULL,"now scalar",0));
    CHECK(!Tcl_SetVar2(i,"settings","theme","dark",0));
    CHECK(Tcl_CreateObjCommand(i,"remove_on_write",remove_on_write,NULL,NULL));
    CHECK(Tcl_Eval(i,"trace add variable victim write remove_on_write")==0);
    v=Tcl_SetVar2(i,"victim",NULL,"gone",TKGPU_LEAVE_ERR_MSG); CHECK(v&&!*v);
    CHECK(!Tcl_GetVar2(i,"victim",NULL,0));
    CHECK(Tcl_Eval(i,"trace add variable victim write remove_on_write")==0);
    Tcl_Obj *obj=Tcl_NewIntObj(7); CHECK(obj&&TKGPU_RetainObject(obj));
    Tcl_Obj *removed=Tcl_SetVar2Ex(i,"victim",NULL,obj,TKGPU_LEAVE_ERR_MSG);
    CHECK(removed&&!strcmp(Tcl_GetString(removed),""));
    CHECK(!Tcl_GetVar2Ex(i,"victim",NULL,0));
    CHECK(!strcmp(Tcl_GetString(obj),"7")); TKGPU_ReleaseObject(obj);
    CHECK(Tcl_Init(i)==0);
    CHECK(!strcmp(Tcl_GetVar2(i,"tcl_platform","threaded",0),"1"));
    CHECK(!Tcl_GetVar2(i,"x",NULL,0));
    Tcl_DeleteInterp(i); puts("compat variable checks passed"); return 0;
}
