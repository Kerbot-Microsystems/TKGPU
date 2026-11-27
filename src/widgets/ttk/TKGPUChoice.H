#ifndef TKGPU_CHOICE_H
#define TKGPU_CHOICE_H
#include "TKGPUUi.H"
int tg_choice_kind(tg_kind);
void tg_choice_defaults(tg_widget *);
int tg_choice_option(tg_widget *,const char *,const char *);
int tg_choice_query(const tg_widget *,const char *,char *,size_t);
int tg_choice_command(tg_widget *,size_t,char **,char *,size_t);
int tg_choice_step(tg_widget *,int);
void tg_choice_paint(const tg_widget *,tg_canvas *,uint32_t);
tg_id tg_choice_trigger(tg_ui *,tg_event);
int tg_choice_open(tg_ui *,tg_id);
int tg_choice_popup_input(tg_ui *,tg_event,tg_id *);
tg_id tg_choice_popup_hit(const tg_ui *,int,int);
void tg_choice_popup_paint(const tg_ui *,tg_canvas *);
#endif
