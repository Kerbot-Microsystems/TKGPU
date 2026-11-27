#include "TKGPUCompat.H"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
const Tcl_ObjType *tkgpu_test_external_register(void);
int tkgpu_test_external_freed(void);
void tkgpu_test_external_change(Tcl_Obj *,char);
int main(void) {
    const Tcl_ObjType *type=tkgpu_test_external_register();
    CHECK(type&&Tcl_GetObjType(type->name)==type);
    CHECK(tkgpu_test_external_register()==type);
    Tcl_Obj *o=Tcl_NewStringObj("42",-1); CHECK(o);
    CHECK(Tcl_ConvertToType(NULL,o,type)==0);
    Tcl_Obj *copy=Tcl_DuplicateObj(o); CHECK(copy&&!strcmp(Tcl_GetString(copy),"42"));
    CHECK(((TKGPUAbiObjectPrefix *)(void *)copy)->typePtr==type);
    tkgpu_test_external_change(copy,'7');
    CHECK(!strcmp(Tcl_GetString(copy),"47")&&!strcmp(Tcl_GetString(o),"42"));
    int value=0; CHECK(Tcl_GetIntFromObj(NULL,o,&value)==0&&value==42);
    CHECK(tkgpu_test_external_freed()==1);
    TKGPU_ReleaseObject(o); CHECK(tkgpu_test_external_freed()==1);
    TKGPU_ReleaseObject(copy); CHECK(tkgpu_test_external_freed()==2);
    o=Tcl_NewStringObj("wrong",-1); CHECK(o);
    CHECK(Tcl_ConvertToType(NULL,o,type)!=0);
    TKGPU_ReleaseObject(o); CHECK(tkgpu_test_external_freed()==2);
    puts("external shared module type lifecycle passed"); return 0;
}
