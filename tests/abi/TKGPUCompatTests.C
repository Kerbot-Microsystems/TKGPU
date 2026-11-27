#include "TKGPUCompat.H"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
static void count_delete(void *p,Tcl_Interp *i) { (void)i; ++*(int *)p; }
static void delete_other(void *p,Tcl_Interp *i) { ++*(int *)p; Tcl_DeleteAssocData(i,"other"); }
int main(void) {
    Tcl_Interp *a=Tcl_CreateInterp(),*b=Tcl_CreateInterp();
    CHECK(a&&b);
    CHECK(Tcl_Eval(a,"set x 41; incr x")==0);
    CHECK(!strcmp(Tcl_GetStringResult(a),"42"));
    CHECK(Tcl_Eval(b,"info exists x")==0);
    CHECK(!strcmp(Tcl_GetStringResult(b),"0"));
    CHECK(Tcl_Eval(a,"error failed")==1);
    CHECK(!strcmp(Tcl_GetStringResult(a),"failed"));
    Tcl_ResetResult(a); CHECK(!strcmp(Tcl_GetStringResult(a),""));
    CHECK(Tcl_Eval(a,"set x")==0&&!strcmp(Tcl_GetStringResult(a),"42"));
    int first=0,second=0,third=0,other=0,hooks=0,cancelled=0;
    Tcl_SetAssocData(a,"state",count_delete,&first);
    Tcl_SetAssocData(a,"state",count_delete,&second);
    Tcl_InterpDeleteProc *proc=NULL;
    CHECK(Tcl_GetAssocData(a,"state",&proc)==&second&&proc==count_delete&&!first);
    CHECK(!Tcl_GetAssocData(b,"state",NULL));
    Tcl_DeleteAssocData(a,"state"); CHECK(second==1&&!first);
    Tcl_DeleteAssocData(a,"state"); CHECK(second==1);
    Tcl_SetAssocData(a,"other",count_delete,&other);
    Tcl_SetAssocData(a,"outer",delete_other,&third);
    Tcl_CallWhenDeleted(a,count_delete,&hooks);
    Tcl_CallWhenDeleted(a,count_delete,&hooks);
    Tcl_CallWhenDeleted(a,count_delete,&cancelled);
    Tcl_DontCallWhenDeleted(a,count_delete,&cancelled);
    Tcl_DeleteInterp(a); Tcl_DeleteInterp(b);
    CHECK(!first&&second==1&&third==1&&other==1&&hooks==2&&!cancelled);
    puts("compatibility facade checks passed"); return 0;
}
