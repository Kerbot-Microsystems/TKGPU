#include "TKGPUUi.H"
#include "TKGPUAsync.H"
#include <stdio.h>
#include <string.h>
#include "../../src/render/async/TKGPUSnapshot.H"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
#define CALL(...) tg_ui_command(u,sizeof((char *[]){__VA_ARGS__})/sizeof(char *),(char *[]){__VA_ARGS__},out,sizeof out)
int main(void) {
    tg_ui *u=tg_ui_create(); CHECK(u); char out[4096],script[512];
    CHECK(CALL("ttk::treeview",".t","-columns","name watts")==TG_OK);
    CHECK(CALL(".t","heading","#0","-text","Parts")==TG_OK);
    CHECK(CALL(".t","column","name","-width","80")==TG_OK);
    CHECK(CALL(".t","insert","","end","-id","p","-text","Motor","-open","1")==TG_OK);
    CHECK(CALL(".t","insert","p","end","-id","a","-values","{motor A} 200")==TG_OK);
    CHECK(CALL(".t","insert","p","0","-id","b","-values","{motor B} 300")==TG_OK);
    CHECK(CALL(".t","children","p")==TG_OK&&!strcmp(out,"b a"));
    CHECK(CALL(".t","parent","a")==TG_OK&&!strcmp(out,"p"));
    CHECK(CALL(".t","index","b")==TG_OK&&!strcmp(out,"0"));
    CHECK(CALL(".t","index","a")==TG_OK&&!strcmp(out,"1"));
    CHECK(CALL(".t","next","b")==TG_OK&&!strcmp(out,"a"));
    CHECK(CALL(".t","prev","a")==TG_OK&&!strcmp(out,"b"));
    CHECK(CALL(".t","item","a")==TG_OK&&strstr(out,"-values"));
    CHECK(CALL(".t","item","p","-open","0")==TG_OK);
    CHECK(CALL(".t","see","a")==TG_OK);
    CHECK(CALL(".t","item","p","-open")==TG_OK&&!strcmp(out,"1"));
    CHECK(CALL(".t","set","a","name")==TG_OK&&!strcmp(out,"motor A"));
    CHECK(CALL(".t","set","a","watts","007")==TG_OK);
    CHECK(CALL(".t","set","a","watts")==TG_OK&&!strcmp(out,"007"));
    CHECK(CALL(".t","set","a")==TG_OK&&!strcmp(out,"name {motor A} watts 007"));
    CHECK(CALL(".t","set","a","watts","200")==TG_OK);
    CHECK(CALL(".t","move","p","a","end")==TG_ERROR);
    CHECK(CALL(".t","move","a","p","0")==TG_OK);
    CHECK(CALL(".t","children","p")==TG_OK&&!strcmp(out,"a b"));
    CHECK(CALL(".t","move","a","p","end")==TG_OK);
    CHECK(CALL(".t","children","p")==TG_OK&&!strcmp(out,"b a"));
    CHECK(CALL(".t","selection","set","a b")==TG_OK);
    CHECK(CALL(".t","selection","set","a missing")==TG_ERROR);
    CHECK(CALL(".t","selection")==TG_OK&&!strcmp(out,"a b"));
    CHECK(CALL(".t","item","a","-values")==TG_OK&&!strcmp(out,"{motor A} 200"));
    CHECK(CALL("place",".t","-x","0","-y","0","-width","280","-height","90")==TG_OK);
    CHECK(tg_ui_arrange(u,280,90)==TG_OK);
    CHECK(CALL(".t","yview","moveto","0")==TG_OK);
    int bx,by,bw,bh; char sx[32],sy[32];
    CHECK(CALL(".t","bbox","p","#0")==TG_OK&&sscanf(out,"%d %d %d %d",&bx,&by,&bw,&bh)==4&&bw>0&&bh>0);
    snprintf(sx,sizeof sx,"%d",bx+bw/2);
    snprintf(sy,sizeof sy,"%d",by+bh/2);
    CHECK(CALL(".t","identify","row",sx,sy)==TG_OK&&!strcmp(out,"p"));
    CHECK(CALL(".t","identify","column",sx,sy)==TG_OK&&!strcmp(out,"#0"));
    CHECK(CALL(".t","identify","region",sx,sy)==TG_OK&&!strcmp(out,"tree"));
    CHECK(CALL(".t","identify","region",sx,"1")==TG_OK&&!strcmp(out,"heading"));
    CHECK(CALL(".t","bbox","missing")==TG_ERROR);
    CHECK(CALL(".t","identify","row",sx,"9999")==TG_OK&&!out[0]);
    tg_id id=tg_ui_find(u,".t");
    CHECK(tg_ui_apply_event(u,(tg_event){TG_POINTER_DOWN,id,5,25,0},script,sizeof script));
    CHECK(CALL(".t","item","p","-open")==TG_OK&&!strcmp(out,"0"));
    CHECK(tg_ui_apply_event(u,(tg_event){TG_POINTER_DOWN,id,5,25,0},script,sizeof script));
    CHECK(tg_ui_apply_event(u,(tg_event){TG_POINTER_DOWN,id,70,45,0},script,sizeof script));
    CHECK(CALL(".t","selection")==TG_OK&&!strcmp(out,"b"));
    tg_canvas *a=tg_canvas_create(280,90),*b=tg_canvas_create(280,90); CHECK(a&&b);
    CHECK(tg_ui_paint(u,a)==TG_OK&&tg_ui_paint_parallel(u,b,8,NULL)==TG_OK);
    CHECK(!memcmp(tg_canvas_pixels(a),tg_canvas_pixels(b),280u*90u*4u));
    tg_ui *snap=tg_snapshot_create(u); CHECK(snap);
    CHECK(CALL(".t","delete","p")==TG_OK);
    CHECK(CALL(".t","exists","a")==TG_OK&&!strcmp(out,"0"));
    CHECK(tg_ui_paint_parallel(snap,b,4,NULL)==TG_OK&&!memcmp(tg_canvas_pixels(a),tg_canvas_pixels(b),280u*90u*4u));
    tg_ui_destroy(snap);
    for(int k=0;k<1024;k++) CHECK(CALL(".t","insert","","end","-text","row")==TG_OK);
    CHECK(CALL(".t","insert","","end")==TG_ERROR);
    CHECK(CALL("destroy",".t")==TG_OK);
    tg_canvas_destroy(a); tg_canvas_destroy(b); tg_ui_destroy(u); puts("tree hierarchy, selection, pixels, snapshots and limits passed"); return 0;
}
