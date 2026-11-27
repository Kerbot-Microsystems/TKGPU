#include "TKGPURaster.H"
#include <stdio.h>
#include <limits.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(void) {
    tg_canvas *c=tg_canvas_create(16,16); CHECK(c);
    tg_rect clip={2,2,12,12},q;
    CHECK(!tg_rect_intersect((tg_rect){INT_MAX,0,10,10},clip,&q));
    CHECK(tg_rect_intersect((tg_rect){-5,-5,10,10},clip,&q)&&q.x==2&&q.y==2&&q.w==3&&q.h==3);
    tg_canvas_clear(c,0);
    tg_raster_line(c,clip,-10,-10,30,30,0xffffff);
    const uint32_t *p=tg_canvas_pixels(c);
    for(int y=0;y<16;y++) for(int x=0;x<16;x++)
        CHECK(p[y*16+x]==(x==y&&x>=2&&x<14?0xffffffu:0));
    tg_canvas_clear(c,0);
    tg_raster_outline(c,clip,(tg_rect){4,4,8,8},0x123456);
    CHECK(p[4*16+4]==0x123456&&p[5*16+5]==0&&p[11*16+11]==0x123456);
    tg_canvas_clear(c,0);
    tg_raster_ellipse(c,clip,(tg_rect){2,2,12,12},0xff,1);
    CHECK(p[8*16+8]==0xff&&p[2*16+2]==0&&p[2*16+7]==0xff);
    tg_canvas_clear(c,0);
    tg_raster_ellipse(c,clip,(tg_rect){2,2,12,12},0xff,0);
    CHECK(p[8*16+8]==0&&p[2*16+7]==0xff);
    tg_canvas_destroy(c); puts("raster clipping and primitives passed"); return 0;
}
