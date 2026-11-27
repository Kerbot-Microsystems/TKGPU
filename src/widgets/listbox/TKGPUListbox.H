#ifndef TKGPU_LISTBOX_H
#define TKGPU_LISTBOX_H
#include "TKGPUUi.H"
typedef struct tg_listbox tg_listbox;
tg_listbox *tg_listbox_create(void);
tg_listbox *tg_listbox_clone(const tg_listbox *);
void tg_listbox_destroy(tg_listbox *);
int tg_listbox_command(tg_widget *,size_t,char **,char *,size_t);
void tg_listbox_view_fractions(const tg_widget *,double *,double *);
void tg_listbox_paint(const tg_widget *,tg_canvas *,uint32_t);
int tg_listbox_click(tg_widget *,int,int);
int tg_listbox_step(tg_widget *,int);
#endif
