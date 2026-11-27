#ifndef TKGPU_TREE_H
#define TKGPU_TREE_H
#include "TKGPUUi.H"
typedef struct tg_tree tg_tree;
tg_tree *tg_tree_create(void);
tg_tree *tg_tree_clone(const tg_tree *);
void tg_tree_destroy(tg_tree *);
int tg_tree_option(tg_tree *,const char *,const char *);
int tg_tree_command(const tg_ui *,tg_widget *,size_t,char **,char *,size_t);
int tg_tree_resize(const tg_ui *,tg_widget *,int,int,int);
int tg_tree_click(const tg_ui *,tg_widget *,int,int);
void tg_tree_paint(const tg_ui *,const tg_widget *,tg_canvas *,uint32_t);
void tg_tree_view_fractions(const tg_ui *,const tg_widget *,int,double *,double *);
#endif
