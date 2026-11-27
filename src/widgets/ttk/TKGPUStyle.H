#ifndef TKGPU_STYLE_H
#define TKGPU_STYLE_H
#include "TKGPUUi.H"
enum { TG_STYLE_LIMIT=512, TG_STYLE_OPTIONS=24, TG_STYLE_VALUE=384, TG_THEME_LIMIT=32, TG_ELEMENT_LIMIT=64, TG_LAYOUT_NODES=32, TG_LAYOUT_BYTES=2048 };
typedef struct { char name[64],parent[64]; } tg_theme;
typedef struct { char key[32],value[TG_STYLE_VALUE],map[TG_STYLE_VALUE]; unsigned has_value,has_map; } tg_style_property;
typedef struct { char name[64],theme[64],images[384]; int padding; } tg_style_element;
typedef struct { char name[64]; int child,next; unsigned sticky,side; } tg_style_node;
typedef struct { char name[64],theme[64]; size_t count; tg_style_property properties[TG_STYLE_OPTIONS];
    int has_layout,node_count; char layout[TG_LAYOUT_BYTES]; tg_style_node nodes[TG_LAYOUT_NODES]; } tg_style;
int tg_style_layout_paint(const tg_ui *,const tg_widget *,tg_canvas *,uint32_t,uint32_t,unsigned);
void tg_style_layout_decorate(const tg_ui *,const tg_widget *,tg_canvas *,uint32_t,uint32_t,unsigned);
int tg_style_command(tg_ui *,size_t,char **,char *,size_t);
int tg_style_identify(const tg_ui *,const tg_widget *,size_t,char **,char *,size_t);
const char *tg_style_current_theme(const tg_ui *);
int tg_style_select_theme(tg_ui *,const char *);
int tg_style_lookup_value(const tg_ui *,const char *,const char *,int,int,char *,size_t);
int tg_style_lookup_color(const tg_ui *,const char *,const char *,int,int,uint32_t *);
int tg_style_widget_value(const tg_ui *,const tg_widget *,const char *,char *,size_t);
int tg_style_widget_color(const tg_ui *,const tg_widget *,const char *,uint32_t *);
int tg_style_option(tg_widget *,const char *,const char *);
int tg_ttk_state_command(tg_ui *,tg_widget *,size_t,char **,char *,size_t);
int tg_ttk_state_matches(const tg_ui *,const tg_widget *,const char *,int *);
void tg_style_colors(const tg_ui *,const tg_widget *,uint32_t *,uint32_t *);
void tg_style_colors_alpha(const tg_ui *,const tg_widget *,uint32_t *,uint32_t *,unsigned *);
#endif
