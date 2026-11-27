#ifndef TKGPU_TEXT_H
#define TKGPU_TEXT_H
#include "TKGPURuntime.H"
/* Built-in 5x7 fallback, Latin and Russian/Ukrainian Cyrillic letters, digits/basic punctuation.
   Lowercase maps to uppercase; unsupported scalars display '?'. */
int tg_text_measure(const char *text,int scale,int *width,int *height);
void tg_text_draw(tg_canvas *c,tg_rect clip,int x,int y,const char *text,int scale,uint32_t rgb);
/* Native shaping/mask path. TG_ERROR means unavailable or unsupported text.
   The bitmap path remains independent and available to legacy builds. */
const char *tg_text_native_backend(void);
int tg_text_native_enabled(void);
int tg_text_row_height(int bitmap_height);
void tg_text_draw_row(tg_canvas *,tg_rect,int,const char *,uint32_t);
int tg_text_font_caret(const char *,const char *,size_t,int *);
int tg_text_font_hit(const char *,const char *,int,size_t *);
int tg_text_font_move(const char *,const char *,size_t *,int);
/* Preserve the shaped caret's cluster/trailing index and weak/strong
   affinity without changing public logical text indices. */
typedef struct { size_t offset,index; int trailing,weak,valid; } tg_visual_cursor;
int tg_text_font_visual_hit(const char *,const char *,int,size_t *,tg_visual_cursor *);
int tg_text_font_visual_move(const char *,const char *,size_t *,int,tg_visual_cursor *);
int tg_text_font_visual_caret(const char *,const char *,size_t,int *,const tg_visual_cursor *);
int tg_text_font_selection(tg_canvas *,tg_rect,int,int,const char *,const char *,size_t,size_t,uint32_t);
int tg_text_layout_measure(const char *,const char *,int,int *,int *);
int tg_text_layout_draw(tg_canvas *,tg_rect,const char *,const char *,int,int,int,int,int,uint32_t);
int tg_text_font_measure(const char *,const char *,int *,int *);
int tg_text_font_draw(tg_canvas *,tg_rect,int,int,const char *,const char *,uint32_t);
int tg_text_native_measure(const char *,int,int *,int *);
int tg_text_native_draw(tg_canvas *,tg_rect,int,int,const char *,int,uint32_t);
/* Native shaped-run caret / hit test: UTF-8 byte offset <-> visual x. */
int tg_text_native_caret(const char *,int,size_t,int *);
int tg_text_native_hit(const char *,int,int,size_t *);
int tg_text_native_selection(tg_canvas *,tg_rect,int,int,const char *,int,size_t,size_t,uint32_t);
#endif
