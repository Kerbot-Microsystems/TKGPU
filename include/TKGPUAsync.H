#ifndef TKGPU_ASYNC_H
#define TKGPU_ASYNC_H
#include "TKGPUUi.H"
/* Owner-thread control. Submission captures the scene; polling never waits
   for rasterization. The caller must own front and keep it outside workers.
   No Tcl or native GUI callbacks run in the coordinator. */
typedef struct tg_async tg_async;
tg_async *tg_async_create(void);
void tg_async_destroy(tg_async *a);
int tg_async_busy(const tg_async *a);
int tg_async_submit(tg_async *a,const tg_ui *u,int w,int h,unsigned participants);
/* The owner must have arranged the live scene after its last layout mutation. */
int tg_async_submit_arranged(tg_async *a,const tg_ui *u,int w,int h,unsigned participants);
/* 1: new frame published, 0: idle/not ready, -1: failed. */
int tg_async_poll(tg_async *a,tg_canvas *front);
/* Reap a completed frame without replacing the current visible buffer. */
int tg_async_discard(tg_async *a);
#endif
