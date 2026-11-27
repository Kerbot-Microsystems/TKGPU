#ifndef TKGPU_INDICATORS_H
#define TKGPU_INDICATORS_H
#include "TKGPUUi.H"
void tg_indicator_defaults(tg_widget *);
int tg_indicator_option(tg_widget *,const char *,const char *);
int tg_indicator_query(const tg_widget *,const char *,char *,size_t);
int tg_indicator_command(tg_widget *,size_t,char **,char *,size_t);
void tg_indicator_paint(const tg_widget *,tg_canvas *,uint32_t);
#endif
