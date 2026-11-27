#include "TKGPURaster.H"
#include <limits.h>
int tg_rect_intersect(tg_rect a,tg_rect b,tg_rect *out) {
    if(!out||a.w<=0||a.h<=0||b.w<=0||b.h<=0) return 0;
    int64_t l=a.x>b.x?a.x:b.x,t=a.y>b.y?a.y:b.y;
    int64_t ar=(int64_t)a.x+a.w,br=(int64_t)b.x+b.w;
    int64_t ab=(int64_t)a.y+a.h,bb=(int64_t)b.y+b.h;
    int64_t r=ar<br?ar:br,d=ab<bb?ab:bb;
    if(r<=l||d<=t||r-l>INT_MAX||d-t>INT_MAX) return 0;
    *out=(tg_rect){(int)l,(int)t,(int)(r-l),(int)(d-t)}; return 1;
}
void tg_raster_fill(tg_canvas *c,tg_rect clip,tg_rect rect,uint32_t rgb) {
    tg_rect q;
    if(c&&tg_rect_intersect(clip,rect,&q)) tg_canvas_fill(c,q,rgb);
}
