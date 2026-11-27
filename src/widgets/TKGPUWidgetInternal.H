#ifndef TKGPU_WIDGET_INTERNAL_H
#define TKGPU_WIDGET_INTERNAL_H
#include "TKGPUUi.H"
#include "ttk/TKGPUStyle.H"
enum { TG_WIDGET_LIMIT=4096,TG_PATH_INDEX_CAP=8192,TG_MENU_DEPTH=16,TG_EVENT_LIMIT=2048,TG_DIM_LIMIT=16384 };
struct tg_canvas_items;
struct tg_image_registry;
/* Native windows share variables and widget IDs, but select separate scene roots. */
enum { TG_OPTION_LIMIT=64 };
typedef struct { char pattern[128],value[256]; int priority; } tg_option_rule;
struct tg_ui { tg_id view_root; int native_toplevels; unsigned uniform_count; char uniform_names[128][64]; unsigned font_count; struct { char name[128],spec[256]; } fonts[256]; size_t option_count; tg_option_rule options[TG_OPTION_LIMIT]; tg_id path_index[TG_PATH_INDEX_CAP]; int path_index_ready,path_index_disabled; tg_widget items[TG_WIDGET_LIMIT]; size_t count; tg_id next; tg_event queue[TG_EVENT_LIMIT]; size_t head,len; tg_id focus,pressed,activated,entry_drag,menu_posted,drag_pane,hover,drag_scroll,drag_scale,scale_changed,dialog_capture; int menu_x,menu_y,drag_sash,drag_scroll_offset,drag_scale_offset; tg_id menu_host,menu_chain[TG_MENU_DEPTH]; unsigned menu_depth,menu_rows[TG_MENU_DEPTH]; tg_id choice_posted,choice_host; int choice_top,choice_active; size_t toggle_count,menu_toggle_count; tg_id menu_activation; unsigned menu_activation_row; char theme[64]; tg_theme themes[TG_THEME_LIMIT]; unsigned theme_count; tg_style_element elements[TG_ELEMENT_LIMIT]; unsigned element_count; tg_style *styles; size_t style_count,style_capacity; struct tg_canvas_items *drawing; struct tg_image_registry *images; };
tg_widget *tg_lookup(tg_ui *u,tg_id id);
void tg_path_index_added(tg_ui *u);
void tg_path_index_rebuild(tg_ui *u);
size_t tg_sibling_order(const tg_ui *,tg_id,size_t [TG_WIDGET_LIMIT]);
void tg_pack_reorder(tg_ui *,tg_id,tg_id,int);
int tg_base_dialog_command(tg_ui *,size_t,char **,char *,size_t);
int tg_base_dialog_event(tg_ui *,tg_event,int *);
int tg_base_dialog_resize_hit(const tg_ui *,int,int);
#endif
