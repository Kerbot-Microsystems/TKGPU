#include "TKGPUWindow.H"
#include "TKGPUHotkeys.H"
#include "../events/script/TKGPUIdle.H"
#include "../../core/TKGPUCoreInternal.H"
#include "../../platform/threads/TKGPUThreadNative.H"
#include "../../platform/time/TKGPUClock.H"
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include "../../widgets/TKGPUWidgetInternal.H"
static tg_window *windows;
static Tcl_ThreadId self(void) { return (Tcl_ThreadId)tg_native_thread_id(); }
static void close_host(void *p) {
    tg_window *w=p;
    if(w->owner!=self()) return;
    tg_idle_cancel(w->layout_idle); w->layout_idle=0;
    if(w->grabbed) tg_window_grab(w,0);
    tg_animation_clear(w);
    if(w->alive) tg_window_close_native(w);
    w->alive=0;
    tg_async_destroy(w->renderer); w->renderer=NULL;
}
static void free_single(void *p) {
    tg_window *w=p;
    close_host(w);
    tg_native_guard_lock();
    tg_window **link=&windows;
    while(*link&&*link!=w) link=&(*link)->next;
    if(*link) *link=w->next;
    tg_native_guard_unlock();
    tg_window_free_native(w);
    tg_canvas_destroy(w->canvas); free(w->fonts); free(w);
}
static void free_host(void *p) {
    tg_window *main=p;
    for(;;) {
        tg_window *found=NULL;
        tg_native_guard_lock();
        for(tg_window *w=windows;w;w=w->next) if(w!=main&&w->interp==main->interp&&w->owner==self()) { found=w; break; }
        tg_native_guard_unlock();
        if(!found) break;
        free_single(found);
    }
    free_single(main);
}
static void close_all(void *p) {
    tg_window *main=p;
    for(tg_window *w=windows;w;w=w->next) if(w->interp==main->interp&&w->owner==self()) close_host(w);
}
static int origin_host(void *p,tg_id target,int *x,int *y) {
    tg_window *main=p;
    const tg_widget *g=tg_ui_get(tg_interp_ui(main->interp),target);
    tg_window *w=g?tg_window_for_widget(main->interp,g->path):main;
    return w&&tg_window_origin(w,x,y);
}
/* Geometry is idle work, including update_idletasks on hidden dialogs.
   It must not depend on a subsequent native mouse or resize message. */
static void layout_idle(void *p) {
    tg_window *main=p;
    main->layout_idle=0;
    for(tg_window *w=windows;w;w=w->next) {
        if(w->interp!=main->interp||w->owner!=self()||!w->alive) continue;
        tg_window_fit_override(w);
        int width=w->requested_width,height=w->requested_height;
#ifdef _WIN32
        RECT client;
        if(GetClientRect((HWND)w->native,&client)) { width=client.right; height=client.bottom; }
#endif
        if(width>0&&height>0) tg_window_select(w,width,height);
#ifdef _WIN32
        if(!w->withdrawn) InvalidateRect((HWND)w->native,NULL,FALSE);
#endif
    }
}
static void repaint_host(void *p,tg_id target) {
    tg_window *main=p;
    const tg_widget *g=tg_ui_get(tg_interp_ui(main->interp),target);
    tg_window *w=g?tg_window_for_widget(main->interp,g->path):main;
    if(w&&w->alive) {
        w->dirty=1;
        if(!main->layout_idle) main->layout_idle=tg_idle_add(layout_idle,main);
    }
}
static void dirty_host(void *p) {
    tg_window *main=p;
    if(!main->layout_idle) main->layout_idle=tg_idle_add(layout_idle,main);
    for(tg_window *w=windows;w;w=w->next) if(w->interp==main->interp&&w->owner==self()) {
        if(w->root!=1&&!tg_ui_get(tg_interp_ui(w->interp),w->root)) close_host(w);
        w->dirty=1; w->layout_dirty=1;
    }
}
int Tk_Init(Tcl_Interp *i) {
    if(!i||i->depth||i->disposing) return TG_ERROR;
    if(i->tk_host) return Tk_MainWindow(i)?TG_OK:tg_set_result(i,"main window has been closed",TG_ERROR);
    if(Tcl_Init(i)!=TG_OK) return TG_ERROR;
    tg_window *w=calloc(1,sizeof *w);
    if(!w) return tg_set_result(i,"cannot allocate window",TG_ERROR);
    w->alpha=1.0; w->root=1; tg_interp_ui(i)->view_root=1; tg_interp_ui(i)->native_toplevels=1;
    w->interp=i; w->owner=self(); w->canvas=tg_canvas_create(640,480);
    strcpy(w->title,"tk"); w->requested_width=640; w->requested_height=480;
    w->min_width=w->min_height=1; w->max_width=w->max_height=16384; w->resize_x=w->resize_y=1;
    if(!w->canvas||!tg_window_open(w)) {
        tg_window_free_native(w); tg_canvas_destroy(w->canvas); free(w);
        return tg_set_result(i,"no available native TKGPU window backend",TG_ERROR);
    }
    w->alive=1; w->dirty=1;
    if(!Tcl_SetVar2(i,"tk_version",NULL,"8.6",TKGPU_LEAVE_ERR_MSG)) {
        tg_window_free_native(w); tg_canvas_destroy(w->canvas); free(w);
        return tg_set_result(i,"cannot initialize Tk target version",TG_ERROR);
    }
    i->tk_host=w; i->tk_command=tg_window_command; i->tk_hotkey=tg_hotkey_dispatch; i->tk_dispose=free_host; i->tk_invalidate=dirty_host; i->tk_repaint=repaint_host; i->tk_close=close_all; i->tk_origin=origin_host; i->tk_clock=tg_clock_now;
    tg_native_guard_lock(); w->next=windows; windows=w; tg_native_guard_unlock();
    return tg_set_result(i,"",TG_OK);
}
Tk_Window Tk_MainWindow(Tcl_Interp *i) {
    tg_window *w=i?i->tk_host:NULL;
    return w&&w->owner==self()&&w->alive?w:NULL;
}
tg_window *tg_window_for_widget(Tcl_Interp *i,const char *path) {
    tg_ui *u=tg_interp_ui(i);
    const tg_widget *g=tg_ui_get(u,tg_ui_find(u,path));
    while(g&&g->id!=1&&g->kind!=TG_TOPLEVEL) g=tg_ui_get(u,g->parent);
    if(!g) return NULL;
    for(tg_window *w=windows;w;w=w->next) if(w->interp==i&&w->root==g->id&&w->owner==self()) return w->alive?w:NULL;
    if(g->id==1) return NULL;
    tg_window *w=calloc(1,sizeof *w); if(!w) return NULL;
    w->alpha=1.0; w->root=g->id; w->interp=i; w->owner=self(); w->canvas=tg_canvas_create(640,480);
    w->requested_width=640; w->requested_height=480; w->min_width=w->min_height=1;
    w->max_width=w->max_height=16384; w->resize_x=w->resize_y=1;
    strcpy(w->title,"TKGPU");
    if(!w->canvas||!tg_window_open(w)) { tg_window_free_native(w); tg_canvas_destroy(w->canvas); free(w); return NULL; }
    w->alive=1; w->dirty=w->layout_dirty=1;
    tg_native_guard_lock(); w->next=windows; windows=w; tg_native_guard_unlock();
    return w;
}
int tg_window_grab(tg_window *target,int enabled) {
    if(!target||target->owner!=self()) return 0;
    for(tg_window *w=windows;w;w=w->next) if(w->interp==target->interp&&w->owner==self()) {
        w->grabbed=enabled&&w==target;
#ifdef _WIN32
        if(w->native) EnableWindow((HWND)w->native,!enabled||w==target);
#endif
    }
    return 1;
}
int Tk_GetNumMainWindows(void) {
    int n=0; Tcl_ThreadId owner=self();
    tg_native_guard_lock();
    for(tg_window *w=windows;w;w=w->next)
        if(w->owner==owner&&w->alive&&n<INT_MAX) n++;
    tg_native_guard_unlock(); return n;
}
void tg_window_frame_stats(Tcl_Interp *i,tg_frame_stats *out) {
    if(!out) return;
    memset(out,0,sizeof *out);
    if(!i) return;
    Tcl_ThreadId owner=self();
    tg_native_guard_lock();
    for(tg_window *w=windows;w;w=w->next) {
        if(w->interp!=i||w->owner!=owner||!w->alive) continue;
        out->present_frames+=w->present_frames;
        out->gpu_frames+=w->gpu_frames;
        out->total_frame_us+=w->present_time_us;
        if(w->last_frame_us>out->last_frame_us) out->last_frame_us=w->last_frame_us;
        out->windows++;
    }
    tg_native_guard_unlock();
}
int tg_window_pending(void) { return Tk_GetNumMainWindows()>0; }
void tg_window_request_close(tg_window *w) {
    if(w&&w->alive) w->close_pending=1;
}
int tg_window_dispatch_close(tg_window *w) {
    if(!w||!w->alive||!w->close_pending||(w->interp->depth&&!w->interp->event_pump_depth)||w->interp->disposing) return 0;
    w->close_pending=0;
    if(!w->close_protocol[0]) {
        if(w->root==1) close_all(w);
        else {
            tg_ui_remove(tg_interp_ui(w->interp),w->root);
            close_host(w);
        }
    }
    else {
        char script[sizeof w->close_protocol]; strcpy(script,w->close_protocol);
        Tcl_Interp *i=w->interp;
        const char *cursor=script;
        if(tg_eval_script(i,&cursor,0)!=TG_OK) {
            char *message=tg_copy_string(i->result);
            if(message) { (void)tg_var_store(i,"errorInfo",message); free(message); }
        }
    }
    return 1;
}
int tg_window_dispatch(void) {
    Tcl_ThreadId owner=self();
    /* Only the owner thread may destroy its windows. Capture the current
       list under the guard; read next pointers under the guard as well. */
    int native_handled=0;
    tg_native_guard_lock(); tg_window *w=windows;
    /* Refresh every live host before running application callbacks. A busy
       tooltip or secondary window must not starve the main window's frames. */
    while(w) {
        if(w->owner==owner&&w->alive) {
            tg_native_guard_unlock();
            native_handled|=tg_window_poll_native(w)!=0;
            tg_native_guard_lock();
        }
        w=w->next;
    }
    w=windows;
    while(w) {
        if(w->owner==owner&&w->alive) {
            tg_native_guard_unlock();
            if(tg_window_dispatch_close(w)) return 1;
            int dispatched=0;
            while(dispatched<32&&tg_interp_dispatch_input(w->interp)) dispatched++;
            if(dispatched) return 1;
            if(!w->layout_dirty&&tg_interp_dispatch_scroll(w->interp)) return 1;
            tg_native_guard_lock();
        }
        w=w->next;
    }
    tg_native_guard_unlock(); return native_handled;
}
