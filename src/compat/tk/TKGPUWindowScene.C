#include "TKGPUWindow.H"
#include "../../widgets/TKGPUWidgetInternal.H"
#include "../../render/parallel/TKGPUWorker.H"
int tg_window_select(tg_window *w,int width,int height) {
    tg_ui *u=tg_interp_ui(w->interp);
    if(!u||!tg_ui_get(u,w->root)) return 0;
    u->view_root=w->root;
    /* Each native toplevel retains its own subtree geometry. Selecting a
       different host for drawing/hit-testing is not a layout mutation. */
    if(w->layout_dirty||w->layout_width!=width||w->layout_height!=height) {
        if(tg_ui_arrange(u,width,height)!=TG_OK) return 0;
        w->layout_width=width; w->layout_height=height; w->layout_dirty=0;
#ifdef _WIN32
        tg_window_sync_child_hosts(w);
#endif
    }
    return 1;
}
/* A borderless popup without an explicit size follows the natural size of
   its packed content, rather than keeping the default 640x480 host. */
void tg_window_fit_override(tg_window *w) {
    if(!w||!w->alive||w->root==1||!w->override_redirect||w->explicit_size) return;
    tg_ui *u=tg_interp_ui(w->interp);
    if(!tg_window_select(w,w->requested_width,w->requested_height)) return;
    const tg_widget *g=tg_ui_get(u,w->root);
    if(!g||!g->child_count) return;
    int width=g->request.w,height=g->request.h;
    if(width<w->min_width) width=w->min_width;
    if(height<w->min_height) height=w->min_height;
    if(width>w->max_width) width=w->max_width;
    if(height>w->max_height) height=w->max_height;
    if(width<=0||height<=0||(width==w->requested_width&&height==w->requested_height)) return;
    if(tg_window_set_size(w,width,height)) {
        w->requested_width=width; w->requested_height=height;
        w->dirty=w->layout_dirty=1;
    }
}
/* Only the owner publishes frames and touches the live interpreter. */
int tg_window_refresh(tg_window *w,int width,int height) {
    if(!w||!w->canvas||width<=0||height<=0||width>16384||height>16384) return 0;
    int ready=0;
    if(w->renderer) {
        int rc=tg_async_poll(w->renderer,w->canvas);
        if(rc<0) {
            tg_async_destroy(w->renderer); w->renderer=NULL;
            w->async_disabled=1; w->dirty=1;
        } else if(rc>0) {
            ready=1;
        }
    }
    if(w->layout_width!=width||w->layout_height!=height) { w->dirty=1; w->layout_dirty=1; }
    if(!w->dirty) return ready;
    tg_ui *u=tg_interp_ui(w->interp);
    if(!tg_window_select(w,width,height)) return ready;
    if((tg_canvas_width(w->canvas)!=width||tg_canvas_height(w->canvas)!=height)&&
       tg_canvas_resize(w->canvas,width,height)!=TG_OK) return ready;
    /* Paint directly in parallel across worker threads without deep-cloning
       all photos/widgets/canvas items on every frame. */
    if(tg_ui_paint_parallel(u,w->canvas,0,NULL)==TG_OK) { w->dirty=0; return 1; }
    return ready;
}
void tg_window_input(tg_window *w,tg_event_kind kind,int x,int y,uint32_t key) {
    tg_ui *u=tg_interp_ui(w->interp);
    int width = kind==TG_CONFIGURE ? x : (w->layout_width > 0 ? w->layout_width : w->requested_width);
    int height = kind==TG_CONFIGURE ? y : (w->layout_height > 0 ? w->layout_height : w->requested_height);
    tg_window_select(w,width,height);
    int keyboard=kind==TG_KEY_DOWN||kind==TG_KEY_UP||kind==TG_TEXT_INPUT;
    /* Resolve the focused child when the event is dispatched, after earlier
       pointer/focus events. Keep the originating native window in target. */
    tg_id target=(keyboard||kind==TG_CLOSE||kind==TG_CONFIGURE||kind==TG_FOCUS_IN||kind==TG_FOCUS_OUT)?w->root:tg_ui_hit(u,x,y);
    if(keyboard) key|=TG_NATIVE_KEY_EVENT;
    tg_event event={kind,kind==TG_POINTER_EXIT?0:target?target:w->root,x,y,key};
    (void)tg_ui_post(u,event);
}
