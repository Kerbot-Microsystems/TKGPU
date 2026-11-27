#ifndef TKGPU_UI_H
#define TKGPU_UI_H
#include "TKGPURuntime.H"
#include "TKGPUDock.H"
#include "TKGPUBaseDialog.H"
typedef struct tg_ui tg_ui;
typedef uint32_t tg_id;
enum { TG_WIDGET_PATH_CAP=512 };
typedef enum { TG_FRAME=1,TG_LABEL,TG_BUTTON,TG_CANVAS,TG_ENTRY,TG_TOPLEVEL,TG_PROGRESS,TG_SEPARATOR,TG_SCALE,TG_SCROLLBAR,TG_LABELFRAME,TG_COMBOBOX,TG_SPINBOX,TG_NOTEBOOK,TG_TREEVIEW,TG_CHECKBUTTON,TG_RADIOBUTTON,TG_PANEDWINDOW,TG_MENUBUTTON,TG_SIZEGRIP,TG_MENU,TG_LISTBOX,TG_TEXT,TG_MESSAGE } tg_kind;
typedef enum { TG_FREE=0,TG_PACK_TOP,TG_PACK_LEFT,TG_PLACE,TG_GRID,TG_PACK_BOTTOM,TG_PACK_RIGHT } tg_layout;
typedef enum { TG_POINTER_DOWN=1,TG_POINTER_UP,TG_POINTER_MOVE,TG_KEY_DOWN,TG_KEY_UP,TG_CLOSE,TG_TEXT_INPUT,TG_CONFIGURE,TG_FOCUS_IN,TG_FOCUS_OUT,TG_ENTER,TG_LEAVE,TG_POINTER_EXIT,TG_WHEEL,TG_MAP,TG_UNMAP,TG_IME_REPLACE } tg_event_kind;
enum { TG_KEY_LEFT=0x110000,TG_KEY_RIGHT,TG_KEY_HOME,TG_KEY_END,TG_KEY_BACKSPACE,TG_KEY_DELETE,TG_KEY_RETURN,TG_KEY_ARROW_UP,TG_KEY_ARROW_DOWN,TG_KEY_ESCAPE,TG_KEY_TAB,TG_KEY_INSERT,TG_KEY_F1,TG_KEY_F2,TG_KEY_F3,TG_KEY_F4,TG_KEY_F5,TG_KEY_F6,TG_KEY_F7,TG_KEY_F8,TG_KEY_F9,TG_KEY_F10,TG_KEY_F11,TG_KEY_F12,TG_KEY_PRIOR,TG_KEY_NEXT };
#define TG_NATIVE_KEY_EVENT 0x80000000u

enum { TG_KEY_VALUE_MASK=0x00ffffff, TG_MOD_SHIFT=0x01000000,TG_MOD_CONTROL=0x02000000,TG_MOD_ALT=0x04000000 };
enum { TG_STATE_NORMAL=0,TG_STATE_DISABLED,TG_STATE_READONLY };
typedef struct { tg_event_kind kind; tg_id target; int x,y; uint32_t key; } tg_event;
#define TG_IS_VIRTUAL(k) (((uint32_t)(k)&0xfff00000u)==0x30000000u)
static inline tg_event_kind tg_virtual_event(const char *name) {
    uint32_t h=0x55555555u;
    for(;*name;name++) h=(h*33u)^(unsigned char)*name;
    return (tg_event_kind)(0x30000000u|(h&0x000fffffu));
}
typedef struct { tg_id id,parent; unsigned child_count; tg_kind kind; tg_layout layout; tg_rect rect,request; tg_dock dock; tg_base_dialog dialog; uint64_t z_order; uint32_t pack_order; int border_width,relief; uint32_t bg,fg; unsigned char opacity,background_alpha,group_opacity,color_alpha; int explicit_fg,option_width,option_height,has_width,has_height; char path[TG_WIDGET_PATH_CAP]; char text[1024]; char font[128],font_spec[256],image_name[128],show[8],cursor_name[32]; int exportselection,takefocus,compound_center,entry_scroll; uint32_t active_bg,active_fg,disabled_fg; int has_active_bg,has_active_fg,has_disabled_fg; int text_anchor,justify,wraplength,text_padx,text_pady,has_ttk_padding,ttk_padding[4],no_pack_propagate,no_grid_propagate,highlight_width; uint32_t highlight_color,highlight_background; int has_highlight_color,has_highlight_background; char command[512]; size_t cursor,sel_first,sel_last,sel_anchor; int state,themed; unsigned char validate_mode,validate_busy; char validate_command[256],invalid_command[256]; unsigned ttk_state_set,ttk_state_clear; char style[64]; int vertical,indeterminate,length; double value,maximum; char value_text[32],maximum_text[32]; double minimum,first,last,resolution; char minimum_text[32]; uint32_t trough_color; int has_trough_color,has_resolution,slider_length,show_value; char choices[512],increment_text[32],choice_postcommand[256],choice_format[24]; int choice_height; double increment; int wrap; tg_id tabs[32],selected; unsigned tab_count; int notebook_traversal,tab_state; char tab_text[128],tab_image[128],tab_padding[48],tab_compound[8]; int tab_pad[4],tab_underline; unsigned tab_sticky; struct tg_tree *tree; char textvariable[128]; char variable[TG_WIDGET_PATH_CAP],on_value[128],off_value[128]; int explicit_variable,checked,alternate; int pane_weight,sashes[32]; char menu_path[TG_WIDGET_PATH_CAP]; struct tg_menu *menu; struct tg_listbox *listbox; struct tg_text_document *document; int pack_fill,pack_expand,pack_padx,pack_pady,pack_padx_extra,pack_pady_extra; int pack_ipadx,pack_ipady; int pack_anchor,place_anchor,place_border_mode; unsigned place_flags; double place_rel[4]; tg_rect canvas_region; int canvas_region_set,canvas_x,canvas_y,canvas_xincrement,canvas_yincrement; int scan_x,scan_y,scan_origin_x,scan_origin_y,scan_marked; char canvas_xscroll[256],canvas_yscroll[256]; unsigned canvas_scroll_sent; double canvas_scroll_last[4]; tg_rect grid_saved,grid_natural,place_natural; int grid_removed,grid_sticky,grid_padx,grid_pady,grid_padx_extra,grid_pady_extra,grid_ipadx,grid_ipady,grid_anchor; uint16_t grid_col_pad[256],grid_row_pad[256]; uint16_t grid_col_uniform[256],grid_row_uniform[256]; uint16_t grid_col_weight[256],grid_row_weight[256],grid_col_min[256],grid_row_min[256]; void *native_host; } tg_widget;
const char *tg_ui_font_spec(const tg_ui *,const char *);
tg_ui *tg_ui_create(void);
void tg_ui_destroy(tg_ui *u);
tg_id tg_ui_find(const tg_ui *u,const char *path);
tg_id tg_ui_add(tg_ui *u,const char *path,tg_kind kind,const char *text);
int tg_ui_remove(tg_ui *u,tg_id id);
const tg_widget *tg_ui_get(const tg_ui *u,tg_id id);
int tg_ui_geometry(tg_ui *u,tg_id id,tg_layout layout,tg_rect request);
int tg_ui_configure(tg_ui *u,tg_id id,const char *text,const uint32_t *bg);
int tg_ui_arrange(tg_ui *u,int width,int height);
int tg_ui_paint(const tg_ui *u,tg_canvas *c);
/* Ordered, clipped solid rectangles for a hardware backend. No CPU framebuffer.
   Returns TG_ERROR for unsupported scenes or sink failure; discard partial output. */
typedef int (*tg_draw_rect_sink)(void *,tg_rect,uint32_t,unsigned);
int tg_ui_record_rects(const tg_ui *,int width,int height,tg_draw_rect_sink,void *);
/* Image source is borrowed only for the duration of the callback; destination
   is clipped, source starts at its matching top-left pixel. */
typedef int (*tg_draw_image_sink)(void *,tg_rect,const unsigned char *,size_t stride,size_t length,uint32_t image_id,uint64_t revision,size_t source_offset);
int tg_ui_record_draws(const tg_ui *,int,int,tg_draw_rect_sink,tg_draw_image_sink,void *);
/* begin=1 saves background; begin=0 blends the saved region back with
   inverse opacity. Calls nest in strict LIFO order. Discard on any error. */
typedef int (*tg_draw_layer_sink)(void *,tg_rect,unsigned opacity,int begin);
int tg_ui_record_composite(const tg_ui *,int,int,tg_draw_rect_sink,tg_draw_image_sink,tg_draw_layer_sink,void *);
/* requested=0: auto, 1: serial, 2..64: explicit participants including owner.
   Scene must remain immutable and canvas exclusively owned until return. */
int tg_ui_paint_parallel(const tg_ui *u,tg_canvas *c,unsigned requested,unsigned *used);
tg_id tg_ui_hit(const tg_ui *u,int x,int y);
int tg_ui_focus(tg_ui *,tg_id);
tg_id tg_ui_focused(const tg_ui *);
int tg_ui_button_script(const tg_ui *,tg_id,char *,size_t);
int tg_ui_apply_event(tg_ui *,tg_event,char *,size_t);
int tg_interp_dispatch_input(tg_interp *);
int tg_ui_post(tg_ui *u,tg_event e);
int tg_ui_poll(tg_ui *u,tg_event *e);
/* Borrowed UI handle; valid until interpreter destruction. */
tg_ui *tg_interp_ui(tg_interp *i);
int tg_ui_command(tg_ui *u,size_t n,char **a,char *result,size_t capacity);
#endif
