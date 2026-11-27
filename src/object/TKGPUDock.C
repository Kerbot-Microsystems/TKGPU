#include "TKGPUDock.H"
#include <stdlib.h>
/* Coordinates are bounded by the command adapter; use double for squared distance. */
static int clamp(int value,int extent,int size) {
    int max=extent-size-4; if(max<4) max=4;
    return value<4?4:value>max?max:value;
}
const char *tg_dock_name(tg_dock_side side) {
    static const char *names[]={"none","top","left","right","bottom"};
    return side>=TG_DOCK_NONE&&side<=TG_DOCK_BOTTOM?names[side]:"none";
}
void tg_dock_reset(tg_dock *d,tg_dock_side side) {
    if(!d) return;
    *d=(tg_dock){0}; d->side=side;
}
void tg_dock_begin(tg_dock *d,int x,int y,int ox,int oy) {
    if(!d) return;
    d->active=1; d->can_snap=0; d->preview=TG_DOCK_NONE;
    d->start_x=x; d->start_y=y; d->offset_x=ox; d->offset_y=oy;
    d->x=x-ox; d->y=y-oy;
}
void tg_dock_move(tg_dock *d,int x,int y,int rw,int rh,int tw,int th,const int anchors[8]) {
    if(!d||!d->active||!anchors) return;
    d->x=x-d->offset_x; d->y=y-d->offset_y;
    if(rw>1&&rh>1) {
        d->x=clamp(d->x,rw,tw>0?tw:1);
        d->y=clamp(d->y,rh,th>0?th:1);
    }
    double dx=(double)x-d->start_x,dy=(double)y-d->start_y;
    if(dx*dx+dy*dy>3600.0) d->can_snap=1;
    d->preview=TG_DOCK_NONE;
    if(!d->can_snap) return;
    if(abs(d->y-anchors[1])<110&&abs(d->x-anchors[0])<200) { d->preview=TG_DOCK_TOP; return; }
    if(abs(d->x-anchors[2])<110) d->preview=TG_DOCK_LEFT;
    if(abs(d->x-anchors[4])<110) d->preview=TG_DOCK_RIGHT;
    if(abs(d->y-anchors[7])<110) d->preview=TG_DOCK_BOTTOM;
}
tg_dock_side tg_dock_end(tg_dock *d) {
    if(!d||!d->active) return TG_DOCK_NONE;
    tg_dock_side result=d->preview;
    if(result!=TG_DOCK_NONE) d->side=result;
    d->active=0; d->can_snap=0; d->preview=TG_DOCK_NONE;
    return result;
}
