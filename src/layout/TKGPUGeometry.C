#include "../widgets/ttk/TKGPUStyle.H"
#include <stdio.h>
#include "../widgets/ttk/TKGPUNotebook.H"
#include "../widgets/TKGPUWidgetInternal.H"
#include "../widgets/ttk/TKGPUContainers.H"
#include "../widgets/canvas/TKGPUCanvasItems.H"
#include "TKGPURaster.H"
static int min(int a,int b) { return a<b?a:b; }
static int max(int a,int b) { return a>b?a:b; }
int tg_ui_geometry(tg_ui *u,tg_id id,tg_layout layout,tg_rect q) {
    tg_widget *w=tg_lookup(u,id);
    if(!w||id==1||layout<TG_FREE||layout>TG_PACK_RIGHT||(layout!=TG_PLACE&&(q.w<0||q.h<0))||q.w< -TG_DIM_LIMIT||q.h< -TG_DIM_LIMIT||
       q.w>TG_DIM_LIMIT||q.h>TG_DIM_LIMIT||q.x< -TG_DIM_LIMIT||q.y< -TG_DIM_LIMIT||q.x>TG_DIM_LIMIT||q.y>TG_DIM_LIMIT||
       (layout!=TG_PLACE&&(q.x<0||q.y<0))) return TG_ERROR;
    if(layout==TG_GRID&&(q.x>=256||q.y>=256||q.w<1||q.h<1||q.w>256-q.x||q.h>256-q.y)) return TG_ERROR;
    if(layout!=TG_FREE&&layout!=TG_PLACE) for(size_t k=1;k<u->count;k++) {
        tg_widget *other=&u->items[k];
        if(other->id!=id&&other->parent==w->parent&&other->layout!=TG_FREE&&other->layout!=TG_PLACE&&
           ((other->layout==TG_GRID)!=(layout==TG_GRID))) return TG_ERROR;
    }
    if(layout==TG_GRID&&w->layout!=TG_GRID) w->grid_natural=w->layout==TG_PLACE&&u->native_toplevels?w->place_natural:w->request;
    if(layout==TG_PLACE&&w->layout!=TG_PLACE&&u->native_toplevels) w->place_natural=w->layout==TG_GRID?w->grid_natural:w->request;
    if(layout!=TG_GRID) w->grid_removed=0;
    w->layout=layout; w->request=q;
    if(w->dialog.enabled) {
        w->dialog.visible=layout==TG_PLACE;
        if(layout==TG_PLACE&&q.w>0&&q.h>0) {
            w->dialog.width=q.w; w->dialog.height=q.h;
            if(!w->dialog.minimized) w->dialog.restore_height=q.h;
        } else if(layout!=TG_PLACE) {
            w->dialog.dragging=0;
            if(u->dialog_capture==id) u->dialog_capture=0;
        }
    }
    return TG_OK;
}
static int vertical_pack(tg_layout l) { return l==TG_PACK_TOP||l==TG_PACK_BOTTOM; }
static int horizontal_pack(tg_layout l) { return l==TG_PACK_LEFT||l==TG_PACK_RIGHT; }
/* Conservative expansion: reserve requested sizes of remaining same-axis
   siblings; cross-axis parcels also reserve their requested minimum. */
static int expansion(const tg_ui *u,const size_t *order,size_t count_siblings,size_t start,int vertical,int extent) {
    int spare=extent,count=0,share=extent;
    for(size_t k=start;k<count_siblings;k++) {
        const tg_widget *w=&u->items[order[k]];
        int same=vertical?vertical_pack(w->layout):horizontal_pack(w->layout);
        int cross=vertical?horizontal_pack(w->layout):vertical_pack(w->layout);
        int req=vertical?w->request.h+(2*w->pack_pady+w->pack_pady_extra)+2*w->pack_ipady:w->request.w+(2*w->pack_padx+w->pack_padx_extra)+2*w->pack_ipadx;
        if(same) { spare-=req; if(w->pack_expand) count++; }
        else if(cross&&count) share=min(share,max(0,spare-req)/count);
    }
    return count?min(share,max(0,spare)/count):0;
}
static tg_rect fit_pack(const tg_ui *u,const tg_widget *w,tg_rect p) {
    int px=min(w->pack_padx,p.w),py=min(w->pack_pady,p.h);
    int right=min(w->pack_padx+w->pack_padx_extra,p.w-px);
    int bottom=min(w->pack_pady+w->pack_pady_extra,p.h-py);
    p.x+=px; p.y+=py; p.w-=px+right; p.h-=py+bottom;
    /* Zero preserves the original preview's cross-axis fill default when unanchored.
       Explicit fill uses bit flags plus one: none=1,x=2,y=3,both=4. */
    int bits=w->pack_fill?w->pack_fill-1:(u->native_toplevels||w->pack_anchor?0:(vertical_pack(w->layout)?1:2));
    int width=(bits&1)?p.w:min(p.w,w->request.w+2*w->pack_ipadx);
    int height=(bits&2)?p.h:min(p.h,w->request.h+2*w->pack_ipady);
    int anchor=w->pack_anchor?w->pack_anchor-1:4;
    return (tg_rect){p.x+(p.w-width)*(anchor%3)/2,p.y+(p.h-height)*(anchor/3)/2,width,height};
}
static void grid_offsets(const tg_ui *u,tg_id id,int rows,int count,int extent,int *offsets) {
    const tg_widget *parent=tg_ui_get(u,id);
    const uint16_t *weights=rows?parent->grid_row_weight:parent->grid_col_weight;
    const uint16_t *mins=rows?parent->grid_row_min:parent->grid_col_min;
    const uint16_t *pads=rows?parent->grid_row_pad:parent->grid_col_pad;
    int sizes[256]={0},configured=0,total=0,weight=0;
    for(int k=0;k<count;k++) { sizes[k]=mins[k]; configured|=mins[k]||weights[k]||pads[k]; weight+=weights[k]; }
    /* Preserve prior equal-cell preview when no tracks are configured. */
    if(!configured&&!u->native_toplevels&&extent>=0) { for(int k=0;k<=count;k++) offsets[k]=extent*k/count; return; }
    /* Resolve single-track requirements before progressively longer spans. */
    for(int length=1;length<=count;length++) for(size_t k=1;k<u->count;k++) {
        const tg_widget *w=&u->items[k]; if(w->parent!=id||w->layout!=TG_GRID) continue;
        int first=rows?w->request.y:w->request.x,span=rows?w->request.h:w->request.w;
        if(span!=length) continue;
        int want=(rows?w->grid_natural.h:w->grid_natural.w)+(rows?2*w->grid_pady+w->grid_pady_extra+2*w->grid_ipady:2*w->grid_padx+w->grid_padx_extra+2*w->grid_ipadx);
        if(span==1) want+=pads[first];
        int have=0,sw=0;
        for(int j=first;j<first+span;j++) { have+=sizes[j]; sw+=weights[j]; }
        int deficit=max(0,want-have),prev=0,part=0;
        for(int j=first;j<first+span;j++) {
            part+=sw?weights[j]:1;
            int next=(int)((int64_t)deficit*part/(sw?sw:span));
            sizes[j]+=next-prev; prev=next;
        }
    }
    const uint16_t *uniforms=rows?parent->grid_row_uniform:parent->grid_col_uniform;
    for(unsigned group=1;group<=u->uniform_count;group++) {
        int unit=0;
        for(int k=0;k<count;k++) if(uniforms[k]==group) { int w=weights[k]?weights[k]:1; unit=max(unit,(sizes[k]+w-1)/w); }
        if(unit) for(int k=0;k<count;k++) if(uniforms[k]==group) sizes[k]=unit*(weights[k]?weights[k]:1);
    }
    for(int k=0;k<count;k++) total+=sizes[k];
    if(extent<0) extent=total;
    if(total>extent&&u->native_toplevels) {
        int remaining=total-extent;
        while(remaining>0) {
            int active=0,limit=remaining;
            for(int k=0;k<count;k++) if(weights[k]&&sizes[k]>mins[k]) active+=weights[k];
            if(!active) break;
            for(int k=0;k<count;k++) if(weights[k]&&sizes[k]>mins[k]) {
                int64_t cap=(int64_t)(sizes[k]-mins[k])*active/weights[k];
                if(cap<limit) limit=(int)cap;
            }
            if(limit<1) limit=1;
            int cumulative=0,applied=0,previous=0;
            for(int k=0;k<count;k++) if(weights[k]&&sizes[k]>mins[k]) {
                cumulative+=weights[k];
                int next=(int)((int64_t)limit*cumulative/active);
                int take=min(sizes[k]-mins[k],next-previous);
                sizes[k]-=take; applied+=take; previous=next;
            }
            if(!applied) break;
            remaining-=applied; total-=applied;
        }
    }
    int extra=max(0,extent-total),cumulative=0,used=0,base=0;
    int anchor=parent->grid_anchor?parent->grid_anchor-1:0;
    int origin=weight?0:extra*(rows?anchor/3:anchor%3)/2;
    offsets[0]=origin;
    for(int k=0;k<count;k++) {
        base+=sizes[k];
        cumulative+=weights[k];
        if(total>extent&&total&&!u->native_toplevels) used=(int)((int64_t)base*extent/total);
        else used=base+(weight?(int)((int64_t)extra*cumulative/weight):0);
        offsets[k+1]=origin+used;
    }
}
static tg_rect grid_fit(const tg_ui *u,const tg_widget *w,tg_rect r) {
    int px=min(w->grid_padx,r.w),py=min(w->grid_pady,r.h);
    int right=min(w->grid_padx+w->grid_padx_extra,r.w-px);
    int bottom=min(w->grid_pady+w->grid_pady_extra,r.h-py);
    r.x+=px; r.y+=py; r.w-=px+right; r.h-=py+bottom;
    if(!w->grid_sticky&&!u->native_toplevels) return r;
    int flags=w->grid_sticky;
    int width=(flags&24)==24?r.w:min(r.w,w->grid_natural.w+2*w->grid_ipadx);
    int height=(flags&6)==6?r.h:min(r.h,w->grid_natural.h+2*w->grid_ipady);
    int x=(flags&8)?0:(flags&16)?r.w-width:(r.w-width)/2;
    int y=(flags&2)?0:(flags&4)?r.h-height:(r.h-height)/2;
    return (tg_rect){r.x+x,r.y+y,width,height};
}
static void container_insets(const tg_ui *u,const tg_widget *w,int *left,int *top,int *right,int *bottom) {
    int border=w->border_width+w->highlight_width;
    *left=*top=*right=*bottom=border;
    if(!u->native_toplevels||(w->kind!=TG_FRAME&&w->kind!=TG_LABELFRAME)) return;
    *left+=w->text_padx; *right+=w->text_padx;
    *top+=w->text_pady; *bottom+=w->text_pady;
    if(w->kind==TG_LABELFRAME) {
        *top+=16; *bottom+=4; *left+=4; *right+=4;
    }
    if(w->has_ttk_padding) {
        *left+=w->ttk_padding[0]; *top+=w->ttk_padding[1]; *right+=w->ttk_padding[2]; *bottom+=w->ttk_padding[3];
    } else if(w->themed) {
        char value[384]; int p[4]={0};
        const char *style=w->style[0]?w->style:w->kind==TG_LABELFRAME?"TLabelframe":"TFrame";
        if(tg_style_lookup_value(u,style,"-padding",0,0,value,sizeof value)>0) {
            int n=sscanf(value,"%d %d %d %d",&p[0],&p[1],&p[2],&p[3]);
            if(n>0) {
                for(int k=0;k<n;k++) p[k]=max(0,min(p[k],TG_DIM_LIMIT/4));
                *left+=p[0]; *top+=n>1?p[1]:p[0]; *right+=n>2?p[2]:p[0]; *bottom+=n>3?p[3]:n>1?p[1]:p[0];
            }
        }
    }
}
static int rounded(double v) { return (int)(v>=0?v+0.5:v-0.5); }
static tg_rect fit_place(const tg_ui *u,const tg_widget *parent,const tg_widget *w,tg_rect area) {
    if(u->native_toplevels&&w->place_border_mode==0) {
        int l,t,r,b; container_insets(u,parent,&l,&t,&r,&b);
        area.x+=l; area.y+=t; area.w=max(0,area.w-l-r); area.h=max(0,area.h-t-b);
    }
    tg_rect q=w->request;
    unsigned f=w->place_flags;
    int rx=rounded(area.w*w->place_rel[0]),ry=rounded(area.h*w->place_rel[1]);
    int rw=u->native_toplevels?rounded(area.w*(w->place_rel[0]+w->place_rel[2]))-rx:rounded(area.w*w->place_rel[2]);
    int rh=u->native_toplevels?rounded(area.h*(w->place_rel[1]+w->place_rel[3]))-ry:rounded(area.h*w->place_rel[3]);
    int width=(f&4)?rw+((f&1)?q.w:0):q.w;
    int height=(f&8)?rh+((f&2)?q.h:0):q.h;
    width=max(0,min(width,TG_DIM_LIMIT)); height=max(0,min(height,TG_DIM_LIMIT));
    int anchor=w->place_anchor?w->place_anchor-1:0;
    int x=q.x+rounded(area.w*w->place_rel[0])-width*(anchor%3)/2;
    int y=q.y+rounded(area.h*w->place_rel[1])-height*(anchor/3)/2;
    if(u->native_toplevels) return (tg_rect){area.x+x,area.y+y,width,height};
    int l=max(0,min(x,area.w)),t=max(0,min(y,area.h));
    int r=max(l,min(x+width,area.w)),b=max(t,min(y+height,area.h));
    return (tg_rect){area.x+l,area.y+t,r-l,b-t};
}
static void arrange(tg_ui *u,tg_id id,tg_rect area) {
    tg_widget *parent=tg_lookup(u,id);
    if(!parent) return;
    tg_canvas_view_clamp(parent);
    if(!parent->child_count) return;
    if(parent->kind==TG_PANEDWINDOW) {
        for(size_t k=1;k<u->count;k++) { tg_widget *w=&u->items[k]; if(w->parent!=id) continue; w->rect=tg_pane_rect(u,parent,w->id); arrange(u,w->id,w->rect); }
        return;
    }
    if(parent&&parent->kind==TG_NOTEBOOK) {
        int top=min(area.h,tg_notebook_tab_height(u,parent)); area.y+=top; area.h-=top;
        for(size_t k=1;k<u->count;k++) {
            tg_widget *w=&u->items[k]; if(w->parent!=id) continue;
            w->rect=w->id==parent->selected&&w->tab_state==1?tg_notebook_page_rect(w,area):(tg_rect){area.x,area.y,0,0};
            arrange(u,w->id,w->rect);
        }
        return;
    }
    tg_rect free_area=area;
    int inset_x,inset_y,inset_right,inset_bottom;
    container_insets(u,parent,&inset_x,&inset_y,&inset_right,&inset_bottom);
    if(parent->kind!=TG_CANVAS&&parent->kind!=TG_PANEDWINDOW) {
        free_area.x+=inset_x; free_area.y+=inset_y;
        free_area.w=max(0,free_area.w-inset_x-inset_right);
        free_area.h=max(0,free_area.h-inset_y-inset_bottom);
    }
    size_t order[TG_WIDGET_LIMIT],siblings=tg_sibling_order(u,id,order);
    int cols=0,rows=0;
    for(size_t k=0;k<siblings;k++) {
        const tg_widget *w=&u->items[order[k]];
        if(w->layout==TG_GRID) {
            cols=max(cols,w->request.x+w->request.w); rows=max(rows,w->request.y+w->request.h);
        }
    }
    if(cols||rows) for(int k=0;k<256;k++) {
        if(parent->grid_col_weight[k]||parent->grid_col_min[k]||parent->grid_col_pad[k]) cols=max(cols,k+1);
        if(parent->grid_row_weight[k]||parent->grid_row_min[k]||parent->grid_row_pad[k]) rows=max(rows,k+1);
    }
    int xoff[257],yoff[257];
    if(cols&&rows) { grid_offsets(u,id,0,cols,free_area.w,xoff); grid_offsets(u,id,1,rows,free_area.h,yoff); }
    for(size_t k=0;k<siblings;k++) {
        tg_widget *w=&u->items[order[k]];
        if(u->native_toplevels&&w->kind==TG_TOPLEVEL) continue;
        tg_rect q=w->request,r={area.x,area.y,0,0};
        if(vertical_pack(w->layout)) {
            int extra=w->pack_expand?expansion(u,order,siblings,k,1,free_area.h):0;
            int height=min(q.h+(2*w->pack_pady+w->pack_pady_extra)+2*w->pack_ipady+extra,free_area.h);
            int bottom=w->layout==TG_PACK_BOTTOM;
            tg_rect parcel={free_area.x,bottom?free_area.y+free_area.h-height:free_area.y,free_area.w,height};
            if(!bottom) free_area.y+=height;
            free_area.h-=height; r=fit_pack(u,w,parcel);
        } else if(horizontal_pack(w->layout)) {
            int extra=w->pack_expand?expansion(u,order,siblings,k,0,free_area.w):0;
            int width=min(q.w+(2*w->pack_padx+w->pack_padx_extra)+2*w->pack_ipadx+extra,free_area.w);
            int right=w->layout==TG_PACK_RIGHT;
            tg_rect parcel={right?free_area.x+free_area.w-width:free_area.x,free_area.y,width,free_area.h};
            if(!right) free_area.x+=width;
            free_area.w-=width; r=fit_pack(u,w,parcel);
        } else if(w->layout==TG_GRID) {
            int x=xoff[q.x],y=yoff[q.y];
            r=grid_fit(u,w,(tg_rect){area.x+inset_x+x,area.y+inset_y+y,xoff[q.x+q.w]-x,yoff[q.y+q.h]-y});
        } else if(w->layout==TG_PLACE) {
            r=fit_place(u,parent,w,area);
        } else if(parent->kind==TG_CANVAS&&w->layout==TG_FREE) {
            const tg_canvas_item *item=tg_canvas_window_item(u,id,w->id);
            tg_rect bounds;
            if(item&&!item->hidden&&tg_canvas_item_bounds(u,item,&bounds)) {
                bounds.x+=area.x-parent->canvas_x; bounds.y+=area.y-parent->canvas_y;
                /* Keep the embedded window's full geometry; painting and hit
                   testing clip it to the canvas viewport, not layout. */
                r=bounds;
            }
        }
        w->rect=r;
        arrange(u,w->id,r);
    }
}
static void native_requests(tg_ui *u) {
    for(size_t at=u->count;at>0;at--) {
        tg_widget *p=&u->items[at-1];
        if(p->kind!=TG_FRAME&&p->kind!=TG_LABELFRAME&&p->kind!=TG_TOPLEVEL) continue;
        if(!p->child_count||(p->no_pack_propagate&&p->no_grid_propagate)) continue;
        size_t order[TG_WIDGET_LIMIT],count=tg_sibling_order(u,p->id,order);
        int width=0,height=0,found=0;
        for(size_t k=count;k>0;k--) {
            tg_widget *c=&u->items[order[k-1]];
            if(c->kind==TG_TOPLEVEL||c->layout==TG_FREE||c->layout==TG_PLACE||c->layout==TG_GRID) continue;
            if(p->no_pack_propagate) continue;
            int cw=c->request.w+2*c->pack_padx+c->pack_padx_extra+2*c->pack_ipadx,ch=c->request.h+2*c->pack_pady+c->pack_pady_extra+2*c->pack_ipady;
            if(vertical_pack(c->layout)) { width=max(width,cw); height+=ch; }
            else { width+=cw; height=max(height,ch); }
            found=1;
        }
        if(!p->no_grid_propagate) {
            int cols=0,rows=0;
            for(size_t k=0;k<count;k++) {
                const tg_widget *c=&u->items[order[k]];
                if(c->layout!=TG_GRID) continue;
                cols=max(cols,c->request.x+c->request.w);
                rows=max(rows,c->request.y+c->request.h);
            }
            for(int k=0;k<256;k++) {
                if(p->grid_col_weight[k]||p->grid_col_min[k]||p->grid_col_pad[k]) cols=max(cols,k+1);
                if(p->grid_row_weight[k]||p->grid_row_min[k]||p->grid_row_pad[k]) rows=max(rows,k+1);
            }
            if(cols&&rows) {
                int xo[257],yo[257];
                grid_offsets(u,p->id,0,cols,-1,xo);
                grid_offsets(u,p->id,1,rows,-1,yo);
                width=xo[cols]; height=yo[rows]; found=1;
            }
        }
        if(found) {
            tg_rect *r=p->layout==TG_GRID?&p->grid_natural:&p->request;
            int l,t,right,bottom; container_insets(u,p,&l,&t,&right,&bottom);
            int rw=min(TG_DIM_LIMIT,width+l+right),rh=min(TG_DIM_LIMIT,height+t+bottom);
            if(p->layout==TG_PLACE) { p->place_natural.w=rw; p->place_natural.h=rh; }
            if(!(p->layout==TG_PLACE&&(p->place_flags&1))) r->w=rw;
            if(!(p->layout==TG_PLACE&&(p->place_flags&2))) r->h=rh;
        }
    }
}
int tg_ui_arrange(tg_ui *u,int w,int h) {
    if(!u||w<0||h<0||w>TG_DIM_LIMIT||h>TG_DIM_LIMIT) return TG_ERROR;
    if(u->native_toplevels) native_requests(u);
    tg_id id=u->view_root?u->view_root:1;
    tg_widget *root=tg_lookup(u,id); if(!root) return TG_ERROR;
    tg_rect before[TG_WIDGET_LIMIT];
    if(u->native_toplevels) for(size_t k=0;k<u->count;k++) before[k]=u->items[k].rect;
    root->rect=(tg_rect){0,0,w,h}; arrange(u,id,root->rect);
    if(u->native_toplevels) for(size_t k=0;k<u->count;k++) {
        tg_widget *widget=&u->items[k];
        if(widget->id==id) continue;
        int dx=widget->rect.x-before[k].x,dy=widget->rect.y-before[k].y;
        /* Tk Configure is relative to the parent, not the native host.
           Translating an entire dialog does not reconfigure its children. */
        if(dx||dy) {
            const tg_widget *parent=tg_ui_get(u,widget->parent);
            if(parent) {
                size_t p=(size_t)(parent-u->items);
                dx-=parent->rect.x-before[p].x;
                dy-=parent->rect.y-before[p].y;
            }
        }
        if(dx||dy||before[k].w!=widget->rect.w||before[k].h!=widget->rect.h)
            (void)tg_ui_post(u,(tg_event){TG_CONFIGURE,widget->id,widget->rect.w,widget->rect.h,0});
    }
    return TG_OK;
}
