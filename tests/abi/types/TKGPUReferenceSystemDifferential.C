#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#include <tcl.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#define TKGPU_SYSTEM_API(name) candidate_##name
#include "TKGPUReferenceSystem.H"
#define CHECK(x) do {if(!(x)){fprintf(stderr,"failure line %d\n",__LINE__);return 1;}}while(0)
int main(int argc,char **argv){
    (void)argc; Tcl_FindExecutable(argv[0]);
    Tcl_Interp *a=Tcl_CreateInterp(),*b=Tcl_CreateInterp();
    CHECK(a&&b);
    for(int e=-1024;e<4096;e++){
        char expected[256];snprintf(expected,sizeof expected,"%s",Tcl_ErrnoMsg(e));
        CHECK(!strcmp(candidate_ErrnoMsg(e),expected));
        Tcl_SetObjResult(a,Tcl_NewStringObj("keep",-1));
        Tcl_SetObjResult(b,Tcl_NewStringObj("keep",-1));
        errno=e;const char *m=candidate_PosixError(a);CHECK(errno==e&&!strcmp(m,expected));
        errno=e;m=Tcl_PosixError(b);CHECK(errno==e&&!strcmp(m,expected));
        CHECK(!strcmp(Tcl_GetStringResult(a),"keep")&&!strcmp(Tcl_GetStringResult(b),"keep"));
    }
    Tcl_DString da,db;
    CHECK(candidate_GetCwd(a,&da)&&Tcl_GetCwd(b,&db));
    CHECK(da.length==db.length&&!strcmp(da.string,db.string));
    Tcl_DStringFree(&da);Tcl_DStringFree(&db);
    CHECK(candidate_GetHostName()!=NULL);
    /* Canonical DNS hostname resolution is outside this native-only preview. */
    Tcl_DeleteInterp(a);Tcl_DeleteInterp(b);
    puts("10240 diagnostic comparisons and native cwd check passed");
    return 0;
}
