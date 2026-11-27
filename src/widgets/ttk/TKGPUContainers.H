#ifndef TKGPU_CONTAINERS_H
#define TKGPU_CONTAINERS_H
#include "TKGPUUi.H"
typedef struct tg_menu tg_menu;
tg_menu *tg_menu_create(void);
tg_menu *tg_menu_clone(const tg_menu *);
void tg_menu_destroy(tg_menu *);
void tg_container_defaults(tg_widget *);
int tg_container_option(tg_widget *,const char *,const char *);
int tg_container_query(const tg_widget *,const char *,char *,size_t);
int tg_container_command(tg_ui *,tg_widget *,size_t,char **,char *,size_t);
void tg_container_detach(tg_ui *,tg_widget *);
tg_rect tg_pane_rect(const tg_ui *,const tg_widget *,tg_id);
int tg_container_input(tg_ui *,tg_event,char *,size_t);
int tg_menu_input(tg_ui *,tg_event,char *,size_t);
int tg_menu_script(const tg_widget *,const char *,char *,size_t);
typedef struct { char variable[TG_WIDGET_PATH_CAP],value[128],command[512]; } tg_menu_action;
int tg_menu_prepare_action(const tg_widget *,const char *,tg_menu_action *);
int tg_menu_prepare_row(const tg_widget *,unsigned,tg_menu_action *);
int tg_menu_binding(const tg_widget *,unsigned,tg_menu_action *);
int tg_menu_sync_widget(tg_widget *,const char *,const char *);
void tg_container_paint(const tg_widget *,tg_canvas *,uint32_t);
void tg_menu_paint(const tg_ui *,tg_canvas *);
tg_id tg_menu_hit(const tg_ui *,int,int);
int tg_menu_mapped(const tg_ui *,tg_id);
#endif
