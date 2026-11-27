#include "../TKGPUFacadeTest.H"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(int argc,char **argv) {
    CHECK(argc==2);
    Tcl_Interp *i=Tcl_CreateInterp(); CHECK(i);
    Tcl_Obj *path=Tcl_NewStringObj(argv[1],-1); CHECK(path&&TKGPU_RetainObject(path));
    const char *symbols[]={"Fixture_Init","Fixture_SafeInit","Fixture_Answer",NULL};
    void *functions[3]={NULL,NULL,NULL};
    Tcl_LoadHandle first=NULL,second=NULL;
    CHECK(Tcl_LoadFile(i,path,symbols,0,functions,&first)==0&&first);
    CHECK(functions[0]&&functions[1]&&functions[2]);
    int (*answer)(void)=NULL;
    CHECK(sizeof answer==sizeof functions[2]); memcpy(&answer,&functions[2],sizeof answer);
    CHECK(answer()==42);
    CHECK(Tcl_FindSymbol(i,first,"Fixture_Answer")==functions[2]);
    CHECK(!Tcl_FindSymbol(i,first,"Missing"));
    CHECK(strstr(Tcl_GetStringResult(i),"Missing"));
    CHECK(Tcl_LoadFile(i,path,NULL,TKGPU_LOAD_GLOBAL|TKGPU_LOAD_LAZY,NULL,&second)==0);
    CHECK(second&&second!=first);
    CHECK(Tcl_FSUnloadFile(i,first)==0);
    CHECK(Tcl_FindSymbol(i,second,"Fixture_Answer"));
    CHECK(Tcl_FSUnloadFile(i,second)==0);
    const char *missing[]={"Fixture_Init","Missing",NULL};
    CHECK(Tcl_LoadFile(i,path,missing,0,functions,&first)==1);
    CHECK(!first&&!functions[0]&&!functions[1]);
    Tcl_PackageInitProc *init=NULL,*safe=NULL;
    Tcl_FSUnloadFileProc *unload=NULL;
    CHECK(Tcl_FSLoadFile(i,path,"Fixture_Init","Fixture_SafeInit",&init,&safe,&first,&unload)==0);
    CHECK(first&&init&&safe&&init(i)==0&&safe(i)==0);
    CHECK(Tcl_FSUnloadFile(i,first)==0);
    CHECK(Tcl_FSUnloadFile(i,(Tcl_LoadHandle)(uintptr_t)1)==1);
    TKGPU_ReleaseObject(path); Tcl_DeleteInterp(i);
    puts("Native extension loading, symbol lookup, failure cleanup and independent handles passed");
    return 0;
}
