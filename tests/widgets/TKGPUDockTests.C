#include "TKGPUUi.H"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
#define E(s,v) CHECK(tg_eval(i,s)==TG_OK&&!strcmp(tg_result(i),v))
int main(void) {
    tg_dock d={0}; const int anchors[]={0,0,0,0,760,0,0,560};
    tg_dock_begin(&d,100,100,5,5);
    tg_dock_move(&d,101,101,800,600,40,40,anchors);
    CHECK(d.preview==TG_DOCK_NONE);
    tg_dock_move(&d,400,300,800,600,40,40,anchors);
    CHECK(d.x==395&&d.y==295&&d.preview==TG_DOCK_NONE);
    tg_dock_move(&d,5,5,800,600,40,40,anchors);
    CHECK(d.x==4&&d.y==4&&d.preview==TG_DOCK_TOP);
    CHECK(tg_dock_end(&d)==TG_DOCK_TOP&&!d.active);
    tg_dock_begin(&d,400,300,5,5);
    tg_dock_move(&d,1000,1000,800,600,40,40,anchors);
    CHECK(d.x==756&&d.y==556&&d.preview==TG_DOCK_BOTTOM);
    CHECK(tg_dock_end(&d)==TG_DOCK_BOTTOM);
    tg_interp *i=tg_create(); CHECK(i);
    E("frame .bar",".bar");
    E("tkgpu::dock .bar begin 100 100 5 5","");
    E("tkgpu::dock .bar move 400 300 800 600 40 40 0 0 0 0 760 0 0 560","395 295 none");
    E("tkgpu::dock .bar move 750 300 800 600 40 40 0 0 0 0 760 0 0 560","745 295 right");
    E("tkgpu::dock .bar end","right");
    E("tkgpu::dock .bar reset top","");
    E("frame .other",".other");
    E("place .bar -x 0 -y 0 -width 100 -height 100","");
    E("place .other -x 0 -y 0 -width 100 -height 100","");
    CHECK(tg_ui_arrange(tg_interp_ui(i),200,200)==TG_OK);
    CHECK(tg_ui_hit(tg_interp_ui(i),5,5)==tg_ui_find(tg_interp_ui(i),".other"));
    E("raise .bar","");
    CHECK(tg_ui_hit(tg_interp_ui(i),5,5)==tg_ui_find(tg_interp_ui(i),".bar"));
    E("bind .bar <B1-Motion> {set drag %X}","");
    E("bind .bar <ButtonRelease-1> {set release %Y}","");
    E("bind .bar <Double-Button-1> {set double 1}","");
    tg_ui *u=tg_interp_ui(i); tg_id bar=tg_ui_find(u,".bar"),other=tg_ui_find(u,".other");
    CHECK(tg_ui_post(u,(tg_event){TG_POINTER_DOWN,bar,5,5,0})==TG_OK);
    CHECK(tg_interp_dispatch_input(i));
    CHECK(tg_ui_post(u,(tg_event){TG_POINTER_MOVE,other,125,130,0})==TG_OK);
    CHECK(tg_interp_dispatch_input(i));
    E("set drag","125");
    CHECK(tg_ui_post(u,(tg_event){TG_POINTER_UP,other,125,130,0})==TG_OK);
    CHECK(tg_interp_dispatch_input(i));
    E("set release","130");
    E("frame .parent",".parent");
    E("frame .parent.a -width 20",".parent.a");
    E("frame .parent.b -width 20",".parent.b");
    E("place .parent -x 0 -y 0 -width 100 -height 100","");
    E("pack .parent.a -side left","");
    E("pack .parent.b -side left -before .parent.a -padx {2 2}","");
    E("pack slaves .parent",".parent.b .parent.a");
    CHECK(tg_ui_arrange(u,200,200)==TG_OK);
    E("winfo x .parent.b","2");
    E(".parent.b configure -relief raised -borderwidth 1","");
    E(".parent.b cget -relief","raised");
    E("destroy .bar","");
    CHECK(tg_eval(i,"tkgpu::dock .bar end")==TG_ERROR);
    tg_destroy(i); return 0;
}
