#include "TKGPUCompat.H"
#include "../../../src/object/TKGPUObjectInternal.H"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(void) {
    const Tcl_ObjType *type=Tcl_GetObjType("double");
    CHECK(type&&type==Tcl_GetObjType("double")&&!strcmp(type->name,"double"));
    CHECK(!Tcl_GetObjType("unimplemented"));
    Tcl_Obj *o=Tcl_NewDoubleObj(3.25); CHECK(o&&o->typePtr==type&&o->refCount==0);
    double v=0; CHECK(Tcl_GetDoubleFromObj(NULL,o,&v)==0&&v==3.25);
    TKGPU_ReleaseObject(o);
    o=Tcl_NewDoubleObj(2); CHECK(o&&!strcmp(Tcl_GetString(o),"2.0"));
    TKGPU_ReleaseObject(o);
    CHECK(Tcl_GetDouble(NULL," 1.25 ",&v)==0&&v==1.25);
    CHECK(Tcl_GetDouble(NULL,"invalid",&v)!=0&&v==1.25);
    CHECK(Tcl_GetDouble(NULL,"1e999",&v)!=0);
    o=Tcl_NewIntObj(42); CHECK(o&&o->typePtr==Tcl_GetObjType("int"));
    TKGPU_ReleaseObject(o);
    o=Tcl_NewByteArrayObj((const unsigned char *)"a",1);
    CHECK(o&&o->typePtr==Tcl_GetObjType("bytearray")); TKGPU_ReleaseObject(o);
    o=Tcl_NewListObj(0,NULL);
    CHECK(o&&o->typePtr==Tcl_GetObjType("list")); TKGPU_ReleaseObject(o);
    o=Tcl_NewStringObj("0x2a",-1); CHECK(o);
    long long integer=0; CHECK(Tcl_GetWideIntFromObj(NULL,o,&integer)==0&&integer==42);
    CHECK(o->typePtr==Tcl_GetObjType("int")&&!strcmp(Tcl_GetString(o),"0x2a"));
    CHECK(Tcl_GetWideIntFromObj(NULL,o,&integer)==0&&integer==42);
    TKGPU_ReleaseObject(o);
    o=Tcl_NewStringObj("2.50",-1); CHECK(o);
    CHECK(type->setFromAnyProc&&type->setFromAnyProc(NULL,o)==0);
    CHECK(o->typePtr==type&&Tcl_GetDoubleFromObj(NULL,o,&v)==0&&v==2.5);
    CHECK(!strcmp(Tcl_GetString(o),"2.50"));
    CHECK(Tcl_GetObjType("list")->setFromAnyProc(NULL,o)==0);
    CHECK(o->typePtr==Tcl_GetObjType("list"));
    CHECK(type->setFromAnyProc(NULL,o)==0&&o->typePtr==type);
    TKGPU_ReleaseObject(o);
    Tcl_Interp *i=Tcl_CreateInterp(); CHECK(i);
    CHECK(Tcl_Eval(i,"expr 2+3")==0);
    o=Tcl_GetObjResult(i); CHECK(o&&o->typePtr==Tcl_GetObjType("int"));
    CHECK(Tcl_Eval(i,"expr 2.5*2")==0);
    o=Tcl_GetObjResult(i); CHECK(o&&o->typePtr==type&&Tcl_GetDoubleFromObj(i,o,&v)==0&&v==5);
    CHECK(Tcl_Eval(i,"set literal 007")==0);
    o=Tcl_GetObjResult(i); CHECK(o&&!o->typePtr&&!strcmp(Tcl_GetString(o),"007"));
    Tcl_DeleteInterp(i);
    o=Tcl_NewObj(); CHECK(o);
    Tcl_SetStringObj(o,"abc",-1);
    Tcl_AppendToObj(o,Tcl_GetString(o),-1);
    CHECK(!strcmp(Tcl_GetString(o),"abcabc"));
    int units; CHECK(Tcl_GetUnicodeFromObj(o,&units)&&units==6);
    Tcl_SetStringObj(o,"42",-1);
    CHECK(Tcl_GetUnicodeFromObj(o,&units)&&units==2);
    CHECK(Tcl_GetWideIntFromObj(NULL,o,&integer)==0&&integer==42);
    Tcl_SetStringObj(o,"7",-1);
    CHECK(Tcl_GetWideIntFromObj(NULL,o,&integer)==0&&integer==7);
    Tcl_SetWideIntObj(o,1234567890123LL);
    CHECK(Tcl_GetWideIntFromObj(NULL,o,&integer)==0&&integer==1234567890123LL);
    Tcl_SetDoubleObj(o,3.5); CHECK(Tcl_GetDoubleFromObj(NULL,o,&v)==0&&v==3.5);
    Tcl_InvalidateStringRep(o); CHECK(!strcmp(Tcl_GetString(o),"3.5"));
    const unsigned char data[]={0,42,255}; Tcl_SetByteArrayObj(o,data,3);
    int len; unsigned char *raw=Tcl_GetByteArrayFromObj(o,&len);
    CHECK(raw&&len==3&&!memcmp(raw,data,3));
    Tcl_SetByteArrayObj(o,raw+1,2);
    raw=Tcl_GetByteArrayFromObj(o,&len); CHECK(raw&&len==2&&raw[0]==42&&raw[1]==255);
    TKGPU_ReleaseObject(o);
    o=Tcl_NewListObj(0,NULL); CHECK(o);
    Tcl_Obj *item=Tcl_NewStringObj("a b",-1); CHECK(item);
    CHECK(Tcl_ListObjAppendElement(NULL,o,item)==0);
    CHECK(Tcl_ListObjAppendElement(NULL,o,item)==0);
    Tcl_Obj *got=NULL; int count=0;
    CHECK(Tcl_ListObjLength(NULL,o,&count)==0&&count==2);
    CHECK(Tcl_ListObjIndex(NULL,o,1,&got)==0&&got==item);
    CHECK(!strcmp(Tcl_GetString(o),"{a b} {a b}"));
    CHECK(Tcl_ListObjAppendElement(NULL,o,o)!=0);
    Tcl_Obj *new_item=Tcl_NewStringObj("c",-1); CHECK(new_item);
    CHECK(Tcl_ListObjReplace(NULL,o,1,1,1,&new_item)==0);
    CHECK(!strcmp(Tcl_GetString(o),"{a b} c"));
    CHECK(Tcl_ListObjAppendList(NULL,o,o)==0);
    CHECK(!strcmp(Tcl_GetString(o),"{a b} c {a b} c"));
    CHECK(Tcl_ListObjIndex(NULL,o,3,&got)==0&&got==new_item);
    TKGPU_ReleaseObject(o);
    o=Tcl_NewListObj(0,NULL); CHECK(o);
    CHECK(Tcl_AppendAllObjTypes(NULL,o)==0);
    CHECK(Tcl_ListObjLength(NULL,o,&count)==0&&count>=5);
    int seen=0; for(int k=0;k<count;k++) {
        CHECK(Tcl_ListObjIndex(NULL,o,k,&got)==0&&got);
        if(!strcmp(Tcl_GetString(got),"double")) seen++;
    }
    CHECK(seen==1); TKGPU_ReleaseObject(o);
    puts("type identity and finite double checks passed"); return 0;
}
