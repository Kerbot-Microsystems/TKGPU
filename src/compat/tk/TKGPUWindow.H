#ifndef TKGPU_WINDOW_H
#define TKGPU_WINDOW_H
#include "TKGPUCompat.H"
#include "TKGPUUi.H"
#include "TKGPUAsync.H"
#include "TKGPUTitleBar.H"
struct TKGPUOpaqueWindow {
    Tcl_Interp *interp;
    Tcl_ThreadId owner;
    tg_canvas *canvas;
    void *native;
    void *icon_small,*icon_big;
    int alive,dirty,layout_dirty,mouse_tracking,grabbed,override_redirect,live_resize;
    tg_id root; double alpha; int topmost;
    char icon_name[512];
    struct TKGPUOpaqueWindow *transient_owner;
    tg_titlebar caption;
    uint16_t pending_surrogate;
    char title[512];
    char close_protocol[2048];
    int close_pending;
    int requested_width,requested_height,withdrawn,map_pending,explicit_size;
    uint64_t layout_idle;
    int position_set,position_x,position_y,negative_x,negative_y;
    int min_width,min_height,max_width,max_height,resize_x,resize_y;
    tg_async *renderer;
    struct tg_animation_state *animations;
    struct tg_named_fonts *fonts;
    int layout_width,layout_height,async_disabled;
    void *gpu_present;
    int gpu_present_enabled,gpu_present_failed,gpu_present_active;
    int gpu_raster_enabled,gpu_raster_failed,gpu_raster_active;
    uint64_t gpu_frames,gpu_instances,gpu_texture_hits,gpu_texture_uploads,gpu_upload_bytes,gpu_layer_passes;
    uint64_t present_frames,present_time_us,last_frame_us;
    struct TKGPUOpaqueWindow *next;
};
typedef struct TKGPUOpaqueWindow tg_window;
typedef struct {
    uint64_t present_frames,gpu_frames,last_frame_us,total_frame_us,windows;
} tg_frame_stats;
void tg_window_frame_stats(Tcl_Interp *,tg_frame_stats *);
int tg_window_open(tg_window *);
int tg_window_custom_caption(tg_window *,int);
int tg_window_override(tg_window *,int);
int tg_window_caption_action(tg_window *,int);
int tg_window_caption_state(tg_window *);
int tg_window_set_title(tg_window *,const char *);
int tg_window_set_size(tg_window *,int,int);
int tg_window_set_geometry(tg_window *,int,int,int,int,int,int,int);
int tg_window_limits(tg_window *);
int tg_window_screen(tg_window *,int *,int *);
int tg_window_origin(tg_window *,int *,int *);
int tg_window_set_visible(tg_window *,int);
#ifdef TKGPU_COCOA
int tg_cocoa_alpha(tg_window *,double);
int tg_cocoa_topmost(tg_window *,int);
int tg_cocoa_transient(tg_window *,tg_window *);
int tg_cocoa_clipboard_put(const char *);
int tg_cocoa_clipboard_get(char *,size_t);
#endif
int tg_window_command(Tcl_Interp *,size_t,char **);
int tg_dialog_command(Tcl_Interp *,size_t,char **);
int tg_clipboard_command(Tcl_Interp *,size_t,char **);
int tg_font_command(Tcl_Interp *,size_t,char **);
int tg_animation_command(Tcl_Interp *,size_t,char **);
void tg_animation_clear(tg_window *);
void tg_window_close_native(tg_window *);
void tg_window_free_native(tg_window *);
int tg_window_poll_native(tg_window *);
int tg_window_refresh(tg_window *,int,int);
void tg_window_input(tg_window *,tg_event_kind,int,int,uint32_t);
int tg_window_pending(void);
tg_window *tg_window_for_widget(Tcl_Interp *,const char *);
int tg_window_grab(tg_window *,int);
int tg_window_select(tg_window *,int,int);
void tg_window_fit_override(tg_window *);
int tg_window_dispatch(void);
void tg_window_request_close(tg_window *);
#ifdef _WIN32
int tg_window_icon_bitmap(tg_window *,const char *);
void *tg_window_child_host(tg_window *,tg_id);
void tg_window_sync_child_hosts(tg_window *);
#endif
int tg_window_dispatch_close(tg_window *);
#endif
