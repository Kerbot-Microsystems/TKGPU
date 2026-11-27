#include "TKGPUParallelPaint.H"
#include "TKGPUWorker.H"
#include "../TKGPUFrameBufferInternal.H"
#include <stdlib.h>
enum { TG_MAX_PAINT_THREADS=64,TG_AUTO_MIN_PIXELS=65536,TG_AUTO_BAND_PIXELS=65536 };
struct tg_paint_pool { unsigned count; tg_worker *workers[TG_MAX_PAINT_THREADS-1]; };
typedef struct { tg_canvas band; tg_band_paint paint; const void *context; int result; } job;
static void run(void *p) { job *j=p; j->result=j->paint(j->context,&j->band); }
void tg_canvas_parallel_release(tg_canvas *c) {
    if(!c||!c->pool) return;
    for(unsigned k=0;k<c->pool->count;k++) tg_worker_destroy(c->pool->workers[k]);
    free(c->pool); c->pool=NULL;
}
unsigned tg_canvas_parallel_capacity(const tg_canvas *c) { return c&&c->pool?c->pool->count+1:1; }
static unsigned ensure(tg_canvas *c,unsigned n) {
    if(!c->pool) c->pool=calloc(1,sizeof *c->pool);
    if(!c->pool) return 1;
    while(c->pool->count<n-1) {
        tg_worker *w=tg_worker_create(); if(!w) break;
        c->pool->workers[c->pool->count++]=w;
    }
    unsigned cap=c->pool->count+1;
    return cap<n?cap:n;
}
/* Keep the large job array off the common single-thread dispatch path.
   This is particularly useful on legacy systems and small dirty scenes. */
static int paint_bands(tg_canvas *c,tg_band_paint paint,const void *ctx,unsigned n,unsigned *used) {
    job jobs[TG_MAX_PAINT_THREADS];
    for(unsigned k=0;k<n;k++) {
        jobs[k]=(job){*c,paint,ctx,TG_ERROR};
        jobs[k].band.pool=NULL; jobs[k].band.borrowed=1; jobs[k].band.damaged=0;
        jobs[k].band.y0=(int)((size_t)c->h*k/n);
        jobs[k].band.y1=(int)((size_t)c->h*(k+1)/n);
    }
    for(unsigned k=1;k<n;k++) tg_worker_submit(c->pool->workers[k-1],run,&jobs[k]);
    run(&jobs[0]);
    for(unsigned k=1;k<n;k++) tg_worker_wait(c->pool->workers[k-1]);
    int rc=TG_OK;
    for(unsigned k=0;k<n;k++) {
        tg_canvas_merge_damage(c,&jobs[k].band);
        if(jobs[k].result!=TG_OK) rc=TG_ERROR;
    }
    if(used) *used=n;
    return rc;
}

int tg_canvas_parallel_paint(tg_canvas *c,tg_band_paint paint,const void *ctx,unsigned requested,unsigned *used) {
    if(used) *used=0;
    if(!c||!paint||c->y0!=0||c->y1!=c->h||c->borrowed) return TG_ERROR;
    unsigned n=requested;
    if(!n) {
        size_t pixels=(size_t)c->w*c->h;
        n=tg_worker_cpu_count();
        unsigned useful=pixels<TG_AUTO_MIN_PIXELS?1u:(unsigned)(pixels/TG_AUTO_BAND_PIXELS);
        if(n>useful) n=useful;
    }
    if(n>TG_MAX_PAINT_THREADS) n=TG_MAX_PAINT_THREADS;
    if(n>(unsigned)c->h) n=(unsigned)c->h;
    if(n>1) n=ensure(c,n);
    if(n<=1) { if(used) *used=1; return paint(ctx,c); }
    return paint_bands(c,paint,ctx,n,used);
}
