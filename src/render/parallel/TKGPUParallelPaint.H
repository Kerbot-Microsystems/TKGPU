#ifndef TKGPU_PARALLEL_PAINT_H
#define TKGPU_PARALLEL_PAINT_H
#include "TKGPURuntime.H"
/* Callback reads immutable scene data and draws only through band-aware raster APIs.
   It must not resize/destroy the canvas or mutate shared context. Synchronous join. */
typedef int (*tg_band_paint)(const void *,tg_canvas *);
int tg_canvas_parallel_paint(tg_canvas *,tg_band_paint,const void *,unsigned,unsigned *);
unsigned tg_canvas_parallel_capacity(const tg_canvas *);
void tg_canvas_parallel_release(tg_canvas *);
#endif
