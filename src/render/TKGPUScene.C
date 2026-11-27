#include "../widgets/TKGPUWidgetInternal.H"
#include "../widgets/controls/TKGPUControls.H"
#include "../widgets/image/TKGPUImageRegistry.H"
#include "../widgets/listbox/TKGPUListbox.H"
#include "../widgets/text/TKGPUTextWidget.H"
#include "../widgets/canvas/TKGPUCanvasItems.H"
#include "../widgets/ttk/TKGPUIndicators.H"
#include "../widgets/ttk/TKGPUExtra.H"
#include "../widgets/ttk/TKGPUChoice.H"
#include "../widgets/ttk/TKGPUNotebook.H"
#include "../widgets/ttk/TKGPUToggle.H"
#include "../widgets/ttk/TKGPUContainers.H"
#include "../widgets/ttk/tree/TKGPUTree.H"
#include "TKGPUText.H"
#include "TKGPURaster.H"
#include <string.h>
#include <stdlib.h>
#include "TKGPUUtf8.H"
#include "parallel/TKGPUParallelPaint.H"
#include "TKGPUFrameBufferInternal.H"
/* IDs remain sorted by insertion order; deletion preserves relative order. */
typedef struct { const tg_ui *u; size_t count; unsigned order[TG_WIDGET_LIMIT],end[TG_WIDGET_LIMIT]; } plan;
static unsigned parent_index(const tg_ui *u,tg_id id) {
    size_t l=0,r=u->count;
    while(l<r) { size_t m=l+(r-l)/2; if(u->items[m].id<id) l=m+1; else r=m; }
    return l<u->count&&u->items[l].id==id?(unsigned)l:TG_WIDGET_LIMIT;
}
static void append(plan *p,const unsigned *first,const unsigned *next,unsigned k) {
    for(;k<TG_WIDGET_LIMIT;k=next[k]) {
        size_t at=p->count; p->order[p->count++]=k;
        append(p,first,next,first[k]); p->end[at]=(unsigned)p->count;
    }
}
static void prepare(plan *p,const tg_ui *u) {
    unsigned first[TG_WIDGET_LIMIT],next[TG_WIDGET_LIMIT];
    p->u=u; p->count=0;
    for(size_t k=0;k<u->count;k++) first[k]=next[k]=TG_WIDGET_LIMIT;
    /* Preserve ID order for ordinary widgets; raised siblings paint last. */
    for(size_t k=u->count;k>1;k--) {
        if(u->native_toplevels&&u->items[k-1].kind==TG_TOPLEVEL) continue;
        /* An unmapped or zero-sized widget cannot expose any descendants.
           In particular, forgotten frames can still contain canvas windows
           with nonzero item bounds. Do not attach that subtree to the plan. */
        if(u->items[k-1].rect.w<=0||u->items[k-1].rect.h<=0) continue;
        unsigned index=(unsigned)(k-1),parent=parent_index(u,u->items[index].parent);
        if(parent<index) {
            unsigned *slot=&first[parent];
            while(*slot<TG_WIDGET_LIMIT&&u->items[*slot].z_order<u->items[index].z_order)
                slot=&next[*slot];
            next[index]=*slot; *slot=index;
        }
    }
    if(u->count) { unsigned root=parent_index(u,u->view_root?u->view_root:1); if(root<TG_WIDGET_LIMIT) append(p,first,next,root); }
}
/* Tk widgets retain their full geometry even if partly outside their
   parent. Clip drawing, not the widget's layout or requested dimensions. */
static tg_rect ancestor_clip(const tg_ui *u,const tg_widget *w,tg_rect band) {
    if(!u->native_toplevels) return band;
    tg_id root=u->view_root?u->view_root:1;
    while(w&&w->id!=root) {
        w=tg_ui_get(u,w->parent);
        tg_rect q;
        if(!w||!tg_rect_intersect(band,w->rect,&q)) return (tg_rect){0,0,0,0};
        band=q;
    }
    return band;
}
static int paint_range(const plan *p,tg_canvas *c,size_t first,size_t last,size_t bypass,size_t budget) {
    const tg_ui *u=p->u;
    tg_rect band=tg_canvas_render_bounds(c),saved_clip=c->clip;
    int saved_active=c->clip_active;
    uint32_t *under=NULL; size_t capacity=0;
    for(size_t k=first;k<last;k++) {
        c->clip_active=saved_active; c->clip=saved_clip;
        const tg_widget *w=&u->items[p->order[k]]; tg_rect visible;
        if(k!=bypass&&w->group_opacity<255) {
            size_t end=p->end[k];
            if(!w->group_opacity) { k=end-1; continue; }
            /* Include descendants even when they extend beyond the parent. */
            tg_rect bounds={0,0,0,0};
            for(size_t j=k;j<end;j++) {
                const tg_widget *child=&u->items[p->order[j]]; tg_rect q;
                if(child->kind==TG_MENU||!tg_rect_intersect(ancestor_clip(u,child,band),child->rect,&q)) continue;
                if(!bounds.w) bounds=q;
                else {
                    int x=bounds.x<q.x?bounds.x:q.x,y=bounds.y<q.y?bounds.y:q.y;
                    int r=bounds.x+bounds.w>q.x+q.w?bounds.x+bounds.w:q.x+q.w;
                    int b=bounds.y+bounds.h>q.y+q.h?bounds.y+bounds.h:q.y+q.h;
                    bounds=(tg_rect){x,y,r-x,b-y};
                }
            }
            if(bounds.w&&bounds.h) {
                if(c->sink) {
                    if(!c->layer_sink||c->layer_sink(c->sink_context,bounds,w->group_opacity,1)!=TG_OK) { free(under); return TG_ERROR; }
                    int rc=paint_range(p,c,k,end,k,budget);
                    if(rc!=TG_OK||c->sink_failed||c->layer_sink(c->sink_context,bounds,w->group_opacity,0)!=TG_OK) { free(under); return TG_ERROR; }
                    k=end-1; continue;
                }
                size_t pixels=(size_t)bounds.w*bounds.h;
                if(pixels>budget||pixels>SIZE_MAX/sizeof(uint32_t)) { free(under); return TG_ERROR; }
                uint32_t *saved=malloc(pixels*sizeof *saved);
                if(!saved) { free(under); return TG_ERROR; }
                for(int y=0;y<bounds.h;y++)
                    memcpy(saved+(size_t)y*bounds.w,c->p+(size_t)(bounds.y+y)*c->w+bounds.x,(size_t)bounds.w*sizeof *saved);
                int rc=paint_range(p,c,k,end,k,budget-pixels);
                if(rc==TG_OK) for(int y=0;y<bounds.h;y++) {
                    uint32_t *dst=c->p+(size_t)(bounds.y+y)*c->w+bounds.x;
                    const uint32_t *src=saved+(size_t)y*bounds.w;
                    for(int x=0;x<bounds.w;x++) dst[x]=tg_pixel_blend(src[x],((dst[x]&0xffffffu)<<8)|w->group_opacity);
                }
                free(saved);
                if(rc!=TG_OK) { free(under); return rc; }
            }
            k=end-1; continue;
        }
        tg_rect clip=ancestor_clip(u,w,band);
        if(!tg_rect_intersect(clip,w->rect,&visible)) continue;
        if(w->kind==TG_MENU||!w->opacity) continue;
        if(u->native_toplevels) { c->clip_active=1; c->clip=visible; }
        if(w->opacity<255&&c->sink) {
            if(!c->layer_sink||c->layer_sink(c->sink_context,visible,w->opacity,1)!=TG_OK) { free(under); return TG_ERROR; }
        } else if(w->opacity<255) {
            size_t pixels=(size_t)visible.w*visible.h;
            if(pixels>SIZE_MAX/sizeof *under) { free(under); return TG_ERROR; }
            if(pixels>capacity) {
                uint32_t *next=realloc(under,pixels*sizeof *under);
                if(!next) { free(under); return TG_ERROR; }
                under=next; capacity=pixels;
            }
            for(int y=0;y<visible.h;y++)
                memcpy(under+(size_t)y*visible.w,c->p+(size_t)(visible.y+y)*c->w+visible.x,(size_t)visible.w*sizeof *under);
        }
        uint32_t bg=w->bg,fg=(w->kind==TG_ENTRY||w->kind==TG_LISTBOX||w->kind==TG_TEXT||tg_choice_kind(w->kind))?0x101b2b:0xf0f5ff;
        if(w->explicit_fg) fg=w->fg;
        if(u->hover==w->id) { if(w->has_active_bg) bg=w->active_bg; if(w->has_active_fg) fg=w->active_fg; }
        if(w->state==TG_STATE_DISABLED&&w->has_disabled_fg) fg=w->disabled_fg;
        unsigned alpha=w->color_alpha;
        tg_style_colors_alpha(u,w,&bg,&fg,&alpha);
        if(!w->themed&&w->kind==TG_BUTTON&&u->pressed==w->id) bg=0x23568f;
        if(!w->themed&&w->state==TG_STATE_DISABLED&&w->kind!=TG_TEXT) bg=0x808080;
        int custom_layout=tg_style_layout_paint(u,w,c,bg,fg,(alpha*w->background_alpha+127u)/255u);
        int is_shadow=0;
        if(!custom_layout&&w->kind==TG_FRAME) {
            for(size_t si=0;si<u->count;si++) {
                if(u->items[si].dialog.enabled&&u->items[si].dialog.shadow==w->id) { is_shadow=1; break; }
            }
        }
        if(is_shadow) {
            int layers=8;
            for(int k=0;k<layers;k++) {
                int rx=w->rect.x+k, ry=w->rect.y+k;
                int rw=w->rect.w-2*k, rh=w->rect.h-2*k;
                if(rw<=2||rh<=2) break;
                double t=(double)(k+1)/(double)(layers+1);
                unsigned sa=(unsigned)(56.0*t*t);
                if(!sa) continue;
                int trim=k<3?(3-k):0;
                if(rw>2*trim) {
                    tg_canvas_fill_alpha(c,(tg_rect){rx+trim,ry,rw-2*trim,1},0x000000,sa);
                    tg_canvas_fill_alpha(c,(tg_rect){rx+trim,ry+rh-1,rw-2*trim,1},0x000000,sa);
                }
                if(rh>2) {
                    tg_canvas_fill_alpha(c,(tg_rect){rx,ry+1,1,rh-2},0x000000,sa);
                    tg_canvas_fill_alpha(c,(tg_rect){rx+rw-1,ry+1,1,rh-2},0x000000,sa);
                }
            }
            tg_rect inner={w->rect.x+layers,w->rect.y+layers,w->rect.w-2*layers,w->rect.h-2*layers};
            if(inner.w>0&&inner.h>0) tg_canvas_fill_alpha(c,inner,0x000000,52);
        } else if(!custom_layout) {
        tg_canvas_fill_alpha(c,w->rect,bg,(alpha*w->background_alpha+127u)/255u);
        if(w->border_width&&w->relief) {
            int limit=w->rect.w<w->rect.h?w->rect.w/2:w->rect.h/2;
            if(limit>w->border_width) limit=w->border_width;
            for(int b=0;b<limit;b++) {
                tg_rect r={w->rect.x+b,w->rect.y+b,w->rect.w-2*b,w->rect.h-2*b};
                uint32_t light=w->relief==2||w->relief==3?0x38465a:0xa2aec0;
                uint32_t dark=w->relief==2||w->relief==3?0xa2aec0:0x38465a;
                if(u->native_toplevels) {
                    /* Relief derives from the actual widget background,
                       not from a fixed dark preview palette. */
                    uint32_t bright=0,shadow=0;
                    for(unsigned shift=0;shift<=16;shift+=8) {
                        unsigned v=(bg>>shift)&255u;
                        bright|=(v+(255u-v)/3u)<<shift;
                        shadow|=(v*3u/5u)<<shift;
                    }
                    int inset=w->relief==2||w->relief==3;
                    if((w->relief==3||w->relief==4)&&b>=((limit+1)/2)) inset=!inset;
                    light=inset?shadow:bright; dark=inset?bright:shadow;
                }
                if(w->relief==5) light=dark=fg;
                tg_raster_fill(c,w->rect,(tg_rect){r.x,r.y,r.w,1},light);
                tg_raster_fill(c,w->rect,(tg_rect){r.x,r.y,1,r.h},light);
                tg_raster_fill(c,w->rect,(tg_rect){r.x,r.y+r.h-1,r.w,1},dark);
                tg_raster_fill(c,w->rect,(tg_rect){r.x+r.w-1,r.y,1,r.h},dark);
            }
        }
        if(w->highlight_width>0) {
            uint32_t border=u->focus==w->id?(w->has_highlight_color?w->highlight_color:0x0078d7):(w->has_highlight_background?w->highlight_background:bg);
            for(int k=0;k<w->highlight_width&&2*k<w->rect.w&&2*k<w->rect.h;k++) {
                tg_rect r={w->rect.x+k,w->rect.y+k,w->rect.w-2*k,w->rect.h-2*k};
                tg_raster_fill(c,w->rect,(tg_rect){r.x,r.y,r.w,1},border); tg_raster_fill(c,w->rect,(tg_rect){r.x,r.y+r.h-1,r.w,1},border);
                tg_raster_fill(c,w->rect,(tg_rect){r.x,r.y,1,r.h},border); tg_raster_fill(c,w->rect,(tg_rect){r.x+r.w-1,r.y,1,r.h},border);
            }
        }
        if(w->image_name[0]) { const tg_image_slot *image=tg_image_find(u,w->image_name); if(image) tg_photo_paint(image->photo,c,w->rect,w->rect.x+(w->rect.w-tg_photo_width(image->photo))/2,w->rect.y+(w->rect.h-tg_photo_height(image->photo))/2); }
        if(w->kind==TG_PROGRESS||w->kind==TG_SEPARATOR) tg_indicator_paint(w,c,fg);
        if(w->kind>=TG_SCALE&&w->kind<=TG_LABELFRAME) tg_extra_paint(u,w,c,fg);
        if(w->kind==TG_MENUBUTTON||w->kind==TG_SIZEGRIP) tg_container_paint(w,c,fg);
        if(tg_toggle_kind(w->kind)) tg_toggle_paint(u,w,c,fg);
        if(w->kind==TG_TREEVIEW) tg_tree_paint(u,w,c,fg);
        if(w->kind==TG_LISTBOX) tg_listbox_paint(w,c,fg);
        if(w->kind==TG_TEXT||w->kind==TG_MESSAGE) tg_document_paint(w,c,fg,u->focus==w->id);
        if(w->kind==TG_NOTEBOOK) tg_notebook_paint(u,w,c,fg);
        if(w->kind==TG_CANVAS&&tg_canvas_items_paint(u,w,c)!=TG_OK) { free(under); return TG_ERROR; }
        char masked[TG_ENTRY_DISPLAY_CAP];
        const char *shown=(w->kind==TG_ENTRY||tg_choice_kind(w->kind))?
            tg_entry_display_text(w,masked):w->text;
        if((w->kind==TG_LABEL||w->kind==TG_BUTTON)&&shown[0]&&(!w->image_name[0]||w->compound_center)&&tg_text_native_enabled()) {
            tg_text_layout_draw(c,w->rect,shown,w->font_spec,w->wraplength,w->justify,w->text_anchor,w->text_padx,w->text_pady,fg);
        }
        if((w->kind==TG_LABEL||w->kind==TG_BUTTON||w->kind==TG_ENTRY||tg_choice_kind(w->kind))&&shown[0]&&(!w->image_name[0]||w->compound_center)&&(!(w->kind==TG_LABEL||w->kind==TG_BUTTON)||!tg_text_native_enabled())) {
            int tw,th;
            if((tg_text_native_enabled()?tg_text_font_measure(shown,w->font_spec,&tw,&th):tg_text_measure(shown,1,&tw,&th))==TG_OK) {
                int tx=(w->kind==TG_ENTRY||tg_choice_kind(w->kind))?w->rect.x+w->border_width+w->highlight_width+w->text_padx+3-w->entry_scroll:w->rect.x+(w->rect.w-tw)/2;
                int ty=w->rect.y+(w->rect.h-th)/2;
                tg_rect clip=w->rect;
                if(w->kind==TG_ENTRY||tg_choice_kind(w->kind)) { int inset=w->border_width+w->highlight_width+w->text_padx+3; clip.x+=inset; clip.w=clip.w>2*inset?clip.w-2*inset:0; }
                if(tg_choice_kind(w->kind)) clip.w=clip.w>13?clip.w-13:0;
                if((w->kind==TG_ENTRY||tg_choice_kind(w->kind))&&w->sel_first<w->sel_last&&u->focus==w->id) {
                    size_t first=0,last=0,index=0,len=strlen(shown);
                    while(last<len&&index<w->sel_last) {
                        uint32_t cp; size_t used;
                        if(index==w->sel_first) first=last;
                        if(tg_utf8_decode((const unsigned char *)shown+last,len-last,&cp,&used)!=TG_OK) used=1;
                        last+=used; index++;
                    }
                    if(!tg_text_native_enabled()||tg_text_font_selection(c,clip,tx,ty,shown,w->font_spec,first,last,0x718eb9)!=TG_OK)
                        tg_raster_fill(c,clip,(tg_rect){tx+(int)w->sel_first*6,ty,(int)(w->sel_last-w->sel_first)*6,th},0x718eb9);
                }
                if(!tg_text_native_enabled()||tg_text_font_draw(c,clip,tx,ty,shown,w->font_spec,fg)!=TG_OK) tg_text_draw(c,clip,tx,ty,shown,1,fg);
            }
        }
        if(tg_choice_kind(w->kind)) tg_choice_paint(w,c,fg);
        if((w->kind==TG_ENTRY||tg_choice_kind(w->kind))&&u->focus==w->id&&w->state!=TG_STATE_DISABLED) {
            size_t p=0,n=strlen(shown),left=w->cursor;
            while(p<n&&left--) { uint32_t scalar; size_t used;
                if(tg_utf8_decode((const unsigned char *)shown+p,n-p,&scalar,&used)!=TG_OK) break;
                p+=used;
            }
            char prefix[TG_ENTRY_DISPLAY_CAP]; memcpy(prefix,shown,p); prefix[p]=0;
            int tw=0,th=7,width=0;
            if(tg_text_native_enabled()&&tg_text_font_caret(shown,w->font_spec,p,&tw)==TG_OK) {
                (void)tg_text_font_measure(shown[0]?shown:"Mg",w->font_spec,&width,&th);
            } else {
                (void)tg_text_measure(prefix,1,&tw,&th);
                tw+=(p?1:0); th=7;
            }
            tg_rect clip=w->rect;
                if(w->kind==TG_ENTRY||tg_choice_kind(w->kind)) { int inset=w->border_width+w->highlight_width+w->text_padx+3; clip.x+=inset; clip.w=clip.w>2*inset?clip.w-2*inset:0; }
                if(tg_choice_kind(w->kind)) clip.w=clip.w>13?clip.w-13:0;
            tg_raster_fill(c,clip,(tg_rect){w->rect.x+w->border_width+w->highlight_width+w->text_padx+3+tw-w->entry_scroll,w->rect.y+(w->rect.h-th)/2,1,th},fg);
        }

        }
        tg_style_layout_decorate(u,w,c,bg,fg,(alpha*w->background_alpha+127u)/255u);
        if(w->opacity<255&&c->sink) {
            if(!c->layer_sink||c->sink_failed||c->layer_sink(c->sink_context,visible,w->opacity,0)!=TG_OK) { free(under); return TG_ERROR; }
        } else if(w->opacity<255) {
            for(int y=0;y<visible.h;y++) {
                uint32_t *dst=c->p+(size_t)(visible.y+y)*c->w+visible.x;
                const uint32_t *src=under+(size_t)y*visible.w;
                for(int x=0;x<visible.w;x++)
                    dst[x]=tg_pixel_blend(src[x],((dst[x]&0xffffffu)<<8)|w->opacity);
            }
        }
    }
    c->clip_active=saved_active; c->clip=saved_clip;
    free(under); return TG_OK;
}
static int paint_band(const void *context,tg_canvas *c) {
    const plan *p=context;
    /* Bound simultaneously allocated group storage per row band. */
    int rc=paint_range(p,c,0,p->count,SIZE_MAX,(size_t)16*1024*1024);
    if(rc==TG_OK) { tg_menu_paint(p->u,c); tg_choice_popup_paint(p->u,c); }
    return rc;
}
int tg_ui_record_composite(const tg_ui *u,int width,int height,tg_draw_rect_sink sink,tg_draw_image_sink image_sink,tg_draw_layer_sink layer_sink,void *context) {
    if(!u||!sink||width<=0||height<=0||width>TG_DIM_LIMIT||height>TG_DIM_LIMIT||
       !tg_ui_get(u,u->view_root?u->view_root:1)||tg_ui_get(u,u->view_root?u->view_root:1)->rect.w!=width||tg_ui_get(u,u->view_root?u->view_root:1)->rect.h!=height) return TG_ERROR;
    /* Read-modify-write opacity groups and textured images need additional
       GPU passes. Reject before recording instead of misrendering them. */
    for(size_t k=0;k<u->count;k++) {
        const tg_widget *w=&u->items[k];
        if(!layer_sink&&((w->opacity>0&&w->opacity<255)||(w->group_opacity>0&&w->group_opacity<255))) return TG_ERROR;
    }
    if(u->drawing) for(size_t k=0;k<u->drawing->count;k++) {
        const tg_canvas_item *p=&u->drawing->item[k];
        if(p->opacity_set&&!p->opacity) continue;
        if((p->kind==TG_ITEM_IMAGE&&!image_sink)||(!layer_sink&&p->opacity_set&&p->opacity<255)) return TG_ERROR;
    }
    tg_canvas target={0};
    target.w=width; target.h=height; target.y1=height; target.borrowed=1;
    target.sink=sink; target.image_sink=image_sink; target.layer_sink=layer_sink; target.sink_context=context;
    plan p; prepare(&p,u);
    int rc=paint_band(&p,&target);
    return rc==TG_OK&&!target.sink_failed?TG_OK:TG_ERROR;
}
int tg_ui_record_draws(const tg_ui *u,int width,int height,tg_draw_rect_sink sink,tg_draw_image_sink image_sink,void *context) {
    return tg_ui_record_composite(u,width,height,sink,image_sink,NULL,context);
}
int tg_ui_record_rects(const tg_ui *u,int width,int height,tg_draw_rect_sink sink,void *context) {
    return tg_ui_record_draws(u,width,height,sink,NULL,context);
}
int tg_ui_paint(const tg_ui *u,tg_canvas *c) {
    if(!u||!c) return TG_ERROR;
    plan p; prepare(&p,u); return paint_band(&p,c);
}
int tg_ui_paint_parallel(const tg_ui *u,tg_canvas *c,unsigned requested,unsigned *used) {
    if(!u) { if(used) *used=0; return TG_ERROR; }
    plan p; prepare(&p,u); return tg_canvas_parallel_paint(c,paint_band,&p,requested,used);
}
static tg_id hit(const tg_ui *u,tg_id parent,int x,int y) {
    const tg_widget *best=NULL;
    for(size_t k=u->count;k>0;k--) {
        const tg_widget *w=&u->items[k-1]; tg_rect r=w->rect;
        if(w->kind==TG_MENU||(u->native_toplevels&&w->kind==TG_TOPLEVEL)||w->parent!=parent||x<r.x||y<r.y||x>=r.x+r.w||y>=r.y+r.h) continue;
        if(!best||w->z_order>best->z_order) best=w;
    }
    if(!best) return 0;
    tg_id child=best->child_count?hit(u,best->id,x,y):0;
    return child?child:best->id;
}
tg_id tg_ui_hit(const tg_ui *u,int x,int y) { if(!u) return 0; tg_id m=tg_choice_popup_hit(u,x,y); if(m) return m; m=tg_menu_hit(u,x,y); if(m) return m; if(u->view_root) { tg_id child=hit(u,u->view_root,x,y); return child?child:u->view_root; } return hit(u,0,x,y); }
