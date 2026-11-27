#include "../../src/render/parallel/TKGPUParallelPaint.H"
#include "TKGPURaster.H"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
typedef struct { int w,h; unsigned char *rgba; } scene;
static int paint(const void *p,tg_canvas *c) {
    const scene *s=p; tg_rect clip={0,0,s->w,s->h};
    tg_canvas_fill(c,(tg_rect){3,2,s->w-6,s->h-4},0x123456);
    tg_raster_ellipse(c,clip,(tg_rect){4,3,s->w-8,s->h-6},0x9900ff,1);
    if(tg_raster_blit_rgba(c,clip,0,0,s->w,s->h,s->rgba,(size_t)s->w*4,(size_t)s->w*s->h*4)!=TG_OK) return TG_ERROR;
    tg_raster_line(c,clip,5,5,s->w-5,s->h-5,0xffffff);
    return TG_OK;
}
int main(void) {
    scene s={641,481,NULL}; size_t count=(size_t)s.w*s.h;
    s.rgba=malloc(count*4); CHECK(s.rgba);
    for(size_t k=0;k<count*4;k++) s.rgba[k]=(unsigned char)(k*31u+7u);
    tg_canvas *a=tg_canvas_create(s.w,s.h),*b=tg_canvas_create(s.w,s.h); CHECK(a&&b);
    tg_rect da,db; unsigned used=0;
    for(unsigned n=1;n<=64;n*=2) {
        tg_canvas_clear(a,0); tg_canvas_clear(b,0); tg_canvas_damage(a,&da); tg_canvas_damage(b,&db);
        CHECK(tg_canvas_parallel_paint(a,paint,&s,1,NULL)==TG_OK);
        CHECK(tg_canvas_parallel_paint(b,paint,&s,n,&used)==TG_OK&&used>=1&&used<=n);
        CHECK(!memcmp(tg_canvas_pixels(a),tg_canvas_pixels(b),count*sizeof(uint32_t)));
        CHECK(tg_canvas_damage(a,&da)&&tg_canvas_damage(b,&db));
        CHECK(da.x==db.x&&da.y==db.y&&da.w==db.w&&da.h==db.h);
        printf("requested %u actual %u: identical pixels and damage\n",n,used);
    }
    CHECK(tg_canvas_parallel_capacity(b)>=used);
    unsigned capacity=tg_canvas_parallel_capacity(b);
    for(int k=0;k<30;k++) {
        tg_canvas_clear(b,0);
        CHECK(tg_canvas_parallel_paint(b,paint,&s,capacity,&used)==TG_OK&&used==capacity);
        CHECK(tg_canvas_parallel_capacity(b)==capacity);
        CHECK(!memcmp(tg_canvas_pixels(a),tg_canvas_pixels(b),count*sizeof(uint32_t)));
    }
    tg_canvas_parallel_release(b); CHECK(tg_canvas_parallel_capacity(b)==1);
    tg_canvas_destroy(a); tg_canvas_destroy(b); free(s.rgba); return 0;
}
