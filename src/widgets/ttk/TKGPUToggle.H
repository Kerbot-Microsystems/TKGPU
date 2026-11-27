#ifndef TKGPU_TOGGLE_H
#define TKGPU_TOGGLE_H
#include "TKGPUUi.H"
int tg_toggle_kind(tg_kind);
void tg_toggle_defaults(tg_widget *);
int tg_toggle_option(tg_widget *,const char *,const char *);
int tg_toggle_query(const tg_widget *,const char *,char *,size_t);
void tg_toggle_sync(tg_widget *,const char *);
int tg_ui_toggle_sync(tg_ui *,const char *,const char *);
int tg_toggle_diameter(const tg_ui *,const tg_widget *);
void tg_toggle_paint(const tg_ui *,const tg_widget *,tg_canvas *,uint32_t);
#endif
