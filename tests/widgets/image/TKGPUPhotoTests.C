#include "TKGPUUi.H"
#include "TKGPUPhoto.H"
#include "TKGPURaster.H"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
static int replay(void *p,tg_rect r,uint32_t rgb,unsigned alpha) { tg_canvas_fill_alpha(p,r,rgb,alpha); return TG_OK; }
static int image(void *p,tg_rect r,const unsigned char *data,size_t stride,size_t length,uint32_t id,uint64_t revision,size_t offset) {
    (void)offset; if(!id||!revision) return TG_ERROR;
    return tg_raster_blit_rgba(p,r,r.x,r.y,r.w,r.h,data,stride,length);
}
int main(void) {
    tg_photo *p=tg_photo_create(2,2); CHECK(p);
    uint64_t rev=tg_photo_revision(p); CHECK(rev!=0);
    uint32_t rgba=1; CHECK(tg_photo_pixel(p,0,0,&rgba)==TG_OK&&rgba==0);
    CHECK(tg_photo_fill(p,(tg_rect){0,0,2,2},0xff000080)==TG_OK);
    CHECK(tg_photo_revision(p)==rev+1);
    CHECK(tg_photo_fill(p,(tg_rect){1,1,2,2},0)==TG_ERROR);
    CHECK(tg_photo_revision(p)==rev+1);
    tg_photo *clone=tg_photo_clone(p); CHECK(clone&&tg_photo_revision(clone)==rev+1); tg_photo_destroy(clone);
    tg_canvas *c=tg_canvas_create(20,20); CHECK(c);
    CHECK(tg_photo_paint(p,c,(tg_rect){0,0,20,20},0,0)==TG_OK);
    CHECK(tg_canvas_pixels(c)[0]==0x800000);
    tg_photo_destroy(p);
    tg_interp *i=tg_create(); CHECK(i); tg_ui *u=tg_interp_ui(i);
    CHECK(tg_eval(i,"image create photo pic -width 4 -height 4; pic put red -to 0 0 4 4")==TG_OK);
    CHECK(tg_eval(i,"pic get 1 1")==TG_OK&&!strcmp(tg_result(i),"255 0 0"));
    CHECK(tg_eval(i,"pic put blue -to 3 3 5 5")==TG_ERROR);
    CHECK(tg_eval(i,"canvas .c; place .c -width 20 -height 20; .c create image 2 3 -image pic -anchor nw")==TG_OK);
    CHECK(tg_ui_arrange(u,20,20)==TG_OK&&tg_ui_paint(u,c)==TG_OK);
    CHECK(tg_canvas_pixels(c)[3*20+2]==0xff0000);
    tg_canvas *recorded=tg_canvas_create(20,20); CHECK(recorded);
    CHECK(tg_ui_record_rects(u,20,20,replay,recorded)==TG_ERROR);
    CHECK(tg_ui_record_draws(u,20,20,replay,image,recorded)==TG_OK);
    CHECK(!memcmp(tg_canvas_pixels(c),tg_canvas_pixels(recorded),20u*20u*4u));
    CHECK(tg_eval(i,".c create image 18 18 -image pic -anchor nw; .c create rectangle 18 18 19 19 -fill blue -outline {}")==TG_OK);
    CHECK(tg_ui_paint(u,c)==TG_OK&&tg_ui_record_draws(u,20,20,replay,image,recorded)==TG_OK);
    CHECK(!memcmp(tg_canvas_pixels(c),tg_canvas_pixels(recorded),20u*20u*4u));
    tg_canvas_destroy(recorded);
    CHECK(tg_eval(i,"pic blank")==TG_OK&&tg_ui_paint(u,c)==TG_OK);
    CHECK(tg_canvas_pixels(c)[3*20+2]==0x101b2b);
    CHECK(tg_eval(i,"pic put blue -to 0 0; image delete pic; image create photo pic -width 4 -height 4; pic put red -to 0 0")==TG_OK);
    CHECK(tg_ui_paint(u,c)==TG_OK&&tg_canvas_pixels(c)[3*20+2]==0x101b2b);
    tg_destroy(i); tg_canvas_destroy(c); puts("photo and canvas integration passed"); return 0;
}
