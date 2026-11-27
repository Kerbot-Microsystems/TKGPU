#include "TKGPUCompat.H"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(void) {
    Tcl_Interp *i=Tcl_CreateInterp(); CHECK(i);
    Tcl_Obj *args[]={Tcl_NewStringObj("two words",-1),Tcl_NewIntObj(42),Tcl_NewStringObj("",0)};
    CHECK(args[0]&&args[1]&&args[2]);
    for(int k=0;k<3;k++) CHECK(TKGPU_RetainObject(args[k])==args[k]);
    Tcl_Obj *list=Tcl_NewListObj(3,args); CHECK(list);
    for(int k=0;k<3;k++) TKGPU_ReleaseObject(args[k]);
    int n=-1; Tcl_Obj **v=NULL,*item=NULL;
    CHECK(Tcl_ListObjGetElements(i,list,&n,&v)==0&&n==3&&v);
    CHECK(v[0]==args[0]&&v[1]==args[1]&&v[2]==args[2]);
    CHECK(!strcmp(Tcl_GetString(v[0]),"two words"));
    CHECK(Tcl_ListObjIndex(i,list,1,&item)==0&&item==v[1]);
    CHECK(TKGPU_RetainObject(item)==item);
    CHECK(Tcl_ListObjLength(i,list,&n)==0&&n==3);
    Tcl_Obj *missing=item;
    CHECK(Tcl_ListObjIndex(i,list,-1,&missing)==0&&!missing);
    TKGPU_ReleaseObject(list);
    CHECK(!strcmp(Tcl_GetString(item),"42")); TKGPU_ReleaseObject(item);
    Tcl_Obj *same=Tcl_NewStringObj("repeat",-1); CHECK(same&&TKGPU_RetainObject(same));
    Tcl_Obj *repeated[]={same,same};
    list=Tcl_NewListObj(2,repeated); CHECK(list);
    CHECK(Tcl_ListObjGetElements(i,list,&n,&v)==0&&n==2&&v[0]==same&&v[1]==same);
    TKGPU_ReleaseObject(list);
    CHECK(!strcmp(Tcl_GetString(same),"repeat")); TKGPU_ReleaseObject(same);
    list=Tcl_NewStringObj("{invalid",-1); CHECK(list);
    n=-1; CHECK(Tcl_ListObjLength(i,list,&n)!=0&&n==-1);
    TKGPU_ReleaseObject(list);
    list=Tcl_NewListObj(0,NULL); CHECK(list);
    CHECK(Tcl_ListObjLength(i,list,&n)==0&&n==0); TKGPU_ReleaseObject(list);
    Tcl_DeleteInterp(i); puts("compat list object checks passed"); return 0;
}
