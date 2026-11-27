#include "TKGPUFrameBufferInternal.H"
#include "TKGPURaster.H"
#include "parallel/TKGPUParallelPaint.H"
#include <stdlib.h>
#include <limits.h>
/* SSE2 is baseline on x86-64; other targets retain the portable C path.
   Do not enable a newer ISA globally: legacy binaries must remain runnable. */
#if !defined(TKGPU_RASTER_SCALAR) && (defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
#include <emmintrin.h>
#define TG_RASTER_SSE2 1
#endif

static void tg_fill_span(uint32_t *p,size_t n,uint32_t rgb) {
#ifdef TG_RASTER_SSE2
    const __m128i v=_mm_set1_epi32((int)rgb);
    while(n>=16) {
        _mm_storeu_si128((__m128i *)(void *)(p+0),v);
        _mm_storeu_si128((__m128i *)(void *)(p+4),v);
        _mm_storeu_si128((__m128i *)(void *)(p+8),v);
        _mm_storeu_si128((__m128i *)(void *)(p+12),v);
        p+=16; n-=16;
    }
    while(n>=4) { _mm_storeu_si128((__m128i *)(void *)p,v); p+=4; n-=4; }
#endif
    while(n--) *p++=rgb;
}

/* Exact round-to-nearest division by 255, for channel sums <= 65025. */
static unsigned tg_blend_div255(unsigned x) {
    x+=128u; return (x+(x>>8))>>8;
}
static void tg_blend_span(uint32_t *p,size_t n,uint32_t rgb,unsigned alpha) {
    const unsigned inv=255u-alpha;
    const unsigned sr=((rgb>>16)&255u)*alpha;
    const unsigned sg=((rgb>>8)&255u)*alpha;
    const unsigned sb=(rgb&255u)*alpha;
#ifdef TG_RASTER_SSE2
    const __m128i z=_mm_setzero_si128(), k=_mm_set1_epi16((short)inv);
    const __m128i bias=_mm_setr_epi16((short)(sb+128u),(short)(sg+128u),(short)(sr+128u),128,
                                     (short)(sb+128u),(short)(sg+128u),(short)(sr+128u),128);
    const __m128i mask=_mm_set1_epi32(0xffffff);
    while(n>=4) {
        __m128i d=_mm_loadu_si128((const __m128i *)(const void *)p);
        __m128i lo=_mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(d,z),k),bias);
        __m128i hi=_mm_add_epi16(_mm_mullo_epi16(_mm_unpackhi_epi8(d,z),k),bias);
        lo=_mm_srli_epi16(_mm_add_epi16(lo,_mm_srli_epi16(lo,8)),8);
        hi=_mm_srli_epi16(_mm_add_epi16(hi,_mm_srli_epi16(hi,8)),8);
        _mm_storeu_si128((__m128i *)(void *)p,_mm_and_si128(_mm_packus_epi16(lo,hi),mask));
        p+=4; n-=4;
    }
#endif
    while(n--) {
        uint32_t d=*p;
        *p++=(tg_blend_div255(((d>>16)&255u)*inv+sr)<<16)|
             (tg_blend_div255(((d>>8)&255u)*inv+sg)<<8)|
              tg_blend_div255((d&255u)*inv+sb);
    }
}
/* Build the AVX2 path separately, and select it only on capable CPUs.
   MSVC and non-x86 compilers use the SSE2 or portable path above. */
#if !defined(TKGPU_RASTER_SCALAR) && !defined(TKGPU_RASTER_NO_AVX2) && (defined(__GNUC__) || defined(__clang__)) && (defined(__i386__) || defined(__x86_64__)) && !defined(_MSC_VER)
#include <immintrin.h>
#define TG_RASTER_AVX2 1
__attribute__((target("avx2")))
static void tg_blend_rect_avx2(uint32_t *p,size_t stride,size_t width,size_t rows,uint32_t rgb,unsigned alpha) {
    const unsigned inv=255u-alpha;
    const unsigned sr=((rgb>>16)&255u)*alpha,sg=((rgb>>8)&255u)*alpha,sb=(rgb&255u)*alpha;
    const __m256i z=_mm256_setzero_si256(), k=_mm256_set1_epi16((short)inv);
    const __m256i bias=_mm256_setr_epi16((short)(sb+128u),(short)(sg+128u),(short)(sr+128u),128,
        (short)(sb+128u),(short)(sg+128u),(short)(sr+128u),128,
        (short)(sb+128u),(short)(sg+128u),(short)(sr+128u),128,
        (short)(sb+128u),(short)(sg+128u),(short)(sr+128u),128);
    const __m256i mask=_mm256_set1_epi32(0xffffff);
    for(size_t y=0;y<rows;y++) {
        uint32_t *row=p+y*stride;
        size_t x=0;
        for(;width-x>=8;x+=8) {
            __m256i d=_mm256_loadu_si256((const __m256i *)(const void *)(row+x));
            __m256i lo=_mm256_add_epi16(_mm256_mullo_epi16(_mm256_unpacklo_epi8(d,z),k),bias);
            __m256i hi=_mm256_add_epi16(_mm256_mullo_epi16(_mm256_unpackhi_epi8(d,z),k),bias);
            lo=_mm256_srli_epi16(_mm256_add_epi16(lo,_mm256_srli_epi16(lo,8)),8);
            hi=_mm256_srli_epi16(_mm256_add_epi16(hi,_mm256_srli_epi16(hi,8)),8);
            _mm256_storeu_si256((__m256i *)(void *)(row+x),_mm256_and_si256(_mm256_packus_epi16(lo,hi),mask));
        }
        for(;x<width;x++) {
            uint32_t d=row[x];
            row[x]=(tg_blend_div255(((d>>16)&255u)*inv+sr)<<16)|
                 (tg_blend_div255(((d>>8)&255u)*inv+sg)<<8)|
                  tg_blend_div255((d&255u)*inv+sb);
        }
    }
}
#endif
void tg_canvas_mark_damage(tg_canvas *c,int x,int y,int w,int h) {
    if(!c->damaged) { c->dirty=(tg_rect){x,y,w,h}; c->damaged=1; return; }
    int l=x<c->dirty.x?x:c->dirty.x, t=y<c->dirty.y?y:c->dirty.y;
    int r=x+w>c->dirty.x+c->dirty.w?x+w:c->dirty.x+c->dirty.w;
    int b=y+h>c->dirty.y+c->dirty.h?y+h:c->dirty.y+c->dirty.h;
    c->dirty=(tg_rect){l,t,r-l,b-t};
}
int tg_canvas_resize(tg_canvas *c,int w,int h) {
    if(!c || c->borrowed || w<=0 || h<=0 || w>16384 || h>16384 ||
       (size_t)w>SIZE_MAX/sizeof(uint32_t)/(size_t)h) return TG_ERROR;
    uint32_t *p=calloc((size_t)w*(size_t)h,sizeof(*p));
    if(!p) return TG_ERROR;
    free(c->p); c->p=p; c->w=w; c->h=h; c->damaged=0; c->y0=0; c->y1=h;
    tg_canvas_mark_damage(c,0,0,w,h); return TG_OK;
}
tg_canvas *tg_canvas_create(int w,int h) {
    tg_canvas *c=calloc(1,sizeof(*c));
    if(!c || tg_canvas_resize(c,w,h)) { free(c); return NULL; } return c;
}
void tg_canvas_destroy(tg_canvas *c) { if(c&&!c->borrowed) { tg_canvas_parallel_release(c); free(c->p); free(c); } }
void tg_canvas_fill(tg_canvas *c,tg_rect q,uint32_t rgb) {
    if(!c || q.w<=0 || q.h<=0) return;
    tg_rect visible;
    if(!tg_rect_intersect(q,tg_canvas_render_bounds(c),&visible)) return;
    int x=visible.x,y=visible.y,r=x+visible.w,b=y+visible.h;
    if(c->sink) {
        if(!c->sink_failed&&c->sink(c->sink_context,(tg_rect){x,y,(int)r-x,(int)b-y},rgb&0xffffffu,255)!=TG_OK) c->sink_failed=1;
        return;
    }
    rgb&=0xffffffu;
    if(x==0 && r==c->w) tg_fill_span(c->p+(size_t)y*c->w,(size_t)(b-y)*c->w,rgb);
    else for(int j=y;j<b;j++) tg_fill_span(c->p+(size_t)j*c->w+x,(size_t)(r-x),rgb);
    tg_canvas_mark_damage(c,x,y,(int)r-x,(int)b-y);
}
void tg_canvas_fill_alpha(tg_canvas *c,tg_rect q,uint32_t rgb,unsigned alpha) {
    if(alpha>=255) { tg_canvas_fill(c,q,rgb); return; }
    if(!c||!alpha) return;
    tg_rect r;
    if(!tg_rect_intersect(q,tg_canvas_render_bounds(c),&r)) return;
    if(c->sink) {
        if(!c->sink_failed&&c->sink(c->sink_context,r,rgb&0xffffffu,alpha)!=TG_OK) c->sink_failed=1;
        return;
    }
#ifdef TG_RASTER_AVX2
    if(r.w>=16 && __builtin_cpu_supports("avx2")) {
        tg_blend_rect_avx2(c->p+(size_t)r.y*c->w+r.x,(size_t)c->w,(size_t)r.w,(size_t)r.h,rgb,alpha);
    } else
#endif
    if(r.x==0 && r.w==c->w)
        tg_blend_span(c->p+(size_t)r.y*c->w,(size_t)r.h*c->w,rgb,alpha);
    else for(int y=r.y;y<r.y+r.h;y++)
        tg_blend_span(c->p+(size_t)y*c->w+r.x,(size_t)r.w,rgb,alpha);
    tg_canvas_mark_damage(c,r.x,r.y,r.w,r.h);
}
void tg_canvas_clear(tg_canvas *c,uint32_t rgb) {
    if(c) tg_canvas_fill(c,(tg_rect){0,0,c->w,c->h},rgb);
}
const uint32_t *tg_canvas_pixels(const tg_canvas *c) { return c?c->p:NULL; }
int tg_canvas_width(const tg_canvas *c) { return c?c->w:0; }
int tg_canvas_height(const tg_canvas *c) { return c?c->h:0; }
int tg_canvas_damage(tg_canvas *c,tg_rect *r) {
    if(!c || !r || !c->damaged) return 0;
    *r=c->dirty; c->damaged=0; return 1;
}

tg_rect tg_canvas_render_bounds(const tg_canvas *c) {
    if(!c) return (tg_rect){0,0,0,0};
    tg_rect band={0,c->y0,c->w,c->y1-c->y0},clipped;
    if(!c->clip_active) return band;
    return tg_rect_intersect(band,c->clip,&clipped)?clipped:(tg_rect){0,0,0,0};
}
void tg_canvas_merge_damage(tg_canvas *owner,const tg_canvas *band) {
    if(owner&&band&&band->damaged) { tg_rect r=band->dirty; tg_canvas_mark_damage(owner,r.x,r.y,r.w,r.h); }
}
