#include "TKGPURuntime.H"
#include <stdio.h>
#include <string.h>
#include <limits.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); return 1; } } while(0)
#define EVAL(s,v) do { CHECK(tg_eval(i,s)==TG_OK); CHECK(!strcmp(tg_result(i),v)); } while(0)
int main(void) {
    tg_interp *i=tg_create(); CHECK(i);
    EVAL("set x 40; incr x 2","42");
    EVAL("set y \"value $x\"","value 42");
    EVAL("set y {literal $x {inner}}","literal $x {inner}");
    EVAL("set z $" "{x}","42");
    EVAL("# comment\nincr x -2","40");
    EVAL("set z {a;b\nc}","a;b\nc");
    CHECK(tg_eval(i,"set x [foo]")==TG_ERROR);
    CHECK(tg_eval(i,"set x {bad")==TG_ERROR);
    CHECK(tg_eval(i,"unset no_such_var")==TG_ERROR);
    EVAL("unset x","");
    CHECK(tg_eval(i,"set x")==TG_ERROR);
    EVAL("incr x","1");
    CHECK(tg_eval(i,"error failure; set x 10")==TG_ERROR);
    EVAL("set x","1");
    EVAL("append message hello { world}","hello world");
    EVAL("append message !","hello world!");
    EVAL("append message","hello world!");
    EVAL("set x 10; set y [incr x 2]","12");
    EVAL("set x [expr {2+3*4}]","14");
    EVAL("set z [expr [expr {1+2}] * 4]","12");
    EVAL("set z \"value [set x]\"","value 14");
    EVAL("set z {[set x]}","[set x]");
    EVAL("set z [set a 1; incr a]","2");
    CHECK(tg_eval(i,"set z [error bad]")==TG_ERROR);
    CHECK(!strcmp(tg_result(i),"bad"));
    CHECK(tg_eval(i,"set z [set a")==TG_ERROR);
    EVAL("expr {-7/3}","-3");
    CHECK(tg_eval(i,"expr {1/0}")==TG_ERROR);
    EVAL("set script {set v 42}","set v 42");
    CHECK(tg_eval(i,tg_result(i))==TG_OK&&!strcmp(tg_result(i),"42"));
    EVAL("info exists v","1");
    EVAL("info exists missing","0");
    EVAL("set escaped \\x41\\u0416\\377","A\xd0\x96\xc3\xbf");
    EVAL("set escaped {\\u0416}","\\u0416");
    EVAL("set escaped \"a\\\n b\"","a b");
    EVAL("set escaped {a\\\n b}","a b");
    EVAL("list a\\\nb","a b");
    EVAL("list {a}\\\n b","a b");
    EVAL("list \\\n b","b");
    EVAL("# hidden\\\nerror ignored\nset escaped ok","ok");
    CHECK(tg_eval(i,"# comment\\\\\nerror visible")==TG_ERROR);
    CHECK(!strcmp(tg_result(i),"visible"));
    EVAL("# comment\\\\\\\nerror ignored\nset escaped ok","ok");
    EVAL("set escaped x\\","x\\");
    CHECK(tg_eval(i,"set escaped \\0")==TG_ERROR);
    CHECK(tg_eval(i,"set escaped \\ud800")==TG_ERROR);
    for(int k=0;k<1024;k++) {
        char key[32]; snprintf(key,sizeof key,"variable_%d",k);
        const char *args[]={"set",key,"value"};
        CHECK(tg_call(i,3,args)==TG_OK&&!strcmp(tg_result(i),"value"));
    }
    EVAL("set variable_1023","value");
    for(int j=0;j<128;j++) EVAL("set ::variable_1023 updated","updated");
    EVAL("set variable_1023 value","value");
    EVAL("set conflict scalar","scalar");
    CHECK(tg_eval(i,"set conflict(member) value")==TG_ERROR);
    EVAL("set conflict again","again");
    EVAL("unset conflict","");
    EVAL("set conflict(member) value","value");
    CHECK(tg_eval(i,"set conflict scalar")==TG_ERROR);
    EVAL("set ::conflict(member) changed","changed");
    EVAL("unset conflict","");
    EVAL("set conflict scalar","scalar");
    EVAL("unset variable_1000","");
    EVAL("set variable_1023","value");
    tg_destroy(i);
    tg_canvas *c=tg_canvas_create(4,4); tg_rect r; CHECK(c);
    CHECK(tg_canvas_damage(c,&r)&&r.w==4&&r.h==4);
    tg_canvas_fill(c,(tg_rect){-2,-2,4,4},0xff123456);
    CHECK(tg_canvas_pixels(c)[0]==0x123456);
    CHECK(tg_canvas_pixels(c)[5]==0x123456);
    CHECK(tg_canvas_pixels(c)[10]==0);
    CHECK(tg_canvas_damage(c,&r)&&r.x==0&&r.y==0&&r.w==2&&r.h==2);
    CHECK(!tg_canvas_damage(c,&r));
    tg_canvas_fill(c,(tg_rect){INT_MAX,INT_MAX,INT_MAX,INT_MAX},0);
    CHECK(!tg_canvas_damage(c,&r));
    CHECK(tg_canvas_resize(c,0,4)==TG_ERROR&&tg_canvas_width(c)==4);
    CHECK(tg_canvas_resize(c,8,2)==TG_OK&&tg_canvas_height(c)==2);
    tg_canvas_destroy(c);
    puts("core checks passed"); return 0;
}
