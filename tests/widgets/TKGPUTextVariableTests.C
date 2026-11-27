#include "TKGPUUi.H"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
#define E(s,v) CHECK(tg_eval(i,s)==TG_OK&&!strcmp(tg_result(i),v))
int main(void) {
    tg_interp *i=tg_create(); CHECK(i);
    E("set ::name hello","hello");
    E("ttk::entry .e -textvariable ::name",".e");
    E("label .l -textvariable name",".l");
    E(".e get","hello"); E(".l cget -text","hello");
    E(".e insert end !","");
    E("set name","hello!"); E(".l cget -text","hello!");
    E("set name world","world"); E(".e get","world");
    E("ttk::combobox .c -textvariable name -values {one two}",".c");
    E(".c current 1",""); E("set name","two"); E(".e get","two");
    E("focus .e; .e icursor end","");
    tg_ui *u=tg_interp_ui(i); tg_id id=tg_ui_find(u,".e");
    CHECK(tg_ui_post(u,(tg_event){TG_TEXT_INPUT,id,0,0,'X'})==TG_OK);
    CHECK(tg_interp_dispatch_input(i));
    E("set name","twoX"); E(".c get","twoX");
    E(".e configure -textvariable other","");
    E("set other","twoX"); E("set name new","new"); E(".e get","twoX");
    E(".e configure -textvariable {}",""); E(".e delete 0 end","");
    E("set other","twoX");
    E("message .m -text init -textvariable fresh",".m"); E("set fresh","init");
    tg_destroy(i); return 0;
}
