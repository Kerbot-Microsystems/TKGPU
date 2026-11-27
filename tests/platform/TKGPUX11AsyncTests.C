#define _POSIX_C_SOURCE 200809L
#include "../../src/compat/tk/TKGPUWindow.H"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/render/parallel/TKGPUWorker.H"
#ifdef TKGPU_ISOLATED_X11_TEST
/* Isolated backend build only; normal CMake target uses the real interpreter. */
struct tg_interp { tg_ui *ui; };
tg_ui *tg_interp_ui(tg_interp *i) { return i->ui; }
#endif
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
static void pump(tg_window *w) { tg_window_poll_native(w); struct timespec t={0,1000000}; nanosleep(&t,NULL); }
static int until(tg_window *w,int width,uint32_t rgb) {
    for(int k=0;k<3000;k++) {
        pump(w);
        if(!w->dirty&&!tg_async_busy(w->renderer)&&tg_canvas_width(w->canvas)==width&&tg_canvas_pixels(w->canvas)[0]==rgb) return 1;
    }
    return 0;
}
int main(void) {
    if(!getenv("DISPLAY")||tg_worker_cpu_count()<2) return 77;
#ifdef TKGPU_ISOLATED_X11_TEST
    tg_interp isolated={tg_ui_create()}; CHECK(isolated.ui);
    tg_interp *i=&isolated;
#else
    tg_interp *i=tg_create(); CHECK(i);
#endif
    tg_ui *ui=tg_interp_ui(i); CHECK(ui);
    tg_window w={0}; w.interp=i; w.canvas=tg_canvas_create(640,480); CHECK(w.canvas);
    CHECK(tg_window_open(&w)); w.alive=1; w.dirty=1;
    CHECK(until(&w,640,0x182536)); CHECK(w.renderer);
    Display *d=XOpenDisplay(NULL); CHECK(d);
    Window root=DefaultRootWindow(d),r,p,*children=NULL; unsigned n=0;
    CHECK(XQueryTree(d,root,&r,&p,&children,&n)&&n>0);
    Window win=children[n-1]; XFree(children);
    CHECK(tg_ui_configure(ui,1,NULL,&(uint32_t){0x123456})==TG_OK); w.dirty=1;
    CHECK(until(&w,640,0x123456)); XSync(d,False);
    XImage *img=XGetImage(d,win,0,0,1,1,AllPlanes,ZPixmap); CHECK(img);
    CHECK((XGetPixel(img,0,0)&0xffffff)==0x123456); XDestroyImage(img);
    XResizeWindow(d,win,800,600); XSync(d,False);
    CHECK(until(&w,800,0x123456));
    CHECK(tg_ui_configure(ui,1,NULL,&(uint32_t){0x778899})==TG_OK); w.dirty=1;
    CHECK(until(&w,800,0x778899));
    tg_window_close_native(&w); CHECK(!w.alive);
    tg_async_destroy(w.renderer); tg_window_free_native(&w); tg_canvas_destroy(w.canvas); XCloseDisplay(d);
#ifdef TKGPU_ISOLATED_X11_TEST
    tg_ui_destroy(ui);
#else
    tg_destroy(i);
#endif
    puts("X11 async presentation, pixel readback, resize and close passed"); return 0;
}
