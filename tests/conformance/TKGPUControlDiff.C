#include "TKGPUUi.H"
#include <stdio.h>
#include <string.h>
#define PROBE(...) do { char *args[]={__VA_ARGS__}; int rc=tg_ui_command(u,sizeof args/sizeof args[0],args,out,sizeof out); printf("%d:%s\n",rc,rc?"":out); } while(0)
int main(void) {
    tg_ui *u=tg_ui_create(); if(!u) return 1; char out[512];
    PROBE("button",".b","-text","Apply","-command","incr clicks");
    PROBE(".b","cget","-command");
    PROBE(".b","cget","-state");
    PROBE(".b","configure","-state","disabled");
    PROBE(".b","cget","-state");
    PROBE("entry",".e");
    PROBE(".e","insert","0","A\xc3\xa9Z");
    PROBE(".e","index","end");
    PROBE(".e","index","-1");
    PROBE(".e","index","999");
    PROBE(".e","delete","1","2");
    PROBE(".e","get");
    PROBE(".e","icursor","1");
    PROBE(".e","index","insert");
    PROBE(".e","configure","-state","readonly");
    PROBE(".e","insert","end","Q");
    PROBE(".e","get");
    PROBE(".e","configure","-state","normal");
    PROBE(".e","delete","0");
    PROBE(".e","get");
    PROBE("ttk::frame",".f");
    PROBE("ttk::button",".f.b","-text","OK","-command","set x 1");
    PROBE(".f.b","cget","-text");
    PROBE(".f.b","cget","-command");
    PROBE("ttk::label",".l","-text","Label");
    PROBE(".l","cget","-text");
    PROBE("ttk::entry",".t");
    PROBE(".t","insert","end","hello");
    PROBE(".t","get");
    PROBE("destroy",".f");
    PROBE("winfo","exists",".f.b");
    tg_ui_destroy(u); return 0;
}
