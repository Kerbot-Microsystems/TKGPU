/* Compiled with the real Tcl 8.6 development header. Linked exclusively
   against TKGPUCompat. This does not initialize Tcl or Tk. */
#include <tcl.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(void) {
    Tcl_Obj *o=Tcl_NewStringObj("hello",-1);
    CHECK(o&&o->refCount==0&&o->length==5);
    Tcl_IncrRefCount(o);
    CHECK(o->refCount==1&&!strcmp(Tcl_GetString(o),"hello"));
    Tcl_IncrRefCount(o); CHECK(o->refCount==2);
    Tcl_DecrRefCount(o); CHECK(o->refCount==1);
    Tcl_DecrRefCount(o);
    Tcl_Obj *wide=Tcl_NewWideIntObj((Tcl_WideInt)1234567890123LL);
    CHECK(wide&&wide->refCount==0);
    Tcl_WideInt value=0;
    CHECK(Tcl_GetWideIntFromObj(NULL,wide,&value)==TCL_OK&&value==1234567890123LL);
    Tcl_IncrRefCount(wide); Tcl_DecrRefCount(wide);
    Tcl_Interp *i=Tcl_CreateInterp(); CHECK(i);
    o=Tcl_NewStringObj("result",-1); CHECK(o&&o->refCount==0);
    Tcl_SetObjResult(i,o);
    CHECK(o->refCount==1&&Tcl_GetObjResult(i)==o);
    Tcl_SetObjResult(i,o); CHECK(o->refCount==1);
    Tcl_IncrRefCount(o);
    CHECK(Tcl_SetVar2(i,"keep",NULL,"value",0));
    CHECK(Tcl_GetObjResult(i)==o&&o->refCount==2);
    CHECK(Tcl_Eval(i,"set same result")==TCL_OK);
    CHECK(Tcl_GetObjResult(i)!=o&&!strcmp(Tcl_GetString(o),"result"));
    Tcl_DecrRefCount(o);
    Tcl_Obj *child=Tcl_NewIntObj(42); CHECK(child&&child->refCount==0);
    Tcl_IncrRefCount(child);
    Tcl_Obj *items[]={child,child};
    Tcl_Obj *list=Tcl_NewListObj(2,items); CHECK(list&&child->refCount==3);
    Tcl_IncrRefCount(list);
    Tcl_Obj *item=NULL; CHECK(Tcl_ListObjIndex(i,list,1,&item)==TCL_OK&&item==child);
    Tcl_DecrRefCount(list); CHECK(child->refCount==1);
    CHECK(Tcl_SetVar2Ex(i,"object",NULL,child,0)==child&&child->refCount==2);
    CHECK(Tcl_GetVar2Ex(i,"object",NULL,0)==child);
    Tcl_DecrRefCount(child);
    Tcl_DeleteInterp(i);
    puts("reference-header consumer passed"); return 0;
}
