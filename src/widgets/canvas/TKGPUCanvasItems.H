#ifndef TKGPU_CANVAS_ITEMS_H
#define TKGPU_CANVAS_ITEMS_H
#include "TKGPUUi.H"
enum { TG_LINE_POINTS=64,TG_ITEM_LIMIT=262144, TG_ITEM_LINE=1,TG_ITEM_RECT,TG_ITEM_OVAL,TG_ITEM_IMAGE,TG_ITEM_TEXT,TG_ITEM_WINDOW,TG_ITEM_POLYGON };
typedef struct {
    uint32_t id; tg_id owner; int kind,x0,y0,x1,y1;
    uint32_t image_id; tg_id window_id; int window_width,window_height; int anchor_nw;
    unsigned text_anchor,text_scale; char text[512],font[128],tags[256];
    unsigned point_count; int points[2*TG_LINE_POINTS]; unsigned width,round_cap,round_join;
    uint32_t fill,outline; int has_fill,has_outline; unsigned char opacity,opacity_set;
    unsigned char hidden,smooth,splinesteps;
    unsigned short dash_on,dash_off;
    int is_arc,arc_style; double arc_start,arc_extent;
} tg_canvas_item;
typedef struct tg_canvas_items {
    size_t count,capacity; uint32_t next,current_id; tg_canvas_item *item;
} tg_canvas_items;
uint32_t tg_canvas_item_add(tg_ui *u,tg_canvas_item item);
void tg_canvas_items_destroy(tg_ui *u);
void tg_canvas_items_remove(tg_ui *u,tg_id owner,uint32_t id);
const tg_canvas_item *tg_canvas_window_item(const tg_ui *,tg_id,tg_id);
int tg_canvas_item_bounds(const tg_ui *,const tg_canvas_item *,tg_rect *);
int tg_canvas_items_paint(const tg_ui *u,const tg_widget *w,tg_canvas *c);
int tg_canvas_option(tg_widget *,const char *,const char *);
int tg_canvas_query(const tg_widget *,const char *,char *,size_t);
void tg_canvas_view_clamp(tg_widget *);
void tg_canvas_view_fractions(const tg_widget *,int,double *,double *);
int tg_canvas_item_command(tg_ui *u,tg_id owner,size_t n,char **a,char *b,size_t cap);
#endif
