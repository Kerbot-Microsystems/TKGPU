#include "TKGPUCompat.H"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
typedef struct { int calls,deleted,typed; } state;
static int callback(void *p,Tcl_Interp *i,int n,Tcl_Obj *const v[]) {
    state *s=p; s->calls++;
    if(n!=2) return 1;
    long long value;
    if(Tcl_GetWideIntFromObj(NULL,v[1],&value)==0&&value==42) s->typed++;
    Tcl_SetObjResult(i,v[1]); return 0;
}
static void deleted(void *p) { ((state *)p)->deleted++; }
static int self_delete(void *p,Tcl_Interp *i,int n,Tcl_Obj *const v[]) {
    (void)n; (void)v;
    state *s=p; s->calls++;
    if(Tcl_Eval(i,"set nested 42")!=0||strcmp(Tcl_GetStringResult(i),"42")) return 1;
    if(Tcl_DeleteCommand(i,"self_delete")!=0||s->deleted) return 1;
    Tcl_SetObjResult(i,Tcl_NewStringObj("done",-1));
    return 0;
}
static int nested_objv(void *p,Tcl_Interp *i,int n,Tcl_Obj *const v[]) {
    (void)p; (void)n; (void)v;
    Tcl_Obj *args[]={Tcl_NewStringObj("set",-1),Tcl_NewStringObj("object_nested",-1),Tcl_NewIntObj(91)};
    for(unsigned k=0;k<3;k++) if(!args[k]||!TKGPU_RetainObject(args[k])) return 1;
    int rc=Tcl_EvalObjv(i,3,args,0);
    for(unsigned k=0;k<3;k++) TKGPU_ReleaseObject(args[k]);
    return rc;
}
int main(void) {
    Tcl_Interp *i=Tcl_CreateInterp(); CHECK(i);
    state a={0},b={0};
    Tcl_Command token=Tcl_CreateObjCommand(i,"echo",callback,&a,deleted); CHECK(token);
    Tcl_Obj *name=Tcl_NewStringObj("::echo",-1); CHECK(name);
    CHECK(Tcl_GetCommandFromObj(i,name)==token&&!strcmp(Tcl_GetCommandName(i,token),"echo"));
    Tcl_Obj *full=Tcl_NewStringObj("prefix:",-1); CHECK(full);
    Tcl_GetCommandFullName(i,token,full);
    CHECK(!strcmp(Tcl_GetString(full),"prefix:::echo")); TKGPU_ReleaseObject(full);
    Tcl_Interp *other=Tcl_CreateInterp(); CHECK(other);
    CHECK(!Tcl_GetCommandFromObj(other,name)&&!strcmp(Tcl_GetCommandName(other,token),""));
    CHECK(Tcl_DeleteCommandFromToken(other,token)==-1);
    Tcl_DeleteInterp(other); TKGPU_ReleaseObject(name);
    CHECK(Tcl_Eval(i,"echo {literal $x}")==0&&!strcmp(Tcl_GetStringResult(i),"literal $x"));
    Tcl_Obj *v[]={Tcl_NewStringObj("echo",-1),Tcl_NewIntObj(42)};
    CHECK(v[0]&&v[1]);
    CHECK(TKGPU_RetainObject(v[0])&&TKGPU_RetainObject(v[1]));
    CHECK(Tcl_EvalObjv(i,2,v,0)==0&&Tcl_GetObjResult(i)==v[1]);
    CHECK(a.calls==2&&a.typed==1);
    TKGPU_ReleaseObject(v[0]); TKGPU_ReleaseObject(v[1]);
    token=Tcl_CreateObjCommand(i,"echo",callback,&b,deleted); CHECK(token&&a.deleted==1);
    CHECK(Tcl_Eval(i,"echo second")==0&&b.calls==1);
    CHECK(Tcl_DeleteCommandFromToken(i,token)==0&&b.deleted==1);
    CHECK(Tcl_DeleteCommand(i,"echo")==-1);
    CHECK(Tcl_CreateObjCommand(i,"echo",callback,&b,deleted));
    state self={0};
    CHECK(Tcl_CreateObjCommand(i,"self_delete",self_delete,&self,deleted));
    CHECK(Tcl_Eval(i,"self_delete")==0&&!strcmp(Tcl_GetStringResult(i),"done"));
    CHECK(self.calls==1&&self.deleted==1);
    CHECK(Tcl_Eval(i,"set nested")==0&&!strcmp(Tcl_GetStringResult(i),"42"));
    CHECK(Tcl_Eval(i,"self_delete")==1);
    CHECK(Tcl_CreateObjCommand(i,"nested_objv",nested_objv,NULL,NULL));
    CHECK(Tcl_Eval(i,"nested_objv")==0&&!strcmp(Tcl_GetStringResult(i),"91"));
    Tcl_DeleteInterp(i); CHECK(b.deleted==2);
    puts("object command callback lifecycle passed"); return 0;
}
