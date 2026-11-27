#include "TKGPUUi.H"
#include "../../src/widgets/TKGPUWidgetInternal.H"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
#define E(s,v) CHECK(tg_eval(i,s)==TG_OK&&!strcmp(tg_result(i),v))
static int translation_regression(void) {
    tg_interp *i=tg_create(); CHECK(i); tg_ui *u=tg_interp_ui(i);
    u->native_toplevels=1;
    E("frame .d; frame .d.inner; pack .d.inner -fill both -expand 1; label .d.inner.l -text hello; pack .d.inner.l; place .d -x 10 -y 20 -width 200 -height 100","");
    CHECK(tg_ui_arrange(u,800,600)==TG_OK);
    tg_event event;
    while(tg_ui_poll(u,&event)) {}
    E("place .d -x 50 -y 60","");
    CHECK(tg_ui_arrange(u,800,600)==TG_OK);
    tg_id dialog=tg_ui_find(u,".d"); int configured=0;
    while(tg_ui_poll(u,&event)) {
        if(event.kind==TG_CONFIGURE) {
            CHECK(event.target==dialog);
            configured++;
        }
    }
    CHECK(configured==1);
    tg_destroy(i); return 0;
}
static int native_drag_fast_path(void) {
    tg_interp *i=tg_create(); CHECK(i); tg_ui *u=tg_interp_ui(i);
    u->native_toplevels=1;
    E("frame .d; frame .d.title -height 32; pack .d.title -side top -fill x; frame .other; place .other -x 10 -y 10 -width 40 -height 40; tkgpu::dialog .d attach 300 200 32 0; tkgpu::dialog .d title .d.title; tkgpu::dialog .d show","");
    CHECK(tg_ui_arrange(u,800,600)==TG_OK);
    tg_event e; while(tg_ui_poll(u,&e)) {}
    tg_id id=tg_ui_find(u,".d"),title=tg_ui_find(u,".d.title"),other=tg_ui_find(u,".other");
    tg_rect before=tg_ui_get(u,id)->rect, child=tg_ui_get(u,title)->rect, unrelated=tg_ui_get(u,other)->rect;
    int consumed=0;
    CHECK(tg_base_dialog_event(u,(tg_event){TG_POINTER_DOWN,title,before.x+5,before.y+5,1},&consumed)&&consumed);
    CHECK(tg_base_dialog_event(u,(tg_event){TG_POINTER_MOVE,id,before.x+35,before.y+25,1},&consumed)==2&&consumed);
    CHECK(tg_ui_get(u,id)->rect.x==before.x+30&&tg_ui_get(u,id)->rect.y==before.y+20);
    CHECK(tg_ui_get(u,title)->rect.x==child.x+30&&tg_ui_get(u,title)->rect.y==child.y+20);
    CHECK(!memcmp(&tg_ui_get(u,other)->rect,&unrelated,sizeof unrelated));
    CHECK(!tg_base_dialog_event(u,(tg_event){TG_POINTER_UP,id,before.x+35,before.y+25,1},&consumed)&&consumed);
    CHECK(!u->dialog_capture);
    tg_destroy(i); return 0;
}
int main(void) {
    CHECK(native_drag_fast_path()==0);
    CHECK(translation_regression()==0);
    tg_interp *i=tg_create(); CHECK(i); tg_ui *u=tg_interp_ui(i);
    E("frame .d",".d"); E("frame .d.title",".d.title");
    E("pack .d.title -side top -fill x","");
    CHECK(tg_ui_arrange(u,800,600)==TG_OK);
    E("tkgpu::dialog .d attach 400 200 32 0","");
    E("tkgpu::dialog .d title .d.title","");
    E("tkgpu::dialog .d show","");
    E("tkgpu::dialog .d state","1 0 0 200 200 200");
    CHECK(tg_ui_arrange(u,800,600)==TG_OK);
    tg_id title=tg_ui_find(u,".d.title"),id=tg_ui_find(u,".d");
    CHECK(tg_ui_post(u,(tg_event){TG_POINTER_DOWN,title,220,210,0})==TG_OK);
    CHECK(tg_interp_dispatch_input(i));
    CHECK(tg_ui_post(u,(tg_event){TG_POINTER_MOVE,1,720,590,0})==TG_OK);
    CHECK(tg_interp_dispatch_input(i));
    E("tkgpu::dialog .d state","1 0 1 400 400 200");
    CHECK(tg_ui_post(u,(tg_event){TG_POINTER_UP,1,720,590,0})==TG_OK);
    CHECK(tg_interp_dispatch_input(i));
    E("tkgpu::dialog .d state","1 0 0 400 400 200");
    E("tkgpu::dialog .d minimize","1 38");
    E("tkgpu::dialog .d minimize","0 200");
    E("tkgpu::dialog .d hide","");
    CHECK(tg_ui_get(u,id)->layout==TG_FREE);
    E("tkgpu::dialog .d show","");
    E("place .d -x 30 -y 40 -width 320 -height 180","");
    E("tkgpu::dialog .d state","1 0 0 30 40 180");
    E("place forget .d","");
    E("tkgpu::dialog .d state","0 0 0 30 40 180");
    E("tkgpu::dialog .d show","");
    E("frame .shade",".shade");
    E("tkgpu::dialog .d shadow .shade 4","");
    CHECK(tg_ui_get(u,tg_ui_find(u,".shade"))->layout==TG_PLACE);
    E("tkgpu::dialog .d hide","");
    CHECK(tg_ui_get(u,tg_ui_find(u,".shade"))->layout==TG_FREE);
    E("tkgpu::dialog .d show","");
    E("frame .d.footer -height 20",".d.footer");
    E("pack .d.footer -in .d -side bottom -fill x -padx {3 7} -pady {0 10}","");
    CHECK(tg_ui_arrange(u,800,600)==TG_OK);
    const tg_widget *footer=tg_ui_get(u,tg_ui_find(u,".d.footer"));
    CHECK(footer->rect.x==tg_ui_get(u,id)->rect.x+3&&footer->rect.w==310&&footer->rect.h==20);
    CHECK(tg_eval(i,"pack info .d.footer")==TG_OK&&strstr(tg_result(i),"-padx {3 7}")&&strstr(tg_result(i),"-pady {0 10}"));
    E("pack forget .d.footer","");
    E("pack .d.footer -in .d -side bottom -fill x -padx {3 7} -pady {0 10}","");
    CHECK(tg_eval(i,"pack .d.footer -pady {1 -2}")==TG_ERROR);
    E("destroy .d","");
    CHECK(tg_eval(i,"tkgpu::dialog .d state")==TG_ERROR);
    tg_destroy(i); return 0;
}
