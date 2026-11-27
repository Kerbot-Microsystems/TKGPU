#ifndef TKGPU_DOCK_H
#define TKGPU_DOCK_H
/* Value object: owned by its widget, copied with the scene, no heap or OS dependencies. */
typedef enum { TG_DOCK_NONE=0,TG_DOCK_TOP,TG_DOCK_LEFT,TG_DOCK_RIGHT,TG_DOCK_BOTTOM } tg_dock_side;
typedef struct { int active,can_snap,offset_x,offset_y,start_x,start_y,x,y; tg_dock_side side,preview; } tg_dock;
void tg_dock_begin(tg_dock *,int,int,int,int);
void tg_dock_move(tg_dock *,int,int,int,int,int,int,const int [8]);
tg_dock_side tg_dock_end(tg_dock *);
void tg_dock_reset(tg_dock *,tg_dock_side);
const char *tg_dock_name(tg_dock_side);
#endif
