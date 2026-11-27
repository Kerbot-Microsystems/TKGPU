#ifndef TKGPU_X11_SHM_H
#define TKGPU_X11_SHM_H

#include "TKGPURuntime.H"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#ifdef TKGPU_X11_SHM
#include <X11/extensions/XShm.h>
#endif

typedef struct {
    XImage *image;
    int direct_rgb;
#ifdef TKGPU_X11_SHM
    int shm_attached;
    XShmSegmentInfo shm_info;
#endif
} tg_x11_slot;

typedef struct {
    tg_x11_slot slots[2];
    int active;
    int cap_w, cap_h;
    int shm_supported;
    int shm_completion_type;
    int in_flight;
} tg_x11_presenter;

void tg_x11_presenter_init(tg_x11_presenter *p, Display *display);
void tg_x11_presenter_destroy(tg_x11_presenter *p, Display *display);
int tg_x11_presenter_ensure(tg_x11_presenter *p, Display *display, int width, int height);
XImage *tg_x11_presenter_image(const tg_x11_presenter *p);
int tg_x11_presenter_direct_rgb(const tg_x11_presenter *p);
unsigned long tg_x11_channel_pixel(unsigned value, unsigned long mask);
void tg_x11_presenter_upload(tg_x11_presenter *p, Display *display, Drawable target, GC gc,
                             const uint32_t *pixels, int stride_w, tg_rect region);
void tg_x11_presenter_blit_existing(tg_x11_presenter *p, Display *display, Drawable target, GC gc,
                                    int width, int height);

#endif
