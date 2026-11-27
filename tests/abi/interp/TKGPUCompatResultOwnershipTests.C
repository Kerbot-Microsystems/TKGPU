#include "../TKGPUFacadeTest.H"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
static int releases;
static void release_string(char *text) { ++releases; free(text); }
static char *owned(const char *text) {
    char *copy=malloc(strlen(text)+1);
    if(copy) strcpy(copy,text);
    return copy;
}
int main(void) {
    Tcl_Interp *source=Tcl_CreateInterp(),*target=Tcl_CreateInterp();
    CHECK(source&&target);
    Tcl_SetResult(source,owned("legacy"),release_string);
    CHECK(!releases&&!strcmp(Tcl_GetStringResult(source),"legacy"));
    Tcl_Obj *object=Tcl_GetObjResult(source);
    CHECK(object&&releases==1&&!strcmp(Tcl_GetString(object),"legacy"));
    Tcl_FreeResult(source);
    CHECK(releases==1&&!strcmp(Tcl_GetStringResult(source),"legacy"));
    Tcl_ResetResult(source); CHECK(releases==1);
    Tcl_SetResult(source,owned("free without converting"),release_string);
    Tcl_FreeResult(source); CHECK(releases==2);
    Tcl_ResetResult(source);
    char temporary[]="volatile";
    Tcl_SetResult(source,temporary,(Tcl_FreeProc *)(uintptr_t)1);
    temporary[0]='X';
    CHECK(!strcmp(Tcl_GetStringResult(source),"volatile"));
    char *large=malloc(20001); CHECK(large);
    memset(large,'x',20000); large[20000]=0;
    Tcl_SetResult(source,large,release_string);
    int length=0;
    object=Tcl_GetObjResult(source);
    CHECK(Tcl_GetStringFromObj(object,&length)&&length==20000&&releases==3);
    Tcl_TransferResult(source,0,target);
    CHECK(!strcmp(Tcl_GetStringResult(source),""));
    CHECK(Tcl_GetStringFromObj(Tcl_GetObjResult(target),&length)&&length==20000);
    const char binary[]={'a',0,'b',0,'c'};
    Tcl_SetObjResult(source,Tcl_NewStringObj(binary,sizeof binary));
    const char *bytes=Tcl_GetStringFromObj(Tcl_GetObjResult(source),&length);
    CHECK(length==sizeof binary&&!memcmp(bytes,binary,sizeof binary));
    Tcl_TransferResult(source,0,target);
    bytes=Tcl_GetStringFromObj(Tcl_GetObjResult(target),&length);
    CHECK(length==sizeof binary&&!memcmp(bytes,binary,sizeof binary));
    CHECK(Tcl_Eval(source,"error original {original details} {MY ERROR}")==1);
    CHECK(Tcl_Eval(target,"error previous")==1);
    Tcl_TransferResult(source,1,target);
    CHECK(!strcmp(Tcl_GetStringResult(source),""));
    CHECK(!strcmp(Tcl_GetStringResult(target),"original"));
    CHECK(!strcmp(Tcl_GetVar(target,"errorInfo",0),"original details"));
    CHECK(!strcmp(Tcl_GetVar(target,"errorCode",0),"MY ERROR"));
    Tcl_TransferResult(target,1,target);
    CHECK(!strcmp(Tcl_GetStringResult(target),"original"));
    Tcl_Obj *kept=Tcl_NewStringObj(binary,sizeof binary);
    CHECK(kept&&TKGPU_RetainObject(kept));
    Tcl_SetObjResult(source,kept);
    CHECK(Tcl_SetVar(source,"plain","value",0));
    CHECK(Tcl_GetObjResult(source)==kept);
    CHECK(Tcl_SetVar2Ex(source,"raw",NULL,kept,0)==kept);
    CHECK(!memcmp(Tcl_GetVar(source,"raw",0),binary,sizeof binary));
    CHECK(Tcl_UnsetVar(source,"plain",0)==0&&Tcl_GetObjResult(source)==kept);
    CHECK(Tcl_UnsetVar(source,"missing",0)==1&&Tcl_GetObjResult(source)==kept);
    CHECK(Tcl_UnsetVar(source,"raw",0)==0&&Tcl_GetObjResult(source)==kept);
    char *long_value=malloc(20001); CHECK(long_value);
    memset(long_value,'v',20000); long_value[20000]=0;
    CHECK(Tcl_SetVar(source,"long",long_value,0));
    CHECK(strlen(Tcl_GetVar(source,"long",0))==20000&&Tcl_GetObjResult(source)==kept);
    free(long_value);
    Tcl_AppendResult(source,"tail",(char *)NULL);
    bytes=Tcl_GetStringFromObj(Tcl_GetObjResult(source),&length);
    CHECK(length==9&&!memcmp(bytes,binary,5)&&!memcmp(bytes+5,"tail",4));
    CHECK(Tcl_GetStringFromObj(kept,&length)&&length==5);
    TKGPU_ReleaseObject(kept);
    int old_releases=releases;
    Tcl_SetResult(source,owned("preserve"),release_string);
    CHECK(Tcl_SetVar(source,"plain","value",0)&&releases==old_releases);
    CHECK(Tcl_UnsetVar(source,"plain",0)==0&&releases==old_releases);
    Tcl_ResetResult(source); CHECK(releases==old_releases+1);
    Tcl_SetResult(source,owned("alias"),release_string);
    const char *alias=Tcl_GetStringResult(source);
    Tcl_AppendResult(source,alias,(char *)NULL);
    CHECK(!strcmp(Tcl_GetStringResult(source),"aliasalias"));
    CHECK(Tcl_SetVar2(source,"persistent","element","value",0)!=NULL);
    CHECK(Tcl_UnsetVar2(source,"persistent","element",0)==0);
    CHECK(Tcl_GetVar(source,"persistent",0)==NULL);
    CHECK(Tcl_SetVar(source,"persistent","scalar",0)==NULL);
    CHECK(Tcl_SetVar2(source,"persistent","next","value",0)!=NULL);
    CHECK(Tcl_UnsetVar(source,"persistent",0)==0);
    CHECK(Tcl_GetVar2(source,"persistent","next",0)==NULL);
    CHECK(Tcl_SetVar(source,"persistent","scalar",0)!=NULL);
    CHECK(Tcl_UnsetVar(source,"persistent",0)==0);
    CHECK(Tcl_SetVar2(source,"empty_array","element","value",0)!=NULL);
    CHECK(Tcl_UnsetVar2(source,"empty_array","element",0)==0);
    CHECK(Tcl_UnsetVar(source,"empty_array",0)==0);
    int before_replacements=releases;
    for(int k=0;k<10000;k++) {
        Tcl_SetResult(source,owned("replace"),release_string);
        Tcl_SetResult(source,owned("replacement"),release_string);
        Tcl_ResetResult(source);
    }
    CHECK(releases==before_replacements+20000);
    Tcl_SetResult(source,owned("destroy"),release_string);
    Tcl_DeleteInterp(source); Tcl_DeleteInterp(target);
    CHECK(releases==before_replacements+20001);
    puts("Legacy/object result ownership, large/binary transfers and teardown passed");
    return 0;
}
