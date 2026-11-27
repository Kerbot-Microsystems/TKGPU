#ifndef TKGPU_EXTRA_H
#define TKGPU_EXTRA_H
#include "TKGPUUi.H"
void tg_extra_defaults(tg_widget *);
int tg_extra_option(tg_widget *,const char *,const char *);
int tg_extra_query(const tg_widget *,const char *,char *,size_t);
int tg_extra_command(tg_widget *,size_t,char **,char *,size_t);
void tg_extra_paint(const tg_ui *,const tg_widget *,tg_canvas *,uint32_t);
int tg_scale_thumb_size(const tg_widget *);
double tg_scale_fraction(const tg_widget *);
int tg_scale_set_value(tg_widget *,double);
int tg_scale_set_at(tg_widget *,int x,int y,int offset);
int tg_scale_step(tg_widget *,double delta);
int tg_scale_sync_widget(tg_widget *,const char *);
int tg_scale_sync(tg_ui *,const char *,const char *);
#endif
