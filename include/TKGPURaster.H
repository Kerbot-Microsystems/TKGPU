#ifndef TKGPU_RASTER_H
#define TKGPU_RASTER_H
#include "TKGPURuntime.H"
/* Pixel rectangles use half-open bounds. Line endpoints are inclusive.
   Drawing is limited to coordinate magnitude 65536. */
int tg_rect_intersect(tg_rect a,tg_rect b,tg_rect *out);
void tg_raster_fill(tg_canvas *c,tg_rect clip,tg_rect rect,uint32_t rgb);
void tg_raster_line(tg_canvas *c,tg_rect clip,int x0,int y0,int x1,int y1,uint32_t rgb);
void tg_raster_outline(tg_canvas *c,tg_rect clip,tg_rect rect,uint32_t rgb);
void tg_raster_ellipse(tg_canvas *c,tg_rect clip,tg_rect rect,uint32_t rgb,int filled);
/* Packed 0xRRGGBBAA over opaque 0xRRGGBB. */
uint32_t tg_pixel_blend(uint32_t rgb,uint32_t rgba);
void tg_canvas_fill_alpha(tg_canvas *c,tg_rect rect,uint32_t rgb,unsigned alpha);
/* Non-overlapping RGBA8 source, validated byte length and stride. */
int tg_raster_blit_rgba(tg_canvas *c,tg_rect clip,int x,int y,int w,int h,
                       const unsigned char *source,size_t stride,size_t length);
#endif
