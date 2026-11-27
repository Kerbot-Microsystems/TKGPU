#ifndef TKGPU_H
#define TKGPU_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Experimental private API, not the Tcl ABI. */
typedef struct tg_interp tg_interp;
enum { TG_OK=0, TG_ERROR=1, TG_BREAK=3, TG_CONTINUE=4 };
tg_interp *tg_create(void);
void tg_destroy(tg_interp *i);
int tg_eval(tg_interp *i, const char *script);
/* Direct literal argument dispatch; not the Tcl C ABI. */
int tg_call(tg_interp *i,size_t argc,const char *const argv[]);
const char *tg_result(const tg_interp *i);
typedef struct { int x, y, w, h; } tg_rect;
typedef struct tg_canvas tg_canvas;
tg_canvas *tg_canvas_create(int width, int height);
void tg_canvas_destroy(tg_canvas *c);
int tg_canvas_resize(tg_canvas *c, int width, int height);
void tg_canvas_clear(tg_canvas *c, uint32_t rgb);
void tg_canvas_fill(tg_canvas *c, tg_rect rect, uint32_t rgb);
const uint32_t *tg_canvas_pixels(const tg_canvas *c);
int tg_canvas_width(const tg_canvas *c);
int tg_canvas_height(const tg_canvas *c);
/* Returns and clears accumulated damage, in pixel coordinates. */
int tg_canvas_damage(tg_canvas *c, tg_rect *rect);
#ifdef __cplusplus
}
#endif
#endif
