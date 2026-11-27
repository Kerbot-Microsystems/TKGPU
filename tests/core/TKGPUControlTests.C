#include "TKGPURuntime.H"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
#define EVAL(s,v) do { CHECK(tg_eval(i,s)==TG_OK); CHECK(!strcmp(tg_result(i),v)); } while(0)
int main(void) {
    tg_interp *i=tg_create(); CHECK(i);
    EVAL("set x 8; expr {$x*2}","16");
    EVAL("set x 3; if {$x} {set y yes} else {set y no}","yes");
    EVAL("if {0} {error bad} else {set y no}","no");
    EVAL("if {0} {error bad}","");
    EVAL("catch {error failed} message","1");
    EVAL("set message","failed");
    EVAL("catch {expr {2+2}} message","0");
    EVAL("set message","4");
    EVAL("eval {set a 2; incr a}","3");
    const char *args[]={"set","payload","[error unexpected]; $missing"};
    CHECK(tg_call(i,3,args)==TG_OK);
    CHECK(!strcmp(tg_result(i),args[2]));
    const char *get[]={"set","payload"};
    CHECK(tg_call(i,2,get)==TG_OK&&!strcmp(tg_result(i),args[2]));
    const char *nested[]={"eval","set q 2; incr q"};
    CHECK(tg_call(i,2,nested)==TG_OK&&!strcmp(tg_result(i),"3"));
    const char *borrowed[]={"set","q",tg_result(i)};
    CHECK(tg_call(i,3,borrowed)==TG_OK&&!strcmp(tg_result(i),"3"));
    tg_destroy(i); puts("control and direct call checks passed"); return 0;
}
