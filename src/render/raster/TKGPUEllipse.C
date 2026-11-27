#include "TKGPURaster.H"
#include "../TKGPUFrameBufferInternal.H"
/* Pixel-center inclusion, using integer arithmetic and bounded dimensions. */
static int inside(int x,int y,int w,int h) {
    int64_t dx=2*(int64_t)x+1-w,dy=2*(int64_t)y+1-h;
    int64_t ww=(int64_t)w*w,hh=(int64_t)h*h;
    return dx*dx*hh+dy*dy*ww<=ww*hh;
}
void tg_raster_ellipse(tg_canvas *c,tg_rect clip,tg_rect r,uint32_t rgb,int filled) {
    tg_rect q;
    if(!c||r.w<=0||r.h<=0||r.w>32768||r.h>32768||
       !tg_rect_intersect(r,clip,&q)||
       !tg_rect_intersect(q,tg_canvas_render_bounds(c),&q)) return;
    for(int y=q.y;y<q.y+q.h;y++) {
        int row=y-r.y,lo=0,hi=(r.w-1)/2;
        if(!inside(hi,row,r.w,r.h)) continue;
        while(lo<hi) {
            int mid=lo+(hi-lo)/2;
            if(inside(mid,row,r.w,r.h)) hi=mid; else lo=mid+1;
        }
        int right=r.w-1-lo;
        if(filled) tg_raster_fill(c,q,(tg_rect){r.x+lo,y,right-lo+1,1},rgb);
        else {
            /* Keep pixels with at least one 4-connected exterior neighbor. */
            int inner_l=lo+1,inner_r=right-1;
            if(row==0||row==r.h-1) inner_l=right+1;
            else {
                while(inner_l<=inner_r&&(!inside(inner_l,row-1,r.w,r.h)||!inside(inner_l,row+1,r.w,r.h))) inner_l++;
                inner_r=r.w-1-inner_l;
                if(inner_r>right-1) inner_r=right-1;
            }
            if(inner_l>inner_r) tg_raster_fill(c,q,(tg_rect){r.x+lo,y,right-lo+1,1},rgb);
            else {
                tg_raster_fill(c,q,(tg_rect){r.x+lo,y,inner_l-lo,1},rgb);
                tg_raster_fill(c,q,(tg_rect){r.x+inner_r+1,y,right-inner_r,1},rgb);
            }
        }
    }
}
