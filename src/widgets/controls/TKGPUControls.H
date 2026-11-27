#ifndef TKGPU_CONTROLS_H
#define TKGPU_CONTROLS_H
#include "TKGPUUi.H"
int tg_control_option(tg_widget *,const char *,const char *);
int tg_control_query(const tg_widget *,const char *,char *,size_t);
int tg_entry_command(tg_widget *,size_t,char **,char *,size_t);
int tg_entry_index(const tg_widget *,const char *,size_t *);
size_t tg_entry_offset(const tg_widget *,size_t);
int tg_entry_caret_rect(const tg_widget *,tg_rect *);
int tg_entry_replace_bytes(tg_widget *,size_t,size_t,const char *);
enum { TG_ENTRY_DISPLAY_CAP=4096 };
const char *tg_entry_display_text(const tg_widget *,char [TG_ENTRY_DISPLAY_CAP]);
int tg_entry_edit(tg_widget *,uint32_t);
int tg_entry_shift(tg_widget *,uint32_t);
int tg_entry_drag(tg_widget *,int);
int tg_entry_click(tg_widget *,int x);
int tg_textvar_kind(tg_kind);
int tg_textvar_editable(tg_kind);
int tg_textvar_matches(const tg_widget *,const char *);
int tg_textvar_valid(const char *);
int tg_textvar_sync(tg_ui *,const char *,const char *);
#endif
