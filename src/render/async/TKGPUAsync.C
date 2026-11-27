#include "TKGPUAsync.H"
#include "TKGPUSnapshot.H"
#include "../../widgets/TKGPUWidgetInternal.H"
#include "../parallel/TKGPUWorker.H"
#include "../TKGPUFrameBufferInternal.H"
#include <stdlib.h>
#define MAX_FRAME_PIXELS ((size_t)16*1024*1024)
struct tg_async {
    tg_worker *coordinator;
    tg_canvas *back;
    tg_ui *snapshot;
    unsigned participants;
    int busy,result;
};
static void draw(void *p) {
    tg_async *a=p;
    a->result=tg_ui_paint_parallel(a->snapshot,a->back,a->participants,NULL);
}
tg_async *tg_async_create(void) {
    tg_async *a=calloc(1,sizeof *a); if(!a) return NULL;
    a->coordinator=tg_worker_create();
    if(!a->coordinator) { free(a); return NULL; }
    return a;
}
void tg_async_destroy(tg_async *a) {
    if(!a) return;
    if(a->busy) tg_worker_wait(a->coordinator);
    tg_worker_destroy(a->coordinator);
    tg_ui_destroy(a->snapshot); tg_canvas_destroy(a->back); free(a);
}
int tg_async_busy(const tg_async *a) { return a&&a->busy; }
static int submit(tg_async *a,const tg_ui *u,int w,int h,unsigned participants,int arranged) {
    if(!a||a->busy||!u||w<=0||h<=0||w>16384||h>16384||
       (size_t)w*(size_t)h>MAX_FRAME_PIXELS) return TG_ERROR;
    if(arranged) {
        const tg_widget *root=tg_ui_get(u,u->view_root?u->view_root:1);
        if(!root||root->rect.w!=w||root->rect.h!=h) return TG_ERROR;
    }
    tg_ui *snapshot=tg_snapshot_create(u); if(!snapshot) return TG_ERROR;
    if(!arranged&&tg_ui_arrange(snapshot,w,h)!=TG_OK) { tg_ui_destroy(snapshot); return TG_ERROR; }
    if(!a->back) a->back=tg_canvas_create(w,h);
    else if((a->back->w!=w||a->back->h!=h)&&tg_canvas_resize(a->back,w,h)!=TG_OK) {
        tg_ui_destroy(snapshot); return TG_ERROR;
    }
    if(!a->back) { tg_ui_destroy(snapshot); return TG_ERROR; }
    a->snapshot=snapshot; a->participants=participants; a->busy=1;
    tg_worker_submit(a->coordinator,draw,a); return TG_OK;
}
int tg_async_submit(tg_async *a,const tg_ui *u,int w,int h,unsigned participants) {
    return submit(a,u,w,h,participants,0);
}
int tg_async_submit_arranged(tg_async *a,const tg_ui *u,int w,int h,unsigned participants) {
    return submit(a,u,w,h,participants,1);
}
int tg_async_discard(tg_async *a) {
    if(!a||!a->busy||!tg_worker_try_wait(a->coordinator)) return 0;
    a->busy=0; tg_ui_destroy(a->snapshot); a->snapshot=NULL;
    return a->result==TG_OK?1:-1;
}
int tg_async_poll(tg_async *a,tg_canvas *front) {
    if(!a||!front||front->borrowed) return -1;
    if(!a->busy||!tg_worker_try_wait(a->coordinator)) return 0;
    a->busy=0; tg_ui_destroy(a->snapshot); a->snapshot=NULL;
    if(a->result!=TG_OK) return -1;
    /* Exchange just image storage: the persistent render pool stays on back. */
    tg_canvas *b=a->back;
    uint32_t *p=front->p; int w=front->w,h=front->h;
    front->p=b->p; front->w=b->w; front->h=b->h;
    front->y0=0; front->y1=front->h; front->damaged=0;
    tg_canvas_mark_damage(front,0,0,front->w,front->h);
    b->p=p; b->w=w; b->h=h; b->y0=0; b->y1=h; b->damaged=0;
    return 1;
}
