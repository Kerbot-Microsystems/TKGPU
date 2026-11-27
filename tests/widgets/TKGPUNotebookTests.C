#include "TKGPUUi.H"
#include "TKGPUAsync.H"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
#define CALL(...) tg_ui_command(u,sizeof((char *[]){__VA_ARGS__})/sizeof(char *),(char *[]){__VA_ARGS__},out,sizeof out)
int main(void) {
    tg_ui *u=tg_ui_create(); CHECK(u); char out[512],script[512];
    CHECK(CALL("ttk::notebook",".n")==TG_OK);
    CHECK(CALL("ttk::frame",".n.a","-background","#ff0000")==TG_OK);
    CHECK(CALL("ttk::frame",".n.b","-background","#0000ff")==TG_OK);
    CHECK(CALL("ttk::label",".n.a.l","-text","A")==TG_OK);
    CHECK(CALL("pack",".n.a.l")==TG_OK);
    CHECK(CALL(".n","add",".n.b","-text","Bee")==TG_OK);
    CHECK(CALL(".n","add",".n.a","-text","Aye")==TG_OK);
    CHECK(CALL(".n","tabs")==TG_OK&&!strcmp(out,".n.b .n.a"));
    CHECK(CALL(".n","select")==TG_OK&&!strcmp(out,".n.b"));
    CHECK(CALL(".n","insert","0",".n.a")==TG_OK);
    CHECK(CALL(".n","tabs")==TG_OK&&!strcmp(out,".n.a .n.b"));
    CHECK(CALL(".n","select")==TG_OK&&!strcmp(out,".n.b"));
    CHECK(CALL(".n","insert","end",".n.a","-text","Aye")==TG_OK);
    CHECK(CALL(".n","tabs")==TG_OK&&!strcmp(out,".n.b .n.a"));
    CHECK(CALL(".n","tab",".n.a")==TG_OK&&!strcmp(out,"-text Aye -state normal -padding 0 -sticky nsew -image {} -compound {} -underline -1"));
    CHECK(CALL(".n","tab",".n.a","-padding","2 3 4","-sticky","nw","-underline","0","-compound","left")==TG_OK);
    CHECK(CALL(".n","tab",".n.a","-padding")==TG_OK&&!strcmp(out,"2 3 4"));
    CHECK(CALL(".n","tab",".n.a","-sticky")==TG_OK&&!strcmp(out,"nw"));
    CHECK(CALL(".n","tab",".n.a","-underline")==TG_OK&&!strcmp(out,"0"));
    CHECK(CALL(".n","tab",".n.a","-padding","5","-sticky","bad")==TG_ERROR);
    CHECK(CALL(".n","tab",".n.a","-padding")==TG_OK&&!strcmp(out,"2 3 4"));
    CHECK(CALL(".n","tab",".n.a","-padding","0","-sticky","nsew")==TG_OK);
    CHECK(CALL(".n","insert","0",".n.a","-invalid","x")==TG_ERROR);
    CHECK(CALL(".n","tabs")==TG_OK&&!strcmp(out,".n.b .n.a"));
    CHECK(CALL("ttk::frame",".n.c")==TG_OK);
    CHECK(CALL(".n","insert","0",".n.c","-text","Third page")==TG_OK);
    CHECK(CALL(".n","tabs")==TG_OK&&!strcmp(out,".n.c .n.b .n.a"));
    CHECK(CALL(".n","select")==TG_OK&&!strcmp(out,".n.b"));
    CHECK(CALL(".n","forget",".n.c")==TG_OK);
    CHECK(CALL("destroy",".n.c")==TG_OK);
    CHECK(CALL(".n","tab",".n.a","-state","disabled")==TG_OK);
    CHECK(CALL(".n","select",".n.a")==TG_OK);
    CHECK(CALL(".n","select")==TG_OK&&!strcmp(out,".n.b"));
    CHECK(CALL(".n","hide",".n.b")==TG_OK);
    CHECK(CALL(".n","select")==TG_OK&&!*out);
    CHECK(CALL(".n","select",".n.b")==TG_OK);
    CHECK(CALL(".n","tab",".n.b","-state")==TG_OK&&!strcmp(out,"normal"));
    CHECK(CALL(".n","index","end")==TG_OK&&!strcmp(out,"2"));
    CHECK(CALL(".n","tab",".n.a","-state","normal")==TG_OK);
    CHECK(CALL("place",".n","-x","0","-y","0","-width","120","-height","80")==TG_OK);
    CHECK(tg_ui_arrange(u,120,80)==TG_OK);
    tg_id n=tg_ui_find(u,".n"),a=tg_ui_find(u,".n.a"),b=tg_ui_find(u,".n.b"),l=tg_ui_find(u,".n.a.l");
    CHECK(tg_ui_get(u,a)->rect.w==0&&tg_ui_get(u,l)->rect.w==0);
    CHECK(tg_ui_hit(u,5,40)==b);
    tg_event event; while(tg_ui_poll(u,&event)) {}
    CHECK(tg_ui_apply_event(u,(tg_event){TG_POINTER_DOWN,n,45,10,0},script,sizeof script));
    CHECK(tg_ui_poll(u,&event)&&event.kind==tg_virtual_event("NotebookTabChanged"));
    CHECK(!tg_ui_poll(u,&event));
    CHECK(tg_ui_apply_event(u,(tg_event){TG_POINTER_DOWN,n,45,10,0},script,sizeof script));
    CHECK(!tg_ui_poll(u,&event));
    CHECK(CALL(".n","select")==TG_OK&&!strcmp(out,".n.a"));
    CHECK(tg_ui_apply_event(u,(tg_event){TG_KEY_DOWN,n,0,0,TG_KEY_RIGHT},script,sizeof script));
    CHECK(CALL(".n","select")==TG_OK&&!strcmp(out,".n.b"));
    CHECK(tg_ui_apply_event(u,(tg_event){TG_KEY_DOWN,n,0,0,TG_KEY_TAB|TG_MOD_CONTROL},script,sizeof script));
    CHECK(CALL(".n","select")==TG_OK&&!strcmp(out,".n.a"));
    CHECK(CALL(".n","tab",".n.b","-state","disabled")==TG_OK);
    CHECK(tg_ui_apply_event(u,(tg_event){TG_KEY_DOWN,n,0,0,TG_KEY_LEFT},script,sizeof script));
    CHECK(CALL(".n","select")==TG_OK&&!strcmp(out,".n.a"));
    CHECK(CALL(".n","tab",".n.b","-state","normal")==TG_OK);
    CHECK(CALL("ttk::notebook::enableTraversal",".n")==TG_OK);
    CHECK(CALL(".n","tab",".n.b","-underline","0")==TG_OK);
    CHECK(CALL("focus",".n.a.l")==TG_OK);
    CHECK(tg_ui_apply_event(u,(tg_event){TG_KEY_DOWN,l,0,0,TG_KEY_TAB|TG_MOD_CONTROL},script,sizeof script));
    CHECK(CALL(".n","select")==TG_OK&&!strcmp(out,".n.b"));
    CHECK(tg_ui_apply_event(u,(tg_event){TG_KEY_DOWN,n,0,0,'a'|TG_MOD_ALT},script,sizeof script));
    CHECK(CALL(".n","select")==TG_OK&&!strcmp(out,".n.a"));
    CHECK(CALL(".n","tab",".n.b","-state","disabled")==TG_OK);
    CHECK(!tg_ui_apply_event(u,(tg_event){TG_KEY_DOWN,n,0,0,'b'|TG_MOD_ALT},script,sizeof script));
    CHECK(CALL(".n","select")==TG_OK&&!strcmp(out,".n.a"));
    CHECK(CALL(".n","tab",".n.b","-state","normal")==TG_OK);
    CHECK(tg_ui_arrange(u,120,80)==TG_OK&&tg_ui_get(u,b)->rect.w==0);
    tg_canvas *x=tg_canvas_create(120,80),*y=tg_canvas_create(120,80); CHECK(x&&y);
    CHECK(tg_ui_paint(u,x)==TG_OK&&tg_ui_paint_parallel(u,y,8,NULL)==TG_OK);
    CHECK(!memcmp(tg_canvas_pixels(x),tg_canvas_pixels(y),120u*80u*4u));
    CHECK(CALL("destroy",".n.a")==TG_OK);
    CHECK(CALL(".n","select")==TG_OK&&!strcmp(out,".n.b"));
    CHECK(CALL(".n","forget","0")==TG_OK);
    CHECK(CALL(".n","tabs")==TG_OK&&!*out);
    CHECK(tg_ui_arrange(u,120,80)==TG_OK&&tg_ui_get(u,b)->rect.w==0);
    tg_canvas_destroy(x); tg_canvas_destroy(y); tg_ui_destroy(u); puts("notebook order, selection, layout, deletion and rendering passed"); return 0;
}
