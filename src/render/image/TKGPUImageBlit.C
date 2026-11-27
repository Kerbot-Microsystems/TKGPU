#include "TKGPURaster.H"
#include "TKGPUChecked.H"
#include "../TKGPUFrameBufferInternal.H"
/* Source is counted, non-overlapping RGBA8, row stride in bytes. */
int tg_raster_blit_rgba(tg_canvas *c,tg_rect clip,int x,int y,int w,int h,
                       const unsigned char *source,size_t stride,size_t length) {
    if(!c||!source||w<=0||h<=0||w>16384||h>16384) return TG_ERROR;
    size_t row,last,needed;
    if(tg_size_mul((size_t)w,4,&row)!=TG_OK||stride<row||
       tg_size_mul((size_t)(h-1),stride,&last)!=TG_OK||
       tg_size_add(last,row,&needed)!=TG_OK||needed>length) return TG_ERROR;
    tg_rect q;
    if(!tg_rect_intersect((tg_rect){x,y,w,h},clip,&q)||
       !tg_rect_intersect(q,tg_canvas_render_bounds(c),&q)) return TG_OK;
    if(c->sink) {
        size_t offset=(size_t)((int64_t)q.y-y)*stride+4*(size_t)((int64_t)q.x-x);
        if(!c->image_sink||c->sink_failed||
           c->image_sink(c->sink_context,q,source+offset,stride,length-offset,c->image_id,c->image_revision,offset)!=TG_OK) {
            c->sink_failed=1; return TG_ERROR;
        }
        return TG_OK;
    }
    uint32_t *pixels=c->p;
    int cw=tg_canvas_width(c);
    int l=q.x+q.w,r=q.x,t=q.y+q.h,b=q.y;
    for(int j=q.y;j<q.y+q.h;j++) {
        const unsigned char *p=source+(size_t)((int64_t)j-y)*stride+4*(size_t)((int64_t)q.x-x);
        for(int k=q.x;k<q.x+q.w;k++,p+=4) {
            if(!p[3]) continue;
            uint32_t rgba=((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
            uint32_t *dst=&pixels[(size_t)j*cw+k];
            *dst=p[3]==255?rgba>>8:tg_pixel_blend(*dst,rgba);
            if(k<l) l=k;
            if(k>=r) r=k+1;
            if(j<t) t=j;
            b=j+1;
        }
    }
    if(r>l&&b>t) tg_canvas_mark_damage(c,l,t,r-l,b-t);
    return TG_OK;
}
