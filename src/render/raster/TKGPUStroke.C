#include "TKGPURaster.H"
static int bounded(int v) { return v>=-65536&&v<=65536; }
void tg_raster_line(tg_canvas *c,tg_rect clip,int x0,int y0,int x1,int y1,uint32_t rgb) {
    tg_rect q;
    if(!c||!bounded(x0)||!bounded(y0)||!bounded(x1)||!bounded(y1)||
       !tg_rect_intersect(clip,(tg_rect){0,0,tg_canvas_width(c),tg_canvas_height(c)},&q)) return;
    int dx=x1>x0?x1-x0:x0-x1,dy=y1>y0?y0-y1:y1-y0;
    int sx=x0<x1?1:-1,sy=y0<y1?1:-1,err=dx+dy;
    if((x0<q.x&&x1<q.x)||(y0<q.y&&y1<q.y)||
       (x0>=q.x+q.w&&x1>=q.x+q.w)||(y0>=q.y+q.h&&y1>=q.y+q.h)) return;
    for(;;) {
        if(x0>=q.x&&x0<q.x+q.w&&y0>=q.y&&y0<q.y+q.h)
            tg_canvas_fill(c,(tg_rect){x0,y0,1,1},rgb);
        if(x0==x1&&y0==y1) break;
        int e=2*err;
        if(e>=dy) { err+=dy; x0+=sx; }
        if(e<=dx) { err+=dx; y0+=sy; }
    }
}
void tg_raster_outline(tg_canvas *c,tg_rect clip,tg_rect r,uint32_t rgb) {
    if(r.w<=0||r.h<=0||!bounded(r.x)||!bounded(r.y)||r.w>65536||r.h>65536) return;
    tg_raster_fill(c,clip,(tg_rect){r.x,r.y,r.w,1},rgb);
    tg_raster_fill(c,clip,(tg_rect){r.x,r.y+r.h-1,r.w,1},rgb);
    tg_raster_fill(c,clip,(tg_rect){r.x,r.y,1,r.h},rgb);
    tg_raster_fill(c,clip,(tg_rect){r.x+r.w-1,r.y,1,r.h},rgb);
}
