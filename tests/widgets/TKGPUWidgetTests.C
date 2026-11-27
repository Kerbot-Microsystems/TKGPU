#include "../../src/widgets/ttk/TKGPUToggle.H"
#include "../../src/widgets/ttk/TKGPUStyle.H"
#include "../../src/widgets/TKGPUWidgetInternal.H"
#include "TKGPUUi.H"
#include "../../src/core/TKGPUCoreInternal.H"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "TKGPURaster.H"
#include "../../src/render/async/TKGPUSnapshot.H"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
static void invalidate(void *p) { (*(int *)p)++; }
typedef struct {
    tg_canvas *canvas;
    unsigned depth,peak,passes;
    struct { tg_rect rect; unsigned alpha; uint32_t *pixels; } stack[8];
} replay_context;
static int replay_rect(void *ctx,tg_rect rect,uint32_t rgb,unsigned alpha) {
    replay_context *r=ctx;
    tg_canvas_fill_alpha(r->canvas,rect,rgb,alpha); return TG_OK;
}
static int replay_layer(void *ctx,tg_rect q,unsigned alpha,int begin) {
    replay_context *r=ctx; int stride=tg_canvas_width(r->canvas);
    const uint32_t *p=tg_canvas_pixels(r->canvas);
    if(q.x<0||q.y<0||q.w<=0||q.h<=0||q.x+q.w>stride||q.y+q.h>tg_canvas_height(r->canvas)) return TG_ERROR;
    if(begin) {
        if(r->depth==8) return TG_ERROR;
        unsigned k=r->depth;
        uint32_t *save=malloc((size_t)q.w*q.h*sizeof *save);
        if(!save) return TG_ERROR;
        for(int y=0;y<q.h;y++) memcpy(save+(size_t)y*q.w,p+(size_t)(q.y+y)*stride+q.x,(size_t)q.w*sizeof *save);
        r->stack[k].rect=q; r->stack[k].alpha=alpha; r->stack[k].pixels=save;
        if(++r->depth>r->peak) r->peak=r->depth;
        return TG_OK;
    }
    if(!r->depth) return TG_ERROR;
    unsigned k=r->depth-1; tg_rect old=r->stack[k].rect;
    if(old.x!=q.x||old.y!=q.y||old.w!=q.w||old.h!=q.h||r->stack[k].alpha!=alpha) return TG_ERROR;
    for(int y=0;y<q.h;y++) for(int x=0;x<q.w;x++) {
        uint32_t saved=r->stack[k].pixels[(size_t)y*q.w+x];
        uint32_t current=p[(size_t)(q.y+y)*stride+q.x+x];
        uint32_t mixed=tg_pixel_blend(saved,((current&0xffffffu)<<8)|alpha);
        tg_canvas_fill(r->canvas,(tg_rect){q.x+x,q.y+y,1,1},mixed);
    }
    free(r->stack[k].pixels); r->depth--; r->passes++; return TG_OK;
}
static int composition_test(void) {
    tg_interp *i=tg_create(); CHECK(i);
    tg_ui *u=tg_interp_ui(i);
    CHECK(tg_eval(i,"frame .g -bg red -groupopacity 0.5; place .g -width 32 -height 32; canvas .g.c -bg blue -opacity 0.6; place .g.c -x 2 -y 2 -width 20 -height 20; .g.c create rectangle 0 0 12 12 -fill white -outline red -opacity 0.4; .g.c create oval 4 4 16 16 -fill blue -opacity 0.5")==TG_OK);
    CHECK(tg_ui_arrange(u,40,40)==TG_OK);
    tg_canvas *cpu=tg_canvas_create(40,40),*recorded=tg_canvas_create(40,40); CHECK(cpu&&recorded);
    replay_context r={0}; r.canvas=recorded;
    CHECK(tg_ui_paint(u,cpu)==TG_OK);
    CHECK(tg_ui_record_composite(u,40,40,replay_rect,NULL,replay_layer,&r)==TG_OK);
    CHECK(!r.depth&&r.peak==3&&r.passes==4);
    CHECK(!memcmp(tg_canvas_pixels(cpu),tg_canvas_pixels(recorded),40u*40u*4u));
    CHECK(tg_ui_record_rects(u,40,40,replay_rect,&r)==TG_ERROR);
    tg_canvas_destroy(cpu); tg_canvas_destroy(recorded); tg_destroy(i); return 0;
}
static int resource_test(void) {
    tg_interp *i=tg_create(); CHECK(i);
    CHECK(tg_eval(i,"option add *Menu.background red 40; option add *Menu.background blue 80; option add *Menu.foreground white; menu .m -tearoff 0; .m cget -background")==TG_OK&&!strcmp(tg_result(i),"#0000ff"));
    CHECK(tg_eval(i,"option get .m background Background")==TG_OK&&!strcmp(tg_result(i),"blue"));
    CHECK(tg_eval(i,"menu .other -background black; .other cget -background")==TG_OK&&!strcmp(tg_result(i),"#000000"));
    CHECK(tg_eval(i,"option clear; option get .m background Background")==TG_OK&&!strcmp(tg_result(i),""));
    CHECK(tg_eval(i,"frame .g; grid columnconfigure .g 1 -weight 7; grid columnconfigure .g 1 -weight")==TG_OK&&!strcmp(tg_result(i),"7"));
    tg_destroy(i); return 0;
}
static int layout_regression_test(void) {
    tg_interp *i=tg_create(); CHECK(i);
    tg_ui *u=tg_interp_ui(i);
    CHECK(tg_eval(i,"frame .pad -width 20 -height 10; pack .pad -fill none -ipadx 7 -ipady 4")==TG_OK);
    CHECK(tg_ui_arrange(u,100,80)==TG_OK);
    const tg_widget *pad=tg_ui_get(u,tg_ui_find(u,".pad"));
    CHECK(pad&&pad->rect.w==34&&pad->rect.h==18);
    CHECK(tg_eval(i,"pack info .pad")==TG_OK&&strstr(tg_result(i),"-ipadx 7")&&strstr(tg_result(i),"-ipady 4"));
    CHECK(tg_eval(i,"pack configure .pad -ipadx 2; pack info .pad")==TG_OK&&strstr(tg_result(i),"-ipady 4"));
    CHECK(tg_ui_arrange(u,100,80)==TG_OK&&pad->rect.w==24&&pad->rect.h==18);
    CHECK(tg_eval(i,"destroy .pad; frame .p; pack .p; image create photo logo -width 20 -height 10; label .p.logo -image logo; grid .p.logo -column 1 -row 1; .p.logo configure -padx 3")==TG_OK);
    const tg_widget *logo=tg_ui_get(u,tg_ui_find(u,".p.logo"));
    CHECK(logo&&logo->request.x==1&&logo->request.y==1&&logo->request.w==1&&logo->request.h==1);
    CHECK(logo->grid_natural.w>=26);
    tg_destroy(i); return 0;
}
static int scroll_and_highlight_regression(void) {
    tg_interp *i=tg_create(); CHECK(i);
    tg_ui *u=tg_interp_ui(i);
    CHECK(tg_eval(i,"canvas .c -scrollregion {0 0 100 500} -yscrollcommand {scroll_callback} -bg white -highlightthickness 1; place .c -width 100 -height 100")==TG_OK);
    CHECK(tg_ui_arrange(u,100,100)==TG_OK);
    CHECK(tg_eval(i,".c cget -scrollregion")==TG_OK&&!strcmp(tg_result(i),"0 0 100 500"));
    CHECK(tg_eval(i,".c cget -yscrollcommand")==TG_OK&&!strcmp(tg_result(i),"scroll_callback"));
    CHECK(tg_eval(i,".c yview scroll 1 units")==TG_OK);
    const tg_widget *w=tg_ui_get(u,tg_ui_find(u,".c"));
    CHECK(w&&w->canvas_y>0&&w->canvas_region_set);
    CHECK(tg_eval(i,".c configure -scrollregion {0 0 100 800}; .c cget -scrollregion")==TG_OK&&!strcmp(tg_result(i),"0 0 100 800"));
    CHECK(tg_eval(i,".c cget -highlightthickness")==TG_OK&&!strcmp(tg_result(i),"1"));
    tg_canvas *c=tg_canvas_create(100,100); CHECK(c);
    CHECK(tg_ui_paint(u,c)==TG_OK&&tg_canvas_pixels(c)[0]==0xffffff);
    CHECK(tg_eval(i,".c configure -highlightbackground black; .c cget -highlightbackground")==TG_OK&&!strcmp(tg_result(i),"#000000"));
    CHECK(tg_ui_paint(u,c)==TG_OK&&tg_canvas_pixels(c)[0]==0);
    tg_canvas_destroy(c); tg_destroy(i); return 0;
}
static int natural_place_regression(void) {
    tg_interp *i=tg_create(); CHECK(i);
    tg_ui *u=tg_interp_ui(i);
    u->native_toplevels=1;
    CHECK(tg_eval(i,"frame .square -width 24 -height 24; place .square -x -10 -y 20 -width 60 -height 32")==TG_OK);
    CHECK(tg_eval(i,"winfo reqwidth .square")==TG_OK&&!strcmp(tg_result(i),"24"));
    CHECK(tg_eval(i,"place .square -width {} -height {}; winfo reqheight .square")==TG_OK&&!strcmp(tg_result(i),"24"));
    CHECK(tg_ui_arrange(u,200,100)==TG_OK);
    const tg_widget *w=tg_ui_get(u,tg_ui_find(u,".square"));
    CHECK(w&&w->rect.w==24&&w->rect.h==24);
    CHECK(tg_eval(i,"place .square -width 80 -height 45; pack .square -side left")==TG_OK);
    CHECK(tg_ui_arrange(u,200,100)==TG_OK&&w->rect.w==24&&w->rect.h==24);
    CHECK(tg_eval(i,"place .square -width 60 -height 32; place forget .square; pack .square -side top")==TG_OK);
    CHECK(tg_ui_arrange(u,200,100)==TG_OK&&w->rect.w==24&&w->rect.h==24);
    tg_destroy(i); return 0;
}
static int themed_radio_regression(void) {
    tg_interp *i=tg_create(); CHECK(i); tg_ui *u=tg_interp_ui(i);
    u->native_toplevels=1;
    CHECK(tg_eval(i,"ttk::style configure TRadiobutton -indicatordiameter 14 -padding {4 3} -font {IBM Plex Sans 10} -indicatorcolor #eeeeee; ttk::style map TRadiobutton -indicatorcolor {selected #0078d7}; set choice python; ttk::radiobutton .r -text {Window Extension (Classic Panel)} -variable choice -value window -style Kerbot.TRadiobutton; pack .r")==TG_OK);
    const tg_widget *w=tg_ui_get(u,tg_ui_find(u,".r")); CHECK(w);
    CHECK(tg_toggle_diameter(u,w)==14&&w->request.w>100&&w->text_padx==4);
    uint32_t color;
    CHECK(tg_style_widget_color(u,w,"-indicatorcolor",&color)&&color==0xeeeeee);
    CHECK(tg_eval(i,"set choice window")==TG_OK);
    CHECK(tg_style_widget_color(u,w,"-indicatorcolor",&color)&&color==0x0078d7);
    CHECK(tg_ui_arrange(u,400,100)==TG_OK);
    tg_canvas *canvas=tg_canvas_create(400,100); CHECK(canvas&&tg_ui_paint(u,canvas)==TG_OK);
    tg_canvas_destroy(canvas); tg_destroy(i); return 0;
}
static int path_index_regression(int linear) {
    tg_ui *u=tg_ui_create(); CHECK(u);
    u->path_index_disabled=linear;
    tg_id group=tg_ui_add(u,".parent",TG_FRAME,NULL); CHECK(group);
    tg_id ids[160];
    for(unsigned k=0;k<160;k++) {
        char name[64]; snprintf(name,sizeof name,".parent.w%u",k);
        ids[k]=tg_ui_add(u,name,TG_LABEL,NULL); CHECK(ids[k]);
    }
    CHECK(u->path_index_ready==!linear&&tg_ui_find(u,".")==1);
    CHECK(tg_ui_find(u,".parent.missing")==0);
    CHECK(!tg_ui_add(u,".parent.w20",TG_LABEL,NULL));
    tg_ui *copy=tg_snapshot_create(u); CHECK(copy);
    CHECK(copy->path_index_disabled==linear&&copy->path_index_ready==!linear);
    for(unsigned k=0;k<160;k+=2) CHECK(tg_ui_remove(u,ids[k])==TG_OK);
    for(unsigned k=0;k<160;k++) {
        char name[64]; snprintf(name,sizeof name,".parent.w%u",k);
        CHECK(tg_ui_find(copy,name)==ids[k]);
        CHECK(tg_ui_find(u,name)==(k%2?ids[k]:0));
        if(!(k%2)) {
            tg_id id=tg_ui_add(u,name,TG_LABEL,NULL); CHECK(id&&id!=ids[k]);
            CHECK(tg_ui_find(u,name)==id);
        }
    }
    CHECK(tg_ui_remove(u,group)==TG_OK&&!u->path_index_ready);
    CHECK(tg_ui_find(u,".parent.w20")==0&&tg_ui_find(u,".")==1);
    CHECK(tg_ui_find(copy,".parent.w20")==ids[20]);
    tg_ui_destroy(copy); tg_ui_destroy(u); return 0;
}
int main(void) {
    CHECK(path_index_regression(0)==0);
    CHECK(path_index_regression(1)==0);
    CHECK(themed_radio_regression()==0);
    CHECK(natural_place_regression()==0);
    CHECK(scroll_and_highlight_regression()==0);
    CHECK(layout_regression_test()==0);
    CHECK(resource_test()==0);
    CHECK(composition_test()==0);
    tg_interp *i=tg_create(); CHECK(i);
    CHECK(tg_eval(i,"frame .f; button .f.b -text OK; pack .f; place .f.b -x 4 -y 4 -width 20 -height 20")==TG_OK);
    CHECK(tg_ui_arrange(tg_interp_ui(i),100,80)==TG_OK);
    CHECK(tg_ui_hit(tg_interp_ui(i),5,5)==tg_ui_find(tg_interp_ui(i),".f.b"));
    CHECK(tg_eval(i,"winfo exists .f.b")==TG_OK&&tg_result(i)[0]=='1');
    CHECK(tg_eval(i,"destroy .f; winfo exists .f.b")==TG_OK&&tg_result(i)[0]=='0');
    CHECK(tg_eval(i,"frame .g; pack .g; button .g.a; label .g.b; grid .g.a -column 0; grid .g.b -column 1")==TG_OK);
    CHECK(tg_ui_arrange(tg_interp_ui(i),100,80)==TG_OK);
    const tg_widget *g=tg_ui_get(tg_interp_ui(i),tg_ui_find(tg_interp_ui(i),".g.b"));
    CHECK(g&&g->rect.x==50&&g->rect.w==50);
    CHECK(tg_eval(i,"pack .g.a")==TG_ERROR);
    CHECK(tg_eval(i,".g.a configure -text Hello -bg #123456; .g.a cget -text")==TG_OK);
    CHECK(!strcmp(tg_result(i),"Hello"));
    CHECK(tg_eval(i,".g.a cget -bg")==TG_OK&&!strcmp(tg_result(i),"#123456"));
    CHECK(tg_eval(i,".g.a configure -text Changed -bg bad")==TG_ERROR);
    CHECK(tg_eval(i,".g.a cget -text")==TG_OK&&!strcmp(tg_result(i),"Hello"));
    CHECK(tg_eval(i,"destroy .g; frame .a; frame .b; pack .a -side bottom -fill x -padx 5 -pady 2; pack .b -expand 1 -fill both")==TG_OK);
    CHECK(tg_ui_arrange(tg_interp_ui(i),100,100)==TG_OK);
    const tg_widget *pa=tg_ui_get(tg_interp_ui(i),tg_ui_find(tg_interp_ui(i),".a"));
    const tg_widget *pb=tg_ui_get(tg_interp_ui(i),tg_ui_find(tg_interp_ui(i),".b"));
    CHECK(pa&&pa->rect.x==5&&pa->rect.y==66&&pa->rect.w==90&&pa->rect.h==32);
    CHECK(pb&&pb->rect.y==0&&pb->rect.h==64&&pb->rect.w==100);
    CHECK(tg_eval(i,"pack .b -fill invalid")==TG_ERROR);
    CHECK(tg_ui_arrange(tg_interp_ui(i),100,100)==TG_OK&&pb->rect.h==64);
    CHECK(tg_eval(i,"scale .s -orient vertical; scrollbar .sb; labelframe .lf -text Group; spinbox .sp -from 0 -to 5; checkbutton .cb -text Check; radiobutton .rb -text Radio; panedwindow .pw; menubutton .mb -text Menu")==TG_OK);
    CHECK(!tg_ui_get(tg_interp_ui(i),tg_ui_find(tg_interp_ui(i),".cb"))->themed);
    CHECK(tg_eval(i,"progressbar .invalid")==TG_ERROR);
    CHECK(tg_eval(i,"frame .tk; pack configure .tk -fill both -expand 1; label .tk.a; grid configure .tk.a -row 0 -column 0; label .tk.b; place configure .tk.b -x 3 -y 4")==TG_OK);
    CHECK(tg_eval(i,"grid slaves .tk")==TG_OK&&!strcmp(tg_result(i),".tk.a"));
    CHECK(tg_eval(i,"winfo children .tk")==TG_OK&&!strcmp(tg_result(i),".tk.a .tk.b"));
    CHECK(tg_eval(i,"winfo parent .tk.a")==TG_OK&&!strcmp(tg_result(i),".tk"));
    CHECK(tg_eval(i,"winfo class .tk.a")==TG_OK&&!strcmp(tg_result(i),"Label"));
    CHECK(tg_eval(i,"winfo toplevel .tk.a")==TG_OK&&!strcmp(tg_result(i),"."));
    CHECK(tg_eval(i,"grid info .tk.a")==TG_OK&&strstr(tg_result(i),"-in .tk")&&strstr(tg_result(i),"-row 0"));
    CHECK(tg_eval(i,"winfo viewable .tk.a")==TG_OK&&!strcmp(tg_result(i),"1"));
    CHECK(tg_eval(i,"pack forget .tk; winfo manager .tk")==TG_OK&&!strcmp(tg_result(i),""));
    CHECK(tg_eval(i,"winfo ismapped .tk.a")==TG_OK&&!strcmp(tg_result(i),"1"));
    CHECK(tg_eval(i,"winfo viewable .tk.a")==TG_OK&&!strcmp(tg_result(i),"0"));
    CHECK(tg_eval(i,"grid forget .tk.a; winfo manager .tk.a")==TG_OK&&!strcmp(tg_result(i),""));
    CHECK(tg_eval(i,"frame .weighted; place .weighted -width 300 -height 100; label .weighted.a; label .weighted.b; grid .weighted.a -column 0 -sticky nsew; grid .weighted.b -column 1 -sticky nsew; grid columnconfigure .weighted 0 -weight 1; grid columnconfigure .weighted 1 -weight 3")==TG_OK);
    CHECK(tg_ui_arrange(tg_interp_ui(i),300,100)==TG_OK);
    tg_id wa=tg_ui_find(tg_interp_ui(i),".weighted.a"),wb=tg_ui_find(tg_interp_ui(i),".weighted.b");
    CHECK(tg_ui_get(tg_interp_ui(i),wa)->rect.w==125);
    CHECK(tg_ui_get(tg_interp_ui(i),wb)->rect.w==175);
    CHECK(tg_eval(i,"grid remove .weighted.b; grid .weighted.b")==TG_OK);
    CHECK(tg_ui_arrange(tg_interp_ui(i),300,100)==TG_OK);
    CHECK(tg_ui_get(tg_interp_ui(i),wb)->rect.x==125);
    int redraws=0;
    i->tk_host=&redraws; i->tk_invalidate=invalidate;
    CHECK(tg_eval(i,"winfo exists .weighted.a; .weighted.a cget -text; grid slaves .weighted; focus")==TG_OK);
    CHECK(redraws==0);
    CHECK(tg_eval(i,".weighted.a configure -text changed")==TG_OK&&redraws==1);
    i->tk_host=NULL; i->tk_invalidate=NULL;
    CHECK(tg_eval(i,"frame .alpha -bg #ff0000 -opacity 0.5; place .alpha -x 0 -y 0 -width 20 -height 20; frame .overlay -bg #0000ff -backgroundalpha 0.5; place .overlay -x 0 -y 0 -width 10 -height 10")==TG_OK);
    CHECK(tg_eval(i,".alpha configure -opacity -0.1")==TG_ERROR);
    CHECK(tg_eval(i,".alpha configure -opacity NaN")==TG_ERROR);
    CHECK(tg_eval(i,".alpha configure -opacity 1.1")==TG_ERROR);
    CHECK(tg_eval(i,".alpha cget -opacity")==TG_OK&&!strcmp(tg_result(i),"0.501960784"));
    CHECK(tg_eval(i,"destroy .a; destroy .b; destroy .weighted; destroy .tk")==TG_OK);
    CHECK(tg_ui_arrange(tg_interp_ui(i),100,100)==TG_OK);
    tg_canvas *alpha_a=tg_canvas_create(100,100),*alpha_b=tg_canvas_create(100,100);
    CHECK(alpha_a&&alpha_b);
    uint32_t red=tg_pixel_blend(0x182536,0xff000080u);
    uint32_t over=tg_pixel_blend(red,0x0000ff80u);
    CHECK(tg_ui_paint(tg_interp_ui(i),alpha_a)==TG_OK);
    CHECK(tg_canvas_pixels(alpha_a)[0]==over&&tg_canvas_pixels(alpha_a)[15*100+15]==red);
    CHECK(tg_ui_paint_parallel(tg_interp_ui(i),alpha_b,4,NULL)==TG_OK);
    CHECK(!memcmp(tg_canvas_pixels(alpha_a),tg_canvas_pixels(alpha_b),100u*100u*4u));
    CHECK(tg_ui_paint(tg_interp_ui(i),alpha_a)==TG_OK&&tg_canvas_pixels(alpha_a)[0]==over);
    CHECK(tg_eval(i,".alpha configure -opacity 0; .overlay configure -backgroundalpha 0")==TG_OK);
    CHECK(tg_ui_paint(tg_interp_ui(i),alpha_a)==TG_OK&&tg_canvas_pixels(alpha_a)[0]==0x182536);
    CHECK(tg_eval(i,"destroy .alpha; destroy .overlay; frame .group -bg red -groupopacity 0.5; place .group -width 20 -height 20; frame .group.child -bg blue; place .group.child -x 0 -y 0 -width 10 -height 10")==TG_OK);
    CHECK(tg_ui_arrange(tg_interp_ui(i),100,100)==TG_OK);
    CHECK(tg_ui_paint(tg_interp_ui(i),alpha_a)==TG_OK);
    CHECK(tg_canvas_pixels(alpha_a)[0]==tg_pixel_blend(0x182536,0x0000ff80u));
    CHECK(tg_canvas_pixels(alpha_a)[15*100+15]==red);
    CHECK(tg_ui_paint_parallel(tg_interp_ui(i),alpha_b,4,NULL)==TG_OK);
    CHECK(!memcmp(tg_canvas_pixels(alpha_a),tg_canvas_pixels(alpha_b),100u*100u*4u));
    CHECK(tg_eval(i,".group configure -groupopacity 0")==TG_OK);
    CHECK(tg_ui_paint(tg_interp_ui(i),alpha_a)==TG_OK&&tg_canvas_pixels(alpha_a)[0]==0x182536);
    CHECK(tg_eval(i,"destroy .group; frame .rgba -bg #ff000080; place .rgba -width 20 -height 20")==TG_OK);
    CHECK(tg_eval(i,".rgba cget -bg")==TG_OK&&!strcmp(tg_result(i),"#ff000080"));
    CHECK(tg_ui_arrange(tg_interp_ui(i),100,100)==TG_OK);
    CHECK(tg_ui_paint(tg_interp_ui(i),alpha_a)==TG_OK&&tg_canvas_pixels(alpha_a)[0]==red);
    CHECK(tg_eval(i,".rgba configure -backgroundalpha 0.5")==TG_OK);
    CHECK(tg_ui_paint(tg_interp_ui(i),alpha_a)==TG_OK&&tg_canvas_pixels(alpha_a)[0]==tg_pixel_blend(0x182536,0xff000040u));
    CHECK(tg_eval(i,".rgba configure -bg #ff0000; .rgba configure -backgroundalpha 1")==TG_OK);
    CHECK(tg_ui_paint(tg_interp_ui(i),alpha_a)==TG_OK&&tg_canvas_pixels(alpha_a)[0]==0xff0000);
    CHECK(tg_eval(i,"frame .rel; place .rel -relx 0.5 -rely 0.5 -relwidth 0.5 -relheight 0.5 -anchor center")==TG_OK);
    CHECK(tg_ui_arrange(tg_interp_ui(i),100,100)==TG_OK);
    const tg_widget *rel=tg_ui_get(tg_interp_ui(i),tg_ui_find(tg_interp_ui(i),".rel"));
    CHECK(rel&&rel->rect.x==25&&rel->rect.y==25&&rel->rect.w==50&&rel->rect.h==50);
    CHECK(tg_ui_arrange(tg_interp_ui(i),200,100)==TG_OK&&rel->rect.x==50&&rel->rect.w==100);
    CHECK(tg_eval(i,"place configure .rel -width 10 -x -5")==TG_OK);
    CHECK(tg_ui_arrange(tg_interp_ui(i),200,100)==TG_OK&&rel->rect.x==40&&rel->rect.w==110);
    CHECK(tg_eval(i,"place info .rel")==TG_OK&&strstr(tg_result(i),"-relwidth 0.5")&&strstr(tg_result(i),"-anchor center"));
    CHECK(tg_eval(i,"place configure .rel -relwidth NaN")==TG_ERROR);
    tg_canvas_destroy(alpha_a); tg_canvas_destroy(alpha_b);
    tg_destroy(i);
    tg_ui *u=tg_ui_create(); CHECK(u);
    tg_id a=tg_ui_add(u,".panel",TG_FRAME,NULL), b=tg_ui_add(u,".panel.ok",TG_BUTTON,"OK");
    tg_id c=tg_ui_add(u,".side",TG_CANVAS,NULL);
    CHECK(a&&b&&c&&tg_ui_find(u,".panel.ok")==b);
    CHECK(tg_ui_get(u,1)->child_count==2&&tg_ui_get(u,a)->child_count==1);
    CHECK(!tg_ui_add(u,".missing.x",TG_LABEL,NULL));
    CHECK(!tg_ui_add(u,".side",TG_LABEL,NULL));
    CHECK(tg_ui_geometry(u,a,TG_PACK_TOP,(tg_rect){0,0,0,30})==TG_OK);
    CHECK(tg_ui_geometry(u,b,TG_PLACE,(tg_rect){5,5,20,20})==TG_OK);
    CHECK(tg_ui_geometry(u,c,TG_PACK_LEFT,(tg_rect){0,0,40,0})==TG_OK);
    CHECK(tg_ui_arrange(u,100,80)==TG_OK);
    CHECK(tg_ui_get(u,c)->rect.y==30&&tg_ui_get(u,c)->rect.h==50);
    CHECK(tg_ui_hit(u,10,10)==b&&tg_ui_hit(u,10,50)==c);
    tg_canvas *canvas=tg_canvas_create(100,80); CHECK(canvas);
    CHECK(tg_ui_paint(u,canvas)==TG_OK);
    CHECK(tg_canvas_pixels(canvas)[1010]==0x397bc4);
    tg_event e={TG_POINTER_DOWN,b,10,10,0}, out;
    CHECK(tg_ui_post(u,e)==TG_OK&&tg_ui_poll(u,&out)&&out.target==b);
    CHECK(tg_ui_post(u,e)==TG_OK);
    CHECK(tg_ui_remove(u,a)==TG_OK&&!tg_ui_get(u,b)&&!tg_ui_poll(u,&out));
    CHECK(tg_ui_get(u,1)->child_count==1&&tg_ui_get(u,c)->id==c);
    tg_id replacement=tg_ui_add(u,".new",TG_FRAME,NULL);
    CHECK(replacement>c&&tg_ui_get(u,replacement)->id==replacement&&tg_ui_get(u,1)->child_count==2);
    CHECK(tg_ui_remove(u,1)==TG_ERROR);
    tg_canvas_destroy(canvas); tg_ui_destroy(u); puts("widget checks passed"); return 0;
}
