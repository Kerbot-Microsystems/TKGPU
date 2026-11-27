#include "TKGPUUi.H"
#include "TKGPUAsync.H"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
#define CALL(...) tg_ui_command(u,sizeof((char *[]){__VA_ARGS__})/sizeof(char *),(char *[]){__VA_ARGS__},out,sizeof out)
int main(void) {
    tg_ui *u=tg_ui_create(); CHECK(u); char out[256];
    CHECK(CALL("ttk::progressbar",".p")==TG_OK);
    CHECK(CALL(".p","cget","-value")==TG_OK&&!strcmp(out,"0.0"));
    CHECK(CALL(".p","cget","-maximum")==TG_OK&&!strcmp(out,"100"));
    CHECK(CALL(".p","step","110")==TG_OK);
    CHECK(CALL(".p","cget","-value")==TG_OK&&!strcmp(out,"10.0"));
    CHECK(CALL(".p","configure","-value","50","-maximum","100")==TG_OK);
    CHECK(CALL(".p","configure","-value","70","-maximum","0")==TG_ERROR);
    CHECK(CALL(".p","cget","-value")==TG_OK&&!strcmp(out,"50"));
    CHECK(CALL("ttk::style","configure","TProgressbar","-foreground","#ff0000","-background","#000000")==TG_OK);
    CHECK(CALL("ttk::separator",".s","-orient","vertical")==TG_OK);
    CHECK(CALL(".s","cget","-orient")==TG_OK&&!strcmp(out,"vertical"));
    CHECK(CALL("place",".p","-x","0","-y","0","-width","100","-height","10")==TG_OK);
    CHECK(CALL("place",".s","-x","100","-y","0","-width","4","-height","40")==TG_OK);
    CHECK(tg_ui_arrange(u,110,40)==TG_OK);
    tg_canvas *a=tg_canvas_create(110,40),*b=tg_canvas_create(110,40); CHECK(a&&b);
    CHECK(tg_ui_paint(u,a)==TG_OK&&tg_ui_paint_parallel(u,b,8,NULL)==TG_OK);
    CHECK(!memcmp(tg_canvas_pixels(a),tg_canvas_pixels(b),110u*40u*4u));
    CHECK(tg_canvas_pixels(a)[49]==0xff0000&&tg_canvas_pixels(a)[50]==0);
    CHECK(tg_canvas_pixels(a)[102]==0xf0f5ff);
    CHECK(CALL(".p","configure","-mode","indeterminate","-orient","vertical")==TG_OK);
    CHECK(CALL(".p","cget","-mode")==TG_OK&&!strcmp(out,"indeterminate"));
    CHECK(tg_ui_paint_parallel(u,b,8,NULL)==TG_OK);
    tg_canvas_destroy(a); tg_canvas_destroy(b); tg_ui_destroy(u); puts("Ttk indicators passed"); return 0;
}
