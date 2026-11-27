#ifndef TKGPU_FRAMEBUFFER_INTERNAL_H
#define TKGPU_FRAMEBUFFER_INTERNAL_H
#include "TKGPUUi.H"
/* Internal row-band views borrow pixels; only the owner may resize/free. */
struct tg_paint_pool;
struct tg_canvas { int w,h; size_t cap; uint32_t *p; tg_rect dirty; int damaged; int y0,y1; int clip_active; tg_rect clip; int borrowed; struct tg_paint_pool *pool; tg_draw_rect_sink sink; tg_draw_image_sink image_sink; tg_draw_layer_sink layer_sink; uint32_t image_id; uint64_t image_revision; void *sink_context; int sink_failed; };
void tg_canvas_mark_damage(tg_canvas *c,int x,int y,int w,int h);
tg_rect tg_canvas_render_bounds(const tg_canvas *c);
void tg_canvas_merge_damage(tg_canvas *owner,const tg_canvas *band);
#endif
