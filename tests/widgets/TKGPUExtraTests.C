#include "TKGPUUi.H"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
#define CALL(...) tg_ui_command(u,sizeof((char *[]){__VA_ARGS__})/sizeof(char *),(char *[]){__VA_ARGS__},out,sizeof out)
int main(void) {
    tg_ui *u=tg_ui_create(); CHECK(u); char out[256];
    CHECK(CALL("ttk::scale",".s","-from","10","-to","20")==TG_OK);
    CHECK(CALL(".s","get")==TG_OK&&!strcmp(out,"0"));
    CHECK(CALL(".s","set","100")==TG_OK);
    CHECK(CALL(".s","get")==TG_OK&&!strcmp(out,"20.0"));
    CHECK(CALL(".s","configure","-from","20","-to","10")==TG_OK);
    CHECK(CALL(".s","set","9")==TG_OK);
    CHECK(CALL(".s","get")==TG_OK&&!strcmp(out,"10.0"));
    CHECK(CALL(".s","state","disabled")==TG_OK&&!strcmp(out,"!disabled"));
    CHECK(CALL(".s","instate","disabled")==TG_OK&&!strcmp(out,"1"));
    CHECK(CALL(".s","state")==TG_OK&&!strcmp(out,"disabled"));
    CHECK(CALL(".s","state","!disabled")==TG_OK&&!strcmp(out,"disabled"));
    CHECK(CALL(".s","instate","!disabled")==TG_OK&&!strcmp(out,"1"));
    CHECK(CALL("ttk::scrollbar",".b")==TG_OK);
    CHECK(CALL(".b","get")==TG_OK&&!strcmp(out,"0.0 1.0"));
    CHECK(CALL(".b","set","0.8","0.2")==TG_OK);
    CHECK(CALL(".b","get")==TG_OK&&!strcmp(out,"0.8 0.8"));
    CHECK(CALL(".b","set","0.2","0.6")==TG_OK);
    CHECK(CALL("ttk::labelframe",".f","-text","Options")==TG_OK);
    CHECK(CALL("ttk::label",".f.l","-text","Hello")==TG_OK);
    CHECK(CALL("place",".s","-x","0","-y","0","-width","100","-height","20")==TG_OK);
    CHECK(CALL("place",".b","-x","100","-y","0","-width","16","-height","100")==TG_OK);
    CHECK(CALL("place",".f","-x","0","-y","20","-width","100","-height","80")==TG_OK);
    CHECK(CALL("pack",".f.l")==TG_OK); CHECK(tg_ui_arrange(u,116,100)==TG_OK);
    const tg_widget *l=tg_ui_get(u,tg_ui_find(u,".f.l")); CHECK(l&&l->rect.y==32);
    tg_canvas *a=tg_canvas_create(116,100),*b=tg_canvas_create(116,100); CHECK(a&&b);
    CHECK(tg_ui_paint(u,a)==TG_OK&&tg_ui_paint_parallel(u,b,8,NULL)==TG_OK);
    CHECK(!memcmp(tg_canvas_pixels(a),tg_canvas_pixels(b),116u*100u*4u));
    CHECK(tg_canvas_pixels(a)[40*116+105]==0xf0f5ff);
    CHECK(tg_canvas_pixels(a)[0]==0x26374d);
    tg_canvas_destroy(a); tg_canvas_destroy(b); tg_ui_destroy(u); puts("scale, scrollbar, labelframe passed"); return 0;
}
