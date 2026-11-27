#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif
#include "TKGPUAsync.H"
#include "TKGPUPhoto.H"
#include "../../src/widgets/image/TKGPUImageRegistry.H"
#include "../../src/widgets/canvas/TKGPUCanvasItems.H"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
static void nap(void) { Sleep(1); }
#else
#include <time.h>
static void nap(void) { struct timespec t={0,1000000}; nanosleep(&t,NULL); }
#endif
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
static int frame(tg_async *a,tg_canvas *c) {
    for(int k=0;k<5000;k++) { int rc=tg_async_poll(a,c); if(rc) return rc; nap(); }
    return -1;
}
int main(void) {
    tg_ui *u=tg_ui_create(); CHECK(u);
    tg_id id=tg_ui_add(u,".c",TG_CANVAS,NULL); CHECK(id);
    CHECK(tg_ui_geometry(u,id,TG_PLACE,(tg_rect){0,0,640,480})==TG_OK);
    uint32_t image=tg_image_add(u,"p",8,8); CHECK(image);
    tg_photo *p=tg_image_find(u,"p")->photo;
    CHECK(tg_photo_fill(p,(tg_rect){0,0,8,8},0xff0000ff)==TG_OK);
    CHECK(tg_canvas_item_add(u,(tg_canvas_item){.owner=id,.kind=TG_ITEM_IMAGE,.image_id=image,.anchor_nw=1,.x0=10,.y0=10}));
    CHECK(tg_ui_arrange(u,640,480)==TG_OK);
    tg_canvas *front=tg_canvas_create(640,480),*expected=tg_canvas_create(640,480); CHECK(front&&expected);
    CHECK(tg_ui_paint(u,expected)==TG_OK);
    tg_async *a=tg_async_create(); CHECK(a);
    CHECK(tg_async_submit_arranged(a,u,800,600,4)==TG_ERROR);
    CHECK(tg_async_submit_arranged(a,u,640,480,4)==TG_OK&&tg_async_busy(a));
    CHECK(tg_async_submit(a,u,640,480,4)==TG_ERROR);
    CHECK(tg_photo_fill(p,(tg_rect){0,0,8,8},0x0000ffff)==TG_OK);
    CHECK(tg_ui_configure(u,id,NULL,&(uint32_t){0xabcdef})==TG_OK);
    CHECK(tg_canvas_pixels(front)[10*640+10]==0);
    CHECK(frame(a,front)==1&&!tg_async_busy(a));
    CHECK(!memcmp(tg_canvas_pixels(front),tg_canvas_pixels(expected),640u*480u*4u));
    CHECK(tg_canvas_pixels(front)[10*640+10]==0xff0000);
    CHECK(tg_async_submit(a,u,800,600,4)==TG_OK);
    CHECK(tg_image_delete(u,"p")==TG_OK);
    CHECK(tg_ui_remove(u,id)==TG_OK);
    CHECK(frame(a,front)==1&&tg_canvas_width(front)==800&&tg_canvas_height(front)==600);
    CHECK(tg_canvas_pixels(front)[10*800+10]==0x0000ff);
    CHECK(tg_async_poll(a,front)==0);
    for(int k=0;k<20;k++) {
        CHECK(tg_async_submit(a,u,640+(k%2)*160,480,4)==TG_OK);
        CHECK(frame(a,front)==1);
        CHECK(tg_canvas_pixels(front)[0]==0x182536);
    }
    CHECK(tg_async_submit(a,u,640,480,4)==TG_OK);
    tg_ui_destroy(u); tg_async_destroy(a); tg_canvas_destroy(front); tg_canvas_destroy(expected);
    puts("async snapshots, mutation/deletion, resize, publication and destruction passed"); return 0;
}
