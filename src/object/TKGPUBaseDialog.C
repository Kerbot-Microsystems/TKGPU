#include "../widgets/TKGPUWidgetInternal.H"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
static int bounded(const char *s,int *value) {
    char *end; errno=0; long v=strtol(s,&end,10);
    if(errno||end==s||*end||v<0||v>TG_DIM_LIMIT) return 0;
    *value=(int)v; return 1;
}
static int result(char *out,size_t cap,const char *s,int rc) {
    if(!cap) return TG_ERROR;
    int n=snprintf(out,cap,"%s",s);
    return n>=0&&(size_t)n<cap?rc:TG_ERROR;
}
static int descendant(tg_ui *u,tg_id id,tg_id ancestor) {
    while(id) {
        if(id==ancestor) return 1;
        const tg_widget *w=tg_ui_get(u,id); id=w?w->parent:0;
    }
    return 0;
}
static int limit(int64_t value,int min,int max) {
    if(max<min) max=min;
    return value<min?min:value>max?max:(int)value;
}
static int raise_widget(tg_ui *u,tg_widget *w) {
    if(!w) return 0;
    uint64_t max=0;
    for(size_t k=0;k<u->count;k++) if(u->items[k].parent==w->parent&&u->items[k].z_order>max)
        max=u->items[k].z_order;
    if(w->z_order==max&&max) return 0;
    if(max==UINT64_MAX) return 0;
    w->z_order=max+1; return 1;
}
static int front(tg_ui *u,tg_widget *w) {
    int changed=0;
    tg_widget *shadow=tg_lookup(u,w->dialog.shadow);
    if(shadow&&shadow->parent==w->parent&&shadow!=w) {
        uint64_t top_other=0;
        for(size_t k=0;k<u->count;k++) {
            const tg_widget *it=&u->items[k];
            if(it->parent==w->parent&&it->id!=w->id&&it->id!=shadow->id&&it->z_order>top_other)
                top_other=it->z_order;
        }
        if(shadow->z_order<=top_other||shadow->z_order>=w->z_order)
            changed|=raise_widget(u,shadow);
    }
    changed|=raise_widget(u,w);
    return changed;
}
#define TG_DIALOG_SHADOW_SPREAD 8
#define TG_DIALOG_SHADOW_DX 1
#define TG_DIALOG_SHADOW_DY 3
static void sync_shadow_rect(tg_ui *u,tg_widget *w) {
    tg_base_dialog *d=&w->dialog;
    tg_widget *shadow=tg_lookup(u,d->shadow);
    if(!shadow||shadow->parent!=w->parent||shadow==w) return;
    shadow->place_anchor=1; shadow->place_flags=3;
    shadow->layout=d->visible&&!d->minimized?TG_PLACE:TG_FREE;
    shadow->request=(tg_rect){
        w->request.x-TG_DIALOG_SHADOW_SPREAD+TG_DIALOG_SHADOW_DX,
        w->request.y-TG_DIALOG_SHADOW_SPREAD+TG_DIALOG_SHADOW_DY,
        d->width+2*TG_DIALOG_SHADOW_SPREAD,
        d->height+2*TG_DIALOG_SHADOW_SPREAD
    };
    if(!d->visible||d->minimized) shadow->rect=(tg_rect){0,0,0,0};
}
static void place(tg_widget *w,int x,int y) {
    tg_base_dialog *d=&w->dialog;
    w->layout=TG_PLACE;
    w->request=(tg_rect){x,y,d->width,d->height};
    w->place_anchor=1; w->place_flags=3;
    memset(w->place_rel,0,sizeof w->place_rel);
}
static void move(tg_ui *u,tg_widget *w,int64_t x,int64_t y) {
    const tg_widget *p=tg_ui_get(u,w->parent);
    if(!p) return;
    tg_base_dialog *d=&w->dialog;
    int inset=u->native_toplevels&&w->place_border_mode==0?p->border_width+p->highlight_width:0;
    place(w,limit(x,0,p->rect.w-2*inset-d->width),limit(y,d->minimum_y,p->rect.h-2*inset-d->height));
    sync_shadow_rect(u,w);
}
static void center(tg_ui *u,tg_widget *w) {
    const tg_widget *p=tg_ui_get(u,w->parent);
    if(!p) return;
    tg_base_dialog *d=&w->dialog;
    int y=(p->rect.h-d->height)/2;
    if(y<d->title_height) y=d->title_height+5;
    /* Embedded dialogs stay within their owner, including first show. */
    move(u,w,(p->rect.w-d->width)/2,y);
}
static void hide(tg_ui *u,tg_widget *w) {
    w->dialog.visible=0; w->dialog.dragging=0; w->dialog.resize_edges=0;
    if(u->dialog_capture==w->id) u->dialog_capture=0;
    if(descendant(u,u->pressed,w->id)) u->pressed=0;
    if(descendant(u,u->hover,w->id)) u->hover=0;
    if(descendant(u,u->focus,w->id)) u->focus=0;
    if(descendant(u,u->entry_drag,w->id)) u->entry_drag=0;
    if(descendant(u,u->drag_scroll,w->id)) u->drag_scroll=0;
    w->layout=TG_FREE; w->rect=(tg_rect){0,0,0,0};
    tg_widget *shadow=tg_lookup(u,w->dialog.shadow);
    if(shadow&&shadow->parent==w->parent) { shadow->layout=TG_FREE; shadow->rect=(tg_rect){0,0,0,0}; }
}
int tg_base_dialog_command(tg_ui *u,size_t n,char **a,char *out,size_t cap) {
    if(strcmp(a[0],"tkgpu::dialog")) return -1;
    if(n<3) return result(out,cap,"dialog needs widget and operation",TG_ERROR);
    tg_widget *w=tg_lookup(u,tg_ui_find(u,a[1]));
    if(!w||w->kind!=TG_FRAME) return result(out,cap,"invalid dialog frame",TG_ERROR);
    tg_base_dialog *d=&w->dialog;
    if(!strcmp(a[2],"attach")&&n==7) {
        int width,height,title,minimum;
        if(!bounded(a[3],&width)||!bounded(a[4],&height)||!bounded(a[5],&title)||
           !bounded(a[6],&minimum)||!width||!height||!title||title>height||minimum>TG_DIM_LIMIT) goto bad;
        if(u->dialog_capture==w->id) u->dialog_capture=0;
        *d=(tg_base_dialog){0}; d->enabled=1; d->width=width; d->height=height;
        d->restore_height=height; d->title_height=title; d->minimum_y=minimum;
        return result(out,cap,"",TG_OK);
    }
    if(!d->enabled) goto bad;
    if(!strcmp(a[2],"shadow")&&n==5) {
        tg_id id=tg_ui_find(u,a[3]); int offset;
        const tg_widget *s=tg_ui_get(u,id);
        if(!s||s->parent!=w->parent||id==w->id||!bounded(a[4],&offset)||offset>64) goto bad;
        d->shadow=id;
        sync_shadow_rect(u,w);
        front(u,w); return result(out,cap,"",TG_OK);
    }
    if(!strcmp(a[2],"title")&&n==4) {
        tg_id id=tg_ui_find(u,a[3]);
        if(!id||!descendant(u,id,w->id)) goto bad;
        d->title=id; return result(out,cap,"",TG_OK);
    }
    if(!strcmp(a[2],"size")&&n==5) {
        int width,height;
        if(!bounded(a[3],&width)||!bounded(a[4],&height)||!width||!height) goto bad;
        d->width=width; d->height=height;
        if(!d->minimized) d->restore_height=height;
        if(d->visible) move(u,w,w->request.x,w->request.y);
        return result(out,cap,"",TG_OK);
    }
    if(!strcmp(a[2],"resizable")&&n==9) {
        int rx,ry,min_w,min_h,max_w,max_h;
        if(!bounded(a[3],&rx)||!bounded(a[4],&ry)||
           !bounded(a[5],&min_w)||!bounded(a[6],&min_h)||
           !bounded(a[7],&max_w)||!bounded(a[8],&max_h)) goto bad;
        d->resize_x=rx?1:0; d->resize_y=ry?1:0;
        d->min_width=min_w>0?min_w:160;
        d->min_height=min_h>0?min_h:(d->title_height+48);
        d->max_width=max_w; d->max_height=max_h;
        if(!d->resize_x&&!d->resize_y) d->resize_edges=0;
        return result(out,cap,"",TG_OK);
    }
    if(!strcmp(a[2],"show")&&n==3) {
        d->visible=1; center(u,w); front(u,w); return result(out,cap,"",TG_OK);
    }
    if(!strcmp(a[2],"hide")&&n==3) { hide(u,w); return result(out,cap,"",TG_OK); }
    if(!strcmp(a[2],"front")&&n==3) { front(u,w); return result(out,cap,"",TG_OK); }
    if(!strcmp(a[2],"minimize")&&n==3) {
        if(!d->minimized) { d->restore_height=d->height; d->height=d->title_height+6; d->minimized=1; }
        else { d->height=d->restore_height; d->minimized=0; }
        if(d->visible) {
            if(u->native_toplevels) {
                place(w,w->request.x,w->request.y);
                sync_shadow_rect(u,w);
            } else move(u,w,w->request.x,w->request.y);
        }
        int z=snprintf(out,cap,"%d %d",d->minimized,d->height);
        return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR;
    }
    if(!strcmp(a[2],"state")&&n==3) {
        int z=snprintf(out,cap,"%d %d %d %d %d %d",d->visible,d->minimized,d->dragging,
                       w->request.x,w->request.y,d->height);
        return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR;
    }
bad:
    return result(out,cap,"invalid base dialog operation",TG_ERROR);
}
/* All native widget rectangles are in host coordinates. A pure dialog
   translation can move its existing subtree without requesting sizes or
   re-running the layout managers of unrelated windows. */
static void translate_tree(tg_ui *u,tg_widget *root,int dx,int dy) {
    size_t n=strlen(root->path);
    for(size_t k=0;k<u->count;k++) {
        tg_widget *p=&u->items[k];
        if(p!=root&&(strncmp(p->path,root->path,n)||p->path[n]!='.')) continue;
        const tg_widget *a=p;
        while(a&&a!=root&&a->kind!=TG_TOPLEVEL) a=tg_ui_get(u,a->parent);
        if(a!=root) continue;
        p->rect.x+=dx; p->rect.y+=dy;
    }
}
static int dialog_edge_hit(const tg_widget *w,int x,int y) {
    if(!w||!w->dialog.enabled||!w->dialog.visible||w->dialog.minimized) return 0;
    const tg_base_dialog *d=&w->dialog;
    if(!d->resize_x&&!d->resize_y) return 0;
    const int rx=w->rect.x,ry=w->rect.y,rw=w->rect.w,rh=w->rect.h;
    if(rw<=0||rh<=0) return 0;
    if(x<rx||x>=rx+rw||y<ry||y>=ry+rh) return 0;
    const int edge=7;
    int edges=0;
    if(d->resize_x) {
        if(x<rx+edge) edges|=1;
        else if(x>=rx+rw-edge) edges|=2;
    }
    if(d->resize_y) {
        if(y<ry+edge) edges|=4;
        else if(y>=ry+rh-edge) edges|=8;
    }
    return edges;
}
int tg_base_dialog_resize_hit(const tg_ui *u,int x,int y) {
    if(!u) return 0;
    const tg_widget *active=u->dialog_capture?tg_ui_get(u,u->dialog_capture):NULL;
    if(active&&active->dialog.resize_edges) return active->dialog.resize_edges;
    const tg_widget *w=tg_ui_get(u,tg_ui_hit((tg_ui *)u,x,y));
    while(w&&!w->dialog.enabled) w=tg_ui_get(u,w->parent);
    return dialog_edge_hit(w,x,y);
}
/* Native pointer processing: no Python callback per move. Existing button
   callbacks remain in the ordinary event path. */
int tg_base_dialog_event(tg_ui *u,tg_event e,int *consumed) {
    *consumed=0;
    if(e.kind==TG_FOCUS_OUT) {
        tg_widget *w=tg_lookup(u,u->dialog_capture);
        if(w) { w->dialog.dragging=0; w->dialog.resize_edges=0; }
        u->dialog_capture=0; return 0;
    }
    if(e.kind==TG_POINTER_MOVE||e.kind==TG_POINTER_UP) {
        tg_widget *w=tg_lookup(u,u->dialog_capture);
        if(!w||(!w->dialog.dragging&&!w->dialog.resize_edges)||!w->dialog.visible) { u->dialog_capture=0; return 0; }
        tg_base_dialog *d=&w->dialog;
        if(d->resize_edges) {
            const tg_widget *p=tg_ui_get(u,w->parent);
            if(!p) { d->resize_edges=0; u->dialog_capture=0; return 0; }
            int inset=u->native_toplevels&&w->place_border_mode==0?p->border_width+p->highlight_width:0;
            int max_pw=p->rect.w-2*inset;
            int max_ph=p->rect.h-2*inset;
            int dx=e.x-d->mouse_x,dy=e.y-d->mouse_y;
            int min_w=d->min_width>0?d->min_width:160;
            int min_h=d->min_height>0?d->min_height:(d->title_height+48);
            int max_w=d->max_width>0?d->max_width:TG_DIM_LIMIT;
            int max_h=d->max_height>0?d->max_height:TG_DIM_LIMIT;
            if(max_pw>min_w&&max_w>max_pw) max_w=max_pw;
            if(max_ph-d->minimum_y>min_h&&max_h>max_ph-d->minimum_y) max_h=max_ph-d->minimum_y;
            int nx=d->start_x,ny=d->start_y,nw=d->start_w,nh=d->start_h;
            if(d->resize_edges&2) {
                int avail=max_pw>0?max_pw-nx:max_w;
                int cap_w=avail<max_w?avail:max_w;
                nw=limit((int64_t)d->start_w+dx,min_w,cap_w);
            } else if(d->resize_edges&1) {
                int right=d->start_x+d->start_w;
                int cap_w=right<max_w?right:max_w;
                nw=limit((int64_t)d->start_w-dx,min_w,cap_w);
                nx=right-nw;
            }
            if(d->resize_edges&8) {
                int avail=max_ph>0?max_ph-ny:max_h;
                int cap_h=avail<max_h?avail:max_h;
                nh=limit((int64_t)d->start_h+dy,min_h,cap_h);
            } else if(d->resize_edges&4) {
                int bottom=d->start_y+d->start_h;
                int avail=bottom-d->minimum_y;
                int cap_h=avail<max_h?avail:max_h;
                nh=limit((int64_t)d->start_h-dy,min_h,cap_h);
                ny=bottom-nh;
            }
            int old_x=w->request.x,old_y=w->request.y;
            int changed=(nx!=old_x||ny!=old_y||nw!=d->width||nh!=d->height);
            d->width=nw; d->height=nh; d->restore_height=nh;
            place(w,nx,ny);
            sync_shadow_rect(u,w);
            if(e.kind==TG_POINTER_UP) { d->resize_edges=0; u->dialog_capture=0; }
            *consumed=1;
            if(changed) (void)tg_ui_post(u,(tg_event){TG_CONFIGURE,w->id,nw,nh,0});
            return changed?1:0;
        }
        int old_x=w->request.x,old_y=w->request.y;
        tg_widget *shadow=tg_lookup(u,d->shadow);
        int sx=shadow?shadow->request.x:0,sy=shadow?shadow->request.y:0;
        int fast=u->native_toplevels&&w->layout==TG_PLACE&&w->rect.w==d->width&&w->rect.h==d->height;
        if(shadow&&shadow->layout==TG_PLACE&&
           (shadow->rect.w!=shadow->request.w||shadow->rect.h!=shadow->request.h)) fast=0;
        move(u,w,(int64_t)d->start_x+e.x-d->mouse_x,(int64_t)d->start_y+e.y-d->mouse_y);
        if(e.kind==TG_POINTER_UP) { d->dragging=0; u->dialog_capture=0; }
        *consumed=1;
        int dx=w->request.x-old_x,dy=w->request.y-old_y;
        if(!dx&&!dy) return 0;
        if(fast) {
            translate_tree(u,w,dx,dy);
            if(shadow&&shadow->layout==TG_PLACE)
                translate_tree(u,shadow,shadow->request.x-sx,shadow->request.y-sy);
            (void)tg_ui_post(u,(tg_event){TG_CONFIGURE,w->id,w->rect.w,w->rect.h,0});
            return 2; /* Only paint invalidation is needed. */
        }
        return 1;
    }
    if(e.kind!=TG_POINTER_DOWN||(e.key&255)>1) return 0;
    const tg_widget *target=tg_ui_get(u,e.target);
    tg_widget *w=tg_lookup(u,e.target);
    while(w&&!w->dialog.enabled) w=tg_lookup(u,w->parent);
    if(!w) return 0;
    if(!w->dialog.visible) { *consumed=1; return 0; }
    int changed=front(u,w);
    tg_base_dialog *d=&w->dialog;
    int edges=dialog_edge_hit(w,e.x,e.y);
    if(edges) {
        d->resize_edges=edges; d->dragging=0;
        d->mouse_x=e.x; d->mouse_y=e.y;
        d->start_x=w->request.x; d->start_y=w->request.y;
        d->start_w=d->width; d->start_h=d->height;
        u->dialog_capture=w->id;
        *consumed=1; return 1;
    }
    if(!target||(target->kind!=TG_FRAME&&target->kind!=TG_LABEL&&target->kind!=TG_CANVAS)) return changed;
    if(e.target!=w->id&&(!d->title||!descendant(u,e.target,d->title))) return changed;
    /* Empty space in a title control must not start a drag. An explicit
       arrow cursor identifies the non-draggable control subtree. */
    for(const tg_widget *p=target;p&&p->id!=w->id;p=tg_ui_get(u,p->parent))
        if(!strcmp(p->cursor_name,"arrow")) return changed;
    d->dragging=1; d->mouse_x=e.x; d->mouse_y=e.y;
    d->start_x=w->request.x; d->start_y=w->request.y;
    u->dialog_capture=w->id;
    *consumed=1; return 1;
}
