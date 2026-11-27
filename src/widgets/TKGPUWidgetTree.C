#include "TKGPUWidgetInternal.H"
#include "listbox/TKGPUListbox.H"
#include "text/TKGPUTextWidget.H"
#include "ttk/TKGPUIndicators.H"
#include "ttk/TKGPUExtra.H"
#include "ttk/TKGPUChoice.H"
#include "ttk/TKGPUNotebook.H"
#include "ttk/TKGPUToggle.H"
#include "ttk/TKGPUContainers.H"
#include "ttk/tree/TKGPUTree.H"
#include "canvas/TKGPUCanvasItems.H"
#include "image/TKGPUImageRegistry.H"
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
static const uint32_t backgrounds[]={0,0x26374d,0x233247,0x397bc4,0x101b2b,0xf0f2f5,0x26374d,0x26374d,0x26374d,0x26374d,0x26374d,0x26374d,0xf0f2f5,0xf0f2f5,0x26374d,0x26374d,0x26374d,0x26374d,0x61768c,0x397bc4,0x26374d,0x26374d,0xf0f2f5,0xf0f2f5,0x233247};
/* IDs increase monotonically; subtree deletion preserves the sorted order. */
static size_t lookup_index(const tg_ui *u,tg_id id) {
    if(!u||!id) return TG_WIDGET_LIMIT;
    size_t direct=(size_t)id-1;
    if(direct<u->count&&u->items[direct].id==id) return direct;
    size_t lo=0,hi=u->count;
    while(lo<hi) {
        size_t mid=lo+(hi-lo)/2;
        if(u->items[mid].id<id) lo=mid+1; else hi=mid;
    }
    return lo<u->count&&u->items[lo].id==id?lo:TG_WIDGET_LIMIT;
}
tg_widget *tg_lookup(tg_ui *u,tg_id id) {
    size_t k=lookup_index(u,id); return k==TG_WIDGET_LIMIT?NULL:&u->items[k];
}
const tg_widget *tg_ui_get(const tg_ui *u,tg_id id) {
    size_t k=lookup_index(u,id); return k==TG_WIDGET_LIMIT?NULL:&u->items[k];
}
const char *tg_ui_font_spec(const tg_ui *u,const char *font) {
    if(u&&font) for(unsigned k=0;k<u->font_count;k++) if(!strcmp(u->fonts[k].name,font)) return u->fonts[k].spec;
    return font;
}
tg_ui *tg_ui_create(void) {
    tg_ui *u=calloc(1,sizeof(*u)); if(!u) return NULL;
    u->count=1; u->next=2;
    const char *linear=getenv("TKGPU_LINEAR_WIDGET_LOOKUP");
    u->path_index_disabled=linear&&!strcmp(linear,"1");
    u->items[0]=(tg_widget){.id=1,.kind=TG_TOPLEVEL,.bg=0x182536,.opacity=255,.background_alpha=255,.group_opacity=255,.color_alpha=255};
    strcpy(u->items[0].path,"."); return u;
}
void tg_ui_destroy(tg_ui *u) { if(u) { for(size_t k=0;k<u->count;k++) {
#ifdef _WIN32
    if(u->items[k].native_host) { DestroyWindow((HWND)u->items[k].native_host); u->items[k].native_host=NULL; }
#endif
    tg_tree_destroy(u->items[k].tree); tg_menu_destroy(u->items[k].menu); tg_listbox_destroy(u->items[k].listbox); tg_document_destroy(u->items[k].document); } tg_images_destroy(u); tg_canvas_items_destroy(u); free(u->styles); free(u); } }
tg_id tg_ui_add(tg_ui *u,const char *path,tg_kind kind,const char *text) {
    if(!u||!path||path[0]!='.'||!path[1]||u->count==TG_WIDGET_LIMIT||
       !u->next||kind<TG_FRAME||kind>TG_MESSAGE||tg_ui_find(u,path)||
       strlen(path)>=sizeof(u->items[0].path)|| (text&&strlen(text)>=sizeof(u->items[0].text))) return 0;
    char parent[TG_WIDGET_PATH_CAP]; strcpy(parent,path);
    char *dot=strrchr(parent,'.'); if(!dot[1]) return 0;
    if(dot==parent) dot[1]=0; else *dot=0;
    tg_id pid=tg_ui_find(u,parent); if(!pid) return 0;
    tg_widget *w=&u->items[u->count++];
    *w=(tg_widget){.id=u->next++,.parent=pid,.kind=kind,.request={0,0,100,32},.bg=backgrounds[kind],.opacity=255,.background_alpha=255,.group_opacity=255,.color_alpha=255};
    if(u->native_toplevels&&(kind==TG_FRAME||kind==TG_LABELFRAME)) w->request=(tg_rect){0,0,1,1};
    tg_indicator_defaults(w); tg_extra_defaults(w); tg_choice_defaults(w);
    if(kind==TG_TEXT&&!(w->document=tg_document_create())) { u->count--; return 0; }
    if(kind==TG_TEXT) w->request=(tg_rect){0,0,240,120};
    if(kind==TG_MESSAGE) w->request=(tg_rect){0,0,180,64};
    if(kind==TG_LISTBOX&&!(w->listbox=tg_listbox_create())) { u->count--; return 0; }
    if(kind==TG_LISTBOX) w->request=(tg_rect){0,0,120,100};
    if(kind==TG_TREEVIEW&&!(w->tree=tg_tree_create())) { u->count--; return 0; }
    if(kind==TG_TREEVIEW) w->request=(tg_rect){0,0,300,240};
    if(tg_toggle_kind(kind)) { tg_toggle_defaults(w); u->toggle_count++; if(kind==TG_CHECKBUTTON) strcpy(w->variable,path); }
    tg_container_defaults(w);
    if(kind==TG_MENU&&!(w->menu=tg_menu_create())) { u->count--; return 0; }
    w->takefocus=-1; strcpy(w->path,path); if(text) strcpy(w->text,text);
    tg_lookup(u,pid)->child_count++; tg_path_index_added(u); return w->id;
}
int tg_ui_remove(tg_ui *u,tg_id id) {
    size_t start=lookup_index(u,id);
    if(!u||id==1||start==TG_WIDGET_LIMIT) return TG_ERROR;
    int was_indexed=u->path_index_ready;
    /* During compaction the linear tree is authoritative, including queries
       from internal detach helpers. Never expose a partly stale index. */
    u->path_index_ready=0;
    if(u->entry_drag==id) u->entry_drag=0;
    /* Parent IDs are always strictly less than child IDs, so every descendant
       of 'id' lies at index >= start and can be marked in one forward pass. */
    unsigned char dropped[TG_WIDGET_LIMIT];
    memset(dropped+start,0,u->count-start);
    dropped[start]=1;
    for(size_t k=start+1;k<u->count;k++) {
        size_t pidx=lookup_index(u,u->items[k].parent);
        if(pidx>=start&&pidx<k&&dropped[pidx]) dropped[k]=1;
    }
    for(size_t k=u->count;k>start;k--) {
        size_t pos=k-1;
        if(!dropped[pos]) continue;
        tg_widget *w=&u->items[pos];
        if(u->focus==w->id) u->focus=0;
        if(u->pressed==w->id) u->pressed=0;
        if(u->hover==w->id) u->hover=0;
        if(u->drag_scroll==w->id) u->drag_scroll=0;
        if(u->drag_tree==w->id) u->drag_tree=0;
        if(u->dialog_capture==w->id) u->dialog_capture=0;
        if(u->choice_posted==w->id||u->choice_host==w->id) u->choice_posted=0;
        tg_widget *parent=tg_lookup(u,w->parent);
        if(parent&&parent->child_count) parent->child_count--;
        if(tg_toggle_kind(w->kind)) u->toggle_count--;
#ifdef _WIN32
        if(w->native_host) { DestroyWindow((HWND)w->native_host); w->native_host=NULL; }
#endif
        tg_container_detach(u,w);
        tg_menu_destroy(w->menu);
        tg_tree_destroy(w->tree);
        tg_listbox_destroy(w->listbox);
        tg_document_destroy(w->document);
        tg_notebook_detach(u,w);
        if(w->kind==TG_CANVAS) tg_canvas_items_remove(u,w->id,0);
    }
    size_t dst=start;
    for(size_t k=start;k<u->count;k++) {
        if(!dropped[k]) {
            if(dst!=k) u->items[dst]=u->items[k];
            dst++;
        }
    }
    u->count=dst;
    if(was_indexed) tg_path_index_rebuild(u);
    return TG_OK;
}

int tg_ui_configure(tg_ui *u,tg_id id,const char *text,const uint32_t *bg) {
    tg_widget *w=tg_lookup(u,id);
    if(!w||(text&&strlen(text)>=sizeof(w->text))) return TG_ERROR;
    if(text) memmove(w->text,text,strlen(text)+1);
    if(bg) { w->bg=*bg&0xffffffu; w->color_alpha=255; }
    return TG_OK;
}

/* Pack order is independent of widget IDs and stacking. */
size_t tg_sibling_order(const tg_ui *u,tg_id parent,size_t indices[TG_WIDGET_LIMIT]) {
    size_t n=0,start=lookup_index(u,parent);
    if(start==TG_WIDGET_LIMIT) return 0;
    unsigned expected=u->items[start].child_count;
    if(!expected) return 0;
    for(size_t k=start+1;k<u->count&&n<expected;k++) if(u->items[k].parent==parent) {
        uint32_t rank=u->items[k].pack_order?u->items[k].pack_order:u->items[k].id;
        size_t j=n;
        while(j) {
            const tg_widget *previous=&u->items[indices[j-1]];
            uint32_t r=previous->pack_order?previous->pack_order:previous->id;
            if(r<=rank) break;
            indices[j]=indices[j-1]; j--;
        }
        indices[j]=k; n++;
    }
    return n;
}
void tg_pack_reorder(tg_ui *u,tg_id id,tg_id relative,int after) {
    tg_widget *w=tg_lookup(u,id),*r=tg_lookup(u,relative);
    if(!w||!r||w==r||w->parent!=r->parent) return;
    size_t order[TG_WIDGET_LIMIT],n=tg_sibling_order(u,w->parent,order);
    uint32_t rank=1;
    for(size_t j=0;j<n;j++) {
        tg_widget *p=&u->items[order[j]];
        if(p==w) continue;
        if(p==r&&!after) w->pack_order=rank++;
        p->pack_order=rank++;
        if(p==r&&after) w->pack_order=rank++;
    }
}
