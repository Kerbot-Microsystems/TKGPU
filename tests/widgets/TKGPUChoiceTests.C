#include "TKGPUUi.H"
#include "TKGPUAsync.H"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
#define CALL(...) tg_ui_command(u,sizeof((char *[]){__VA_ARGS__})/sizeof(char *),(char *[]){__VA_ARGS__},out,sizeof out)
int main(void) {
    tg_ui *u=tg_ui_create(); CHECK(u); char out[1024],script[512];
    CHECK(CALL("ttk::combobox",".c","-values","a {b c} d","-state","readonly")==TG_OK);
    CHECK(CALL(".c","current")==TG_OK&&!strcmp(out,"-1"));
    CHECK(CALL(".c","current","1")==TG_OK);
    CHECK(CALL(".c","get")==TG_OK&&!strcmp(out,"b c"));
    CHECK(CALL(".c","current","99")==TG_ERROR);
    CHECK(CALL(".c","configure","-values","{bad")==TG_ERROR);
    CHECK(CALL(".c","cget","-values")==TG_OK&&!strcmp(out,"a {b c} d"));
    CHECK(CALL(".c","insert","end","x")==TG_OK);
    CHECK(CALL(".c","get")==TG_OK&&!strcmp(out,"b c"));
    CHECK(CALL("focus",".c")==TG_OK);
    CHECK(tg_ui_apply_event(u,(tg_event){TG_KEY_DOWN,0,0,0,TG_KEY_ARROW_UP},script,sizeof script));
    CHECK(CALL(".c","get")==TG_OK&&!strcmp(out,"d"));
    CHECK(CALL(".c","configure","-state","disabled")==TG_OK);
    CHECK(CALL(".c","set","custom")==TG_OK);
    CHECK(CALL(".c","current")==TG_OK&&!strcmp(out,"-1"));
    CHECK(!tg_ui_apply_event(u,(tg_event){TG_KEY_DOWN,0,0,0,TG_KEY_ARROW_DOWN},script,sizeof script));
    CHECK(CALL("ttk::spinbox",".s","-from","0","-to","2","-increment","0.5","-wrap","1")==TG_OK);
    CHECK(CALL(".s","set","2")==TG_OK); CHECK(CALL("focus",".s")==TG_OK);
    CHECK(tg_ui_apply_event(u,(tg_event){TG_KEY_DOWN,0,0,0,TG_KEY_ARROW_UP},script,sizeof script));
    CHECK(CALL(".s","get")==TG_OK&&!strcmp(out,"0"));
    CHECK(tg_ui_apply_event(u,(tg_event){TG_KEY_DOWN,0,0,0,TG_KEY_ARROW_UP},script,sizeof script));
    CHECK(CALL(".s","get")==TG_OK&&!strcmp(out,"0.5"));
    CHECK(CALL(".s","current")==TG_ERROR);
    CHECK(CALL(".s","configure","-values","red green blue")==TG_OK);
    CHECK(CALL(".s","set","blue")==TG_OK);
    CHECK(tg_ui_apply_event(u,(tg_event){TG_KEY_DOWN,0,0,0,TG_KEY_ARROW_UP},script,sizeof script));
    CHECK(CALL(".s","get")==TG_OK&&!strcmp(out,"red"));
    CHECK(CALL("place",".c","-x","0","-y","0","-width","100","-height","25")==TG_OK);
    CHECK(CALL("place",".s","-x","0","-y","25","-width","100","-height","25")==TG_OK);
    CHECK(tg_ui_arrange(u,100,50)==TG_OK);
    tg_canvas *a=tg_canvas_create(100,50),*b=tg_canvas_create(100,50); CHECK(a&&b);
    CHECK(tg_ui_paint(u,a)==TG_OK&&tg_ui_paint_parallel(u,b,8,NULL)==TG_OK);
    CHECK(!memcmp(tg_canvas_pixels(a),tg_canvas_pixels(b),100u*50u*4u));
    CHECK(CALL(".c","configure","-state","readonly","-height","2")==TG_OK);
    CHECK(CALL(".c","cget","-height")==TG_OK&&!strcmp(out,"2"));
    CHECK(CALL(".c","current","0")==TG_OK);
    CHECK(tg_ui_arrange(u,180,150)==TG_OK);
    tg_id combo=tg_ui_find(u,".c");
    CHECK(tg_ui_apply_event(u,(tg_event){TG_POINTER_DOWN,combo,95,10,0},script,sizeof script));
    CHECK(tg_ui_hit(u,10,30)==combo);
    CHECK(tg_ui_apply_event(u,(tg_event){TG_KEY_DOWN,combo,0,0,TG_KEY_END},script,sizeof script));
    CHECK(tg_ui_apply_event(u,(tg_event){TG_KEY_DOWN,combo,0,0,TG_KEY_RETURN},script,sizeof script));
    CHECK(CALL(".c","get")==TG_OK&&!strcmp(out,"d"));
    CHECK(tg_ui_hit(u,10,30)!=combo);
    CHECK(CALL(".c","configure","-values","a b c d e f","-height","2")==TG_OK);
    CHECK(CALL(".c","current","0")==TG_OK);
    CHECK(tg_ui_apply_event(u,(tg_event){TG_POINTER_DOWN,combo,95,10,0},script,sizeof script));
    CHECK(tg_ui_apply_event(u,(tg_event){TG_WHEEL,combo,10,30,(uint16_t)(int16_t)-240},script,sizeof script));
    CHECK(tg_ui_apply_event(u,(tg_event){TG_POINTER_DOWN,combo,10,30,0},script,sizeof script));
    CHECK(CALL(".c","get")==TG_OK&&!strcmp(out,"c"));
    CHECK(CALL(".s","configure","-values","","-format","%05.2f","-command","set changed 1")==TG_OK);
    CHECK(CALL(".s","configure","-format","%n")==TG_ERROR);
    CHECK(CALL(".s","set","1")==TG_OK);
    tg_id spin=tg_ui_find(u,".s");
    CHECK(tg_ui_apply_event(u,(tg_event){TG_POINTER_DOWN,spin,95,27,0},script,sizeof script));
    CHECK(!strcmp(script,"set changed 1"));
    CHECK(CALL(".s","get")==TG_OK&&!strcmp(out,"01.50"));
    CHECK(CALL(".s","identify","95","20")==TG_OK&&!strcmp(out,"downarrow"));
    tg_canvas_destroy(a); tg_canvas_destroy(b); tg_ui_destroy(u); puts("choice widgets passed"); return 0;
}
