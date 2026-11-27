#include "TKGPUCompat.H"
#include <stdio.h>
#include <string.h>
#include <limits.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(void) {
    Tcl_Interp *i=Tcl_CreateInterp(); CHECK(i);
    Tcl_Obj *a[]={Tcl_NewStringObj("set",-1),Tcl_NewStringObj("x",-1),Tcl_NewWideIntObj(LLONG_MIN)};
    CHECK(a[0]&&a[1]&&a[2]);
    CHECK(Tcl_EvalObjv(i,3,a,0)==0);
    CHECK(Tcl_EvalObjv(i,3,a,TKGPU_EVAL_DIRECT|TKGPU_EVAL_GLOBAL)==0);
    Tcl_Obj *result=TKGPU_CopyResultObject(i); CHECK(result);
    long long n=0; CHECK(Tcl_GetWideIntFromObj(i,result,&n)==0&&n==LLONG_MIN);
    int small=42; CHECK(Tcl_GetIntFromObj(i,result,&small)!=0&&small==42);
    CHECK(TKGPU_SetResultObject(i,result)==0);
    CHECK(TKGPU_RetainObject(result)==result);
    TKGPU_ReleaseObject(result); TKGPU_ReleaseObject(result);
    for(int k=0;k<3;k++) TKGPU_ReleaseObject(a[k]);
    Tcl_Obj *bytes=Tcl_NewStringObj("a\0b",3); CHECK(bytes);
    int length=-1; CHECK(Tcl_GetStringFromObj(bytes,&length)&&length==3);
    CHECK(!memcmp(Tcl_GetString(bytes),"a\0b",3));
    CHECK(TKGPU_SetResultObject(i,bytes)!=0);
    CHECK(Tcl_AttemptSetObjLength(bytes,5));
    CHECK(Tcl_GetStringFromObj(bytes,&length)&&length==5);
    memcpy(Tcl_GetString(bytes),"hello",5);
    CHECK(!strcmp(Tcl_GetString(bytes),"hello"));
    CHECK(!Tcl_AttemptSetObjLength(bytes,-1));
    Tcl_SetObjLength(bytes,3); CHECK(!strcmp(Tcl_GetString(bytes),"hel"));
    TKGPU_ReleaseObject(bytes);
    CHECK(!Tcl_NewStringObj(NULL,-1));
    Tcl_UniChar units[]={0x0416,0,0xd83d,0xde80,0xd800};
    Tcl_Obj *unicode=Tcl_NewUnicodeObj(units,5); CHECK(unicode);
    Tcl_UniChar *decoded=Tcl_GetUnicodeFromObj(unicode,&length);
    CHECK(decoded&&length==5&&!memcmp(decoded,units,sizeof units));
    CHECK(Tcl_GetUnicodeFromObj(unicode,NULL)==decoded);
    TKGPU_ReleaseObject(unicode);
    unicode=Tcl_NewStringObj("\xf0\x9f\x9a\x80",4); CHECK(unicode);
    decoded=Tcl_GetUnicodeFromObj(unicode,&length);
    CHECK(decoded&&length==2&&decoded[0]==0xd83d&&decoded[1]==0xde80);
    TKGPU_ReleaseObject(unicode);
    unicode=Tcl_NewUnicodeObj(NULL,0); CHECK(unicode);
    CHECK(Tcl_GetUnicodeFromObj(unicode,&length)&&length==0);
    TKGPU_ReleaseObject(unicode);
    CHECK(!Tcl_NewUnicodeObj(NULL,-1));
    Tcl_DString ds; Tcl_DStringInit(&ds);
    char block[256]; memset(block,'x',sizeof block);
    Tcl_DStringAppend(&ds,block,sizeof block);
    Tcl_DStringAppend(&ds,Tcl_DStringValue(&ds),Tcl_DStringLength(&ds));
    CHECK(Tcl_DStringLength(&ds)==512&&!memcmp(ds.string,ds.string+256,256));
    Tcl_DStringSetLength(&ds,2); CHECK(!strcmp(ds.string,"xx"));
    Tcl_DStringSetLength(&ds,-1); CHECK(!ds.length&&!ds.string[0]);
    Tcl_DStringFree(&ds); CHECK(ds.string==ds.staticSpace&&ds.spaceAvl==200);
    Tcl_DeleteInterp(i); puts("compat object checks passed"); return 0;
}
