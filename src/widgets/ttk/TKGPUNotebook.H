#ifndef TKGPU_NOTEBOOK_H
#define TKGPU_NOTEBOOK_H
#include "TKGPUUi.H"
int tg_notebook_command(tg_ui *,tg_widget *,size_t,char **,char *,size_t);
void tg_notebook_detach(tg_ui *,tg_widget *);
int tg_notebook_tab_height(const tg_ui *,const tg_widget *);
tg_rect tg_notebook_page_rect(const tg_widget *,tg_rect);
int tg_notebook_click(tg_ui *,tg_widget *,int,int);
int tg_notebook_step(tg_ui *,tg_widget *,int);
int tg_notebook_traverse(tg_ui *,uint32_t,unsigned);
void tg_notebook_paint(const tg_ui *,const tg_widget *,tg_canvas *,uint32_t);
#endif
