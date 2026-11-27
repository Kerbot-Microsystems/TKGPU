#include "TKGPUUi.H"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
static int eq(tg_interp *i,const char *s,const char *v) { return tg_eval(i,s)==TG_OK&&!strcmp(tg_result(i),v); }
int main(void) {
    tg_interp *i=tg_create(); CHECK(i);
    CHECK(eq(i,"set v yes; set clicks 0; ttk::checkbutton .c -variable v -onvalue yes -offvalue no -command {incr clicks}; .c instate selected","1"));
    CHECK(eq(i,".c invoke; set v","no"));
    CHECK(eq(i,"set clicks","1"));
    CHECK(eq(i,"ttk::radiobutton .a -variable r -value A; ttk::radiobutton .b -variable r -value B; .a invoke; .a instate selected","1"));
    CHECK(eq(i,".b invoke; .a instate selected","0"));
    CHECK(eq(i,"unset r; .b state","selected alternate"));
    tg_ui *u=tg_interp_ui(i); tg_id c=tg_ui_find(u,".c");
    CHECK(tg_ui_post(u,(tg_event){TG_POINTER_DOWN,c,0,0,0})==TG_OK&&tg_interp_dispatch_input(i));
    CHECK(tg_ui_post(u,(tg_event){TG_POINTER_UP,c,0,0,0})==TG_OK&&tg_interp_dispatch_input(i));
    CHECK(eq(i,"set v","yes"));
    CHECK(eq(i,".c configure -state disabled; .c invoke; set clicks","2"));
    tg_destroy(i); puts("toggle interpreter integration passed"); return 0;
}
