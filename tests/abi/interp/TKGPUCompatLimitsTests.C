#include "../TKGPUFacadeTest.H"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
static int calls,deleted,active;
static void on_delete(void *data) { ++*(int *)data; }
static void extend(void *data,Tcl_Interp *i) {
    ++calls;
    Tcl_LimitRemoveHandler(i,1,extend,data);
    if(deleted) active=-1; /* Snapshot must keep this callback alive. */
    Tcl_LimitSetCommands(i,100);
    if(Tcl_LimitCheck(i)!=0) active=-2;
}
static int object_command(void *data,Tcl_Interp *i,int n,Tcl_Obj *const a[]) {
    (void)n; (void)a; if(!Tcl_InterpActive(i)) return 1; ++*(int *)data; return 0;
}
static int recurse(void *data,Tcl_Interp *i,int n,Tcl_Obj *const args[]) {
    (void)n; (void)args;
    int *left=data;
    if((*left)--<=0) return 0;
    return Tcl_Eval(i,"recurse");
}
int main(void) {
    Tcl_Interp *i=Tcl_CreateInterp();
    CHECK(i&&!Tcl_InterpActive(i));
    CHECK(Tcl_LimitGetCommands(i)==0);
    CHECK(Tcl_LimitGetGranularity(i,1)==1);
    CHECK(Tcl_LimitGetGranularity(i,2)==10);
    CHECK(!Tcl_LimitTypeEnabled(i,1)&&!Tcl_LimitTypeEnabled(i,2));
    Tcl_Time time={123,456},copy={0,0};
    Tcl_LimitSetTime(i,&time); Tcl_LimitGetTime(i,&copy);
    CHECK(copy.sec==123&&copy.usec==456);
    Tcl_LimitSetGranularity(i,2,1);
    CHECK(Tcl_LimitGetGranularity(i,2)==1);
    Tcl_LimitSetCommands(i,1); Tcl_LimitTypeSet(i,1);
    CHECK(Tcl_Eval(i,"set first 1")==0);
    CHECK(Tcl_Eval(i,"set second 2")==1);
    CHECK(strstr(Tcl_GetStringResult(i),"command count limit exceeded"));
    CHECK(Tcl_LimitExceeded(i)&&Tcl_LimitTypeExceeded(i,1));
    CHECK(!Tcl_GetVar(i,"second",0));
    Tcl_LimitSetGranularity(i,1,100);
    CHECK(Tcl_Eval(i,"set blocked_after_error 1")==1);
    CHECK(!Tcl_GetVar(i,"blocked_after_error",0));
    Tcl_LimitSetCommands(i,100);
    CHECK(!Tcl_LimitExceeded(i));
    Tcl_LimitSetGranularity(i,1,1);
    CHECK(Tcl_Eval(i,"set recovered 1")==0);

    Tcl_LimitTypeReset(i,1);
    CHECK(!Tcl_LimitExceeded(i)&&!Tcl_LimitTypeEnabled(i,1));
    CHECK(Tcl_Eval(i,"set second 2")==0);
    Tcl_LimitSetCommands(i,0);
    Tcl_LimitAddHandler(i,1,extend,&deleted,on_delete);
    Tcl_LimitTypeSet(i,1);
    CHECK(Tcl_Eval(i,"set extended 3")==0);
    CHECK(calls==1&&deleted==1&&active==0);
    Tcl_LimitTypeReset(i,1);
    Tcl_LimitTypeSet(i,2);
    CHECK(Tcl_Eval(i,"set expired 1")==1);
    CHECK(strstr(Tcl_GetStringResult(i),"time limit exceeded"));
    CHECK(!Tcl_GetVar(i,"expired",0));
    Tcl_LimitTypeReset(i,2);
    int object_calls=0;
    CHECK(Tcl_CreateObjCommand(i,"native",object_command,&object_calls,NULL));
    CHECK(Tcl_Eval(i,"native")==0&&object_calls==1);
    CHECK(!Tcl_InterpActive(i));
    Tcl_LimitSetCommands(i,0); Tcl_LimitTypeSet(i,1);
    Tcl_Obj *arg=Tcl_NewStringObj("native",-1);
    CHECK(TKGPU_RetainObject(arg));
    CHECK(Tcl_EvalObjv(i,1,&arg,0)==1&&object_calls==1);
    TKGPU_ReleaseObject(arg);
    Tcl_LimitTypeReset(i,1);
    int remaining=8;
    CHECK(Tcl_CreateObjCommand(i,"recurse",recurse,&remaining,NULL));
    int original=Tcl_SetRecursionLimit(i,0);
    CHECK(original>0&&Tcl_SetRecursionLimit(i,-1)==original);
    CHECK(Tcl_SetRecursionLimit(i,2)==original);
    CHECK(Tcl_Eval(i,"recurse")==1&&remaining>0);
    CHECK(Tcl_SetRecursionLimit(i,64)==2);
    remaining=8;
    CHECK(Tcl_Eval(i,"recurse")==0&&remaining==-1);
    CHECK(Tcl_SetRecursionLimit(i,original)==64);
    Tcl_DeleteInterp(i);
    puts("Resource limit configuration, evaluation, callback reentry and removal passed");
    return 0;
}
