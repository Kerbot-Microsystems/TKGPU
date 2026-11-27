#include "TKGPUUi.H"
#include "TKGPURaster.H"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
static int replay(void *p,tg_rect r,uint32_t rgb,unsigned alpha) {
    tg_canvas_fill_alpha(p,r,rgb,alpha); return TG_OK;
}
static int reject(void *p,tg_rect r,uint32_t rgb,unsigned alpha) {
    (void)p;(void)r;(void)rgb;(void)alpha; return TG_ERROR;
}
int main(void) {
    tg_interp *i=tg_create(); CHECK(i);
    tg_ui *u=tg_interp_ui(i);
    CHECK(tg_eval(i,"canvas .c; place .c -x 5 -y 5 -width 20 -height 20; .c create rectangle -5 -5 10 10 -fill red -outline {}")==TG_OK);
    CHECK(!strcmp(tg_result(i),"1"));
    CHECK(tg_eval(i,".c create line 0 0 19 19 -fill white")==TG_OK);
    CHECK(tg_eval(i,".c create oval 10 0 20 10 -fill blue -outline {}")==TG_OK);
    CHECK(tg_eval(i,".c create polygon 0 0 2 2")==TG_ERROR);
    CHECK(tg_ui_arrange(u,40,40)==TG_OK);
    tg_canvas *c=tg_canvas_create(40,40); CHECK(c);
    CHECK(tg_ui_paint(u,c)==TG_OK);
    const uint32_t *p=tg_canvas_pixels(c);
    CHECK(p[5*40+4]==0x182536&&p[6*40+7]==0xff0000);
    CHECK(p[5*40+5]==0xffffff&&p[9*40+19]==0x0000ff);
    tg_canvas *recorded=tg_canvas_create(40,40); CHECK(recorded);
    CHECK(tg_ui_record_rects(u,40,40,replay,recorded)==TG_OK);
    CHECK(!memcmp(p,tg_canvas_pixels(recorded),40u*40u*4u));
    CHECK(tg_ui_record_rects(u,40,40,reject,NULL)==TG_ERROR);
    CHECK(tg_ui_record_rects(u,41,40,replay,recorded)==TG_ERROR);
    CHECK(tg_eval(i,".c configure -backgroundalpha 0.5")==TG_OK);
    CHECK(tg_ui_paint(u,c)==TG_OK&&tg_ui_record_rects(u,40,40,replay,recorded)==TG_OK);
    CHECK(!memcmp(p,tg_canvas_pixels(recorded),40u*40u*4u));
    CHECK(tg_eval(i,".c configure -backgroundalpha 1")==TG_OK);
    tg_canvas_destroy(recorded);
    CHECK(tg_eval(i,".c delete 1")==TG_OK&&tg_ui_paint(u,c)==TG_OK);
    CHECK(p[6*40+7]==0x101b2b);
    CHECK(tg_eval(i,".c delete all")==TG_OK&&tg_ui_paint(u,c)==TG_OK);
    CHECK(p[5*40+5]==0x101b2b);
    CHECK(tg_eval(i,".c create rectangle 0 0 20 20 -fill red; destroy .c; canvas .c; place .c -width 20 -height 20")==TG_OK);
    CHECK(tg_ui_arrange(u,40,40)==TG_OK&&tg_ui_paint(u,c)==TG_OK);
    CHECK(p[0]==0x101b2b);
    CHECK(tg_eval(i,"set item [.c create rectangle 0 0 10 10 -fill red -outline red -opacity 0.5]")==TG_OK);
    CHECK(tg_eval(i,".c create rectangle 2 2 8 8 -fill blue -outline {} -opacity 0.5")==TG_OK);
    CHECK(tg_ui_paint(u,c)==TG_OK);
    CHECK(tg_ui_record_rects(u,40,40,reject,NULL)==TG_ERROR);
    uint32_t red=tg_pixel_blend(0x101b2b,0xff000080);
    CHECK(p[0]==red&&p[41]==red&&p[3*40+3]==tg_pixel_blend(red,0x0000ff80));
    CHECK(tg_eval(i,".c itemconfigure $item -opacity -1")==TG_ERROR);
    CHECK(tg_eval(i,".c itemcget $item -opacity")==TG_OK&&!strcmp(tg_result(i),"0.501960784"));
    CHECK(tg_eval(i,".c itemconfigure $item -opacity 0")==TG_OK);
    CHECK(tg_ui_paint(u,c)==TG_OK&&p[0]==0x101b2b);
    tg_canvas *parallel=tg_canvas_create(40,40); CHECK(parallel);
    CHECK(tg_ui_paint_parallel(u,parallel,4,NULL)==TG_OK);
    CHECK(!memcmp(p,tg_canvas_pixels(parallel),40u*40u*4u));
    CHECK(tg_eval(i,".c delete all; set line [.c create line 2.4 5.6 9 12 17.5 3.5 -fill white -width 3 -capstyle round -joinstyle round]")==TG_OK);
    CHECK(tg_eval(i,".c coords $line")==TG_OK&&!strcmp(tg_result(i),"2 6 9 12 18 4"));
    CHECK(tg_eval(i,".c itemconfigure $line -width 6 -fill blue")==TG_OK);
    CHECK(tg_eval(i,".c itemcget $line -width")==TG_OK&&!strcmp(tg_result(i),"6"));
    CHECK(tg_eval(i,".c coords $line 2 10 17 10")==TG_OK);
    CHECK(tg_ui_paint(u,c)==TG_OK&&p[8*40+10]==0x0000ff);
    CHECK(tg_ui_paint_parallel(u,parallel,4,NULL)==TG_OK);
    CHECK(!memcmp(p,tg_canvas_pixels(parallel),40u*40u*4u));
    CHECK(tg_eval(i,".c delete all; set text [.c create text 3 3 -text HI -font {{IBM Plex Sans} 8 bold} -fill white -anchor nw]")==TG_OK);
    CHECK(tg_eval(i,".c itemcget $text -text")==TG_OK&&!strcmp(tg_result(i),"HI"));
    CHECK(tg_ui_paint(u,c)==TG_OK);
    unsigned lit=0;
    for(int y=3;y<10;y++) for(int x=3;x<14;x++) if(p[y*40+x]==0xffffff) lit++;
    CHECK(lit>0);
    CHECK(tg_eval(i,".c coords $text 6 6; .c itemconfigure $text -text OK -fill blue")==TG_OK);
    CHECK(tg_ui_paint(u,c)==TG_OK&&tg_ui_paint_parallel(u,parallel,4,NULL)==TG_OK);
    CHECK(!memcmp(p,tg_canvas_pixels(parallel),40u*40u*4u));
    CHECK(tg_eval(i,".c delete all; set a [.c create rectangle 1 2 5 6 -fill red -tags {node shared}]; set b [.c create rectangle 7 8 10 12 -fill blue -tags shared]")==TG_OK);
    CHECK(tg_eval(i,".c bbox shared")==TG_OK&&!strcmp(tg_result(i),"1 2 10 12"));
    CHECK(tg_eval(i,"canvas .other; .other create rectangle 0 0 1 1; set c [.c create rectangle 15 15 16 16 -tags third]")==TG_OK);
    char neighbor[32];
    CHECK(tg_eval(i,"set c")==TG_OK);
    snprintf(neighbor,sizeof neighbor,"%s",tg_result(i));
    CHECK(tg_eval(i,".c find above shared")==TG_OK&&!strcmp(tg_result(i),neighbor));
    CHECK(tg_eval(i,"set b")==TG_OK);
    snprintf(neighbor,sizeof neighbor,"%s",tg_result(i));
    CHECK(tg_eval(i,".c find below $c")==TG_OK&&!strcmp(tg_result(i),neighbor));
    CHECK(tg_eval(i,".c find below shared")==TG_OK&&!strcmp(tg_result(i),""));
    CHECK(tg_eval(i,".c find above missing")==TG_OK&&!strcmp(tg_result(i),""));
    CHECK(tg_eval(i,".c addtag above_shared above shared; .c gettags $c")==TG_OK&&!strcmp(tg_result(i),"third above_shared"));
    CHECK(tg_eval(i,".c addtag below_third below $c; .c gettags $b")==TG_OK&&!strcmp(tg_result(i),"shared below_third"));
    CHECK(tg_eval(i,".c addtag global all; .c gettags $a")==TG_OK&&!strcmp(tg_result(i),"node shared global"));
    CHECK(tg_eval(i,".other find withtag global")==TG_OK&&!strcmp(tg_result(i),""));
    CHECK(tg_eval(i,".c dtag all global; .c dtag $b below_third; .c delete $c; destroy .other")==TG_OK);
    CHECK(tg_eval(i,".c move shared 2 3; .c bbox all")==TG_OK&&!strcmp(tg_result(i),"3 5 12 15"));
    CHECK(tg_eval(i,".c itemconfigure shared -fill white; .c itemcget $b -fill")==TG_OK&&!strcmp(tg_result(i),"#ffffff"));
    CHECK(tg_eval(i,".c addtag selected withtag shared; .c dtag node shared; .c gettags $a")==TG_OK&&!strcmp(tg_result(i),"node selected"));
    CHECK(tg_eval(i,".c find withtag shared; set b")==TG_OK);
    char expected[32]; snprintf(expected,sizeof expected,"%s",tg_result(i));
    CHECK(tg_eval(i,".c find withtag shared")==TG_OK&&!strcmp(tg_result(i),expected));
    CHECK(tg_eval(i,".c delete selected; .c find all")==TG_OK&&!strcmp(tg_result(i),""));
    CHECK(tg_eval(i,".c configure -scrollregion {0 0 100 100} -xscrollincrement 5; .c create rectangle 30 30 40 40 -fill red -outline {}; .c xview moveto 0.3; .c yview moveto 0.3")==TG_OK);
    CHECK(tg_eval(i,".c xview")==TG_OK&&!strcmp(tg_result(i),"0.3 0.5"));
    CHECK(tg_eval(i,".c canvasx 5")==TG_OK&&!strcmp(tg_result(i),"35"));
    CHECK(tg_ui_paint(u,c)==TG_OK&&p[0]==0xff0000);
    CHECK(tg_eval(i,".c xview scroll 1 units; .c canvasx 0")==TG_OK&&!strcmp(tg_result(i),"35"));
    CHECK(tg_eval(i,".c bbox all")==TG_OK&&!strcmp(tg_result(i),"30 30 40 40"));
    CHECK(tg_eval(i,".c delete all; .c configure -scrollregion {0 0 100 100}; .c xview moveto 0; .c yview moveto 0; frame .c.inner -bg red; set win [.c create window 3 4 -window .c.inner -anchor nw -width 10 -height 12]")==TG_OK);
    CHECK(tg_ui_arrange(u,40,40)==TG_OK);
    CHECK(tg_eval(i,"winfo manager .c.inner")==TG_OK&&!strcmp(tg_result(i),"canvas"));
    CHECK(tg_eval(i,".c bbox $win")==TG_OK&&!strcmp(tg_result(i),"3 4 13 16"));
    CHECK(tg_ui_paint(u,c)==TG_OK&&p[4*40+3]==0xff0000);
    CHECK(tg_eval(i,".c itemconfigure $win -width 15; .c bbox $win")==TG_OK&&!strcmp(tg_result(i),"3 4 18 16"));
    CHECK(tg_eval(i,".c xview moveto 0.05")==TG_OK&&tg_ui_arrange(u,40,40)==TG_OK);
    CHECK(tg_ui_paint(u,c)==TG_OK&&p[4*40]==0xff0000);
    CHECK(tg_eval(i,".c delete $win; winfo manager .c.inner")==TG_OK&&!strcmp(tg_result(i),""));
    /* Startup gradient is repainted after foreground items are created. */
    CHECK(tg_eval(i,".c delete all; .c xview moveto 0; set track [.c create line 2 10 18 10 -fill red -width 6 -capstyle round]; set gradient [.c create rectangle 0 0 20 20 -fill blue -outline {}]")==TG_OK);
    CHECK(tg_ui_paint(u,c)==TG_OK&&p[10*40+10]==0x0000ff);
    CHECK(tg_eval(i,".c raise $track")==TG_OK);
    CHECK(tg_ui_paint(u,c)==TG_OK&&p[10*40+10]==0xff0000);
    CHECK(tg_eval(i,".c lower $track $gradient")==TG_OK);
    CHECK(tg_ui_paint(u,c)==TG_OK&&p[10*40+10]==0x0000ff);
    CHECK(tg_eval(i,".c raise $track $gradient")==TG_OK);
    CHECK(tg_ui_paint(u,c)==TG_OK&&p[10*40+10]==0xff0000);
    CHECK(tg_eval(i,".c scale $track 0 0 2 0.5; .c coords $track")==TG_OK&&!strcmp(tg_result(i),"4 5 36 5"));
    CHECK(tg_eval(i,".c scale $track 0 0 NaN 1")==TG_ERROR);
    CHECK(tg_eval(i,".c coords $track")==TG_OK&&!strcmp(tg_result(i),"4 5 36 5"));
    CHECK(tg_eval(i,".c configure -scrollregion {0 0 100 100}; .c scan mark 10 10; .c scan dragto 5 7 2; .c canvasx 0")==TG_OK&&!strcmp(tg_result(i),"10"));
    CHECK(tg_eval(i,".c canvasy 0")==TG_OK&&!strcmp(tg_result(i),"6"));
    tg_canvas_destroy(parallel);
    tg_canvas_destroy(c); tg_destroy(i); puts("retained canvas item checks passed"); return 0;
}
