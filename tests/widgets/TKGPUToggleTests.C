#include "TKGPUUi.H"
#include "TKGPUAsync.H"
#include "../../src/widgets/ttk/TKGPUToggle.H"
#include "../../src/widgets/TKGPUWidgetInternal.H"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
#define CALL(...) tg_ui_command(u,sizeof((char *[]){__VA_ARGS__})/sizeof(char *),(char *[]){__VA_ARGS__},out,sizeof out)
int main(void) {
    tg_ui *u=tg_ui_create(); CHECK(u); char out[512],script[512];
    CHECK(CALL("ttk::checkbutton",".c","-text","Check","-variable","v","-onvalue","yes","-offvalue","no","-command","incr clicks")==TG_OK);
    CHECK(CALL("ttk::radiobutton",".a","-variable","r","-value","A")==TG_OK);
    CHECK(CALL("ttk::radiobutton",".b","-variable","r","-value","B")==TG_OK);
    CHECK(CALL(".c","state")==TG_OK&&!strcmp(out,"alternate"));
    CHECK(tg_ui_toggle_sync(u,"v","yes"));
    CHECK(CALL(".c","instate","selected")==TG_OK&&!strcmp(out,"1"));
    CHECK(tg_ui_toggle_sync(u,"r","A"));
    CHECK(CALL(".a","instate","selected")==TG_OK&&!strcmp(out,"1"));
    CHECK(CALL(".b","instate","selected")==TG_OK&&!strcmp(out,"0"));
    CHECK(tg_ui_toggle_sync(u,"r","B"));
    CHECK(CALL(".a","instate","selected")==TG_OK&&!strcmp(out,"0"));
    CHECK(CALL(".b","instate","selected")==TG_OK&&!strcmp(out,"1"));
    CHECK(tg_ui_toggle_sync(u,"r",NULL));
    CHECK(CALL(".b","state")==TG_OK&&!strcmp(out,"selected alternate"));
    CHECK(!tg_ui_toggle_sync(u,"unrelated","x"));
    tg_id c=tg_ui_find(u,".c");
    CHECK(tg_ui_apply_event(u,(tg_event){TG_POINTER_DOWN,c,0,0,0},script,sizeof script));
    CHECK(tg_ui_apply_event(u,(tg_event){TG_POINTER_UP,c,0,0,0},script,sizeof script)&&u->activated==c&&!strcmp(script,"incr clicks"));
    CHECK(CALL("pack",".c")==TG_OK&&CALL("pack",".a")==TG_OK&&CALL("pack",".b")==TG_OK);
    CHECK(tg_ui_arrange(u,120,100)==TG_OK);
    tg_canvas *a=tg_canvas_create(120,100),*b=tg_canvas_create(120,100); CHECK(a&&b);
    CHECK(tg_ui_paint(u,a)==TG_OK&&tg_ui_paint_parallel(u,b,8,NULL)==TG_OK);
    CHECK(!memcmp(tg_canvas_pixels(a),tg_canvas_pixels(b),120u*100u*4u));
    CHECK(CALL("destroy",".c")==TG_OK&&u->toggle_count==2);
    tg_canvas_destroy(a); tg_canvas_destroy(b); tg_ui_destroy(u); puts("toggle variable sync, input and pixels passed"); return 0;
}
