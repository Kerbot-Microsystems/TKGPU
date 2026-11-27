#include "../../compat/tk/TKGPUWindow.H"
/* Unsupported platform: never manufacture a fake native window. */
int tg_window_open(tg_window *w) { (void)w; return 0; }
void tg_window_close_native(tg_window *w) { (void)w; }
void tg_window_free_native(tg_window *w) { (void)w; }
int tg_window_poll_native(tg_window *w) { (void)w; return 0; }

int tg_window_set_title(tg_window *w,const char *s) { (void)w; (void)s; return 0; }
int tg_window_set_size(tg_window *w,int x,int y) { (void)w; (void)x; (void)y; return 0; }
int tg_window_set_visible(tg_window *w,int v) { (void)w; (void)v; return 0; }

int tg_window_set_geometry(tg_window *w,int a,int b,int c,int d,int e,int f,int g) {
    (void)w;(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;return 0;
}
int tg_window_limits(tg_window *w) { (void)w; return 0; }
int tg_window_screen(tg_window *w,int *x,int *y) { (void)w;(void)x;(void)y;return 0; }
int tg_window_origin(tg_window *w,int *x,int *y) { (void)w;(void)x;(void)y;return 0; }

/* The application's platform policy retains the system titlebar here. */
int tg_window_override(tg_window *w,int enabled) { (void)w;(void)enabled;return 0; }
int tg_window_custom_caption(tg_window *w,int enabled) { (void)w;(void)enabled;return 0; }
int tg_window_caption_action(tg_window *w,int action) { (void)w;(void)action;return 0; }
int tg_window_caption_state(tg_window *w) { (void)w;return 0; }
