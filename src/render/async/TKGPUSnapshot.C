#include "TKGPUSnapshot.H"
#include "../../widgets/TKGPUWidgetInternal.H"
#include "../../widgets/listbox/TKGPUListbox.H"
#include "../../widgets/text/TKGPUTextWidget.H"
#include "../../widgets/ttk/tree/TKGPUTree.H"
#include "../../widgets/ttk/TKGPUContainers.H"
#include "../../widgets/canvas/TKGPUCanvasItems.H"
#include "../../widgets/image/TKGPUImageRegistry.H"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#define SNAPSHOT_IMAGE_LIMIT ((size_t)64*1024*1024)
tg_ui *tg_snapshot_create(const tg_ui *u) {
    if(!u||u->count>TG_WIDGET_LIMIT||(u->images&&u->images->bytes>SNAPSHOT_IMAGE_LIMIT)) return NULL;
    size_t ui_bytes=offsetof(tg_ui,items)+u->count*sizeof(u->items[0]);
    if(ui_bytes<sizeof(tg_ui)) {
        /* Keep full struct size for safety, but avoid zeroing unused widget slots. */
    }
    tg_ui *copy=malloc(sizeof *copy); if(!copy) return NULL;
    memset(copy,0,offsetof(tg_ui,items));
    copy->head=0; copy->len=0; copy->focus=u->focus; copy->pressed=u->pressed; copy->activated=0; copy->entry_drag=0;
    copy->menu_posted=u->menu_posted; copy->menu_x=u->menu_x; copy->menu_y=u->menu_y;
    copy->menu_host=u->menu_host; copy->menu_depth=u->menu_depth;
    copy->choice_posted=u->choice_posted; copy->choice_host=u->choice_host; copy->choice_top=u->choice_top; copy->choice_active=u->choice_active;
    memcpy(copy->menu_chain,u->menu_chain,sizeof copy->menu_chain);
    memcpy(copy->menu_rows,u->menu_rows,sizeof copy->menu_rows);
    copy->drag_pane=0; copy->hover=u->hover; copy->drag_scroll=0; copy->dialog_capture=0;
    copy->drag_sash=0; copy->drag_scroll_offset=0; copy->toggle_count=u->toggle_count;
    copy->menu_toggle_count=u->menu_toggle_count; copy->menu_activation=0; copy->menu_activation_row=0;
    copy->styles=NULL; copy->style_count=0; copy->style_capacity=0;
    memcpy(copy->theme,u->theme,sizeof copy->theme);
    copy->theme_count=u->theme_count; memcpy(copy->themes,u->themes,sizeof copy->themes);
    copy->element_count=u->element_count; memcpy(copy->elements,u->elements,sizeof copy->elements);
    copy->drawing=NULL; copy->images=NULL;
    copy->font_count=u->font_count; memcpy(copy->fonts,u->fonts,sizeof copy->fonts); copy->uniform_count=u->uniform_count; memcpy(copy->uniform_names,u->uniform_names,sizeof copy->uniform_names); copy->view_root=u->view_root; copy->native_toplevels=u->native_toplevels; copy->count=u->count; copy->next=u->next;
    if(u->style_count) {
        copy->styles=malloc(u->style_count*sizeof *copy->styles);
        if(!copy->styles) { free(copy); return NULL; }
        memcpy(copy->styles,u->styles,u->style_count*sizeof *copy->styles);
        copy->style_count=copy->style_capacity=u->style_count;
    }
    memcpy(copy->items,u->items,u->count*sizeof u->items[0]);
    copy->path_index_disabled=u->path_index_disabled;
    copy->path_index_ready=u->path_index_ready;
    if(u->path_index_ready) memcpy(copy->path_index,u->path_index,sizeof copy->path_index);
    for(size_t k=0;k<u->count;k++) { copy->items[k].tree=NULL; copy->items[k].menu=NULL; copy->items[k].listbox=NULL; copy->items[k].document=NULL; copy->items[k].native_host=NULL; }
    for(size_t k=0;k<u->count;k++) if(u->items[k].document) {
        copy->items[k].document=tg_document_clone(u->items[k].document);
        if(!copy->items[k].document) goto fail;
    }
    for(size_t k=0;k<u->count;k++) if(u->items[k].listbox) {
        copy->items[k].listbox=tg_listbox_clone(u->items[k].listbox);
        if(!copy->items[k].listbox) goto fail;
    }
    for(size_t k=0;k<u->count;k++) if(u->items[k].tree) {
        copy->items[k].tree=tg_tree_clone(u->items[k].tree);
        if(!copy->items[k].tree) goto fail;
    }
    for(size_t k=0;k<u->count;k++) if(u->items[k].menu) { copy->items[k].menu=tg_menu_clone(u->items[k].menu); if(!copy->items[k].menu) goto fail; }
    if(u->drawing&&u->drawing->count) {
        copy->drawing=calloc(1,sizeof(*copy->drawing)); if(!copy->drawing) goto fail;
        copy->drawing->item=malloc(u->drawing->count*sizeof(tg_canvas_item));
        if(!copy->drawing->item) goto fail;
        copy->drawing->count=copy->drawing->capacity=u->drawing->count;
        copy->drawing->next=u->drawing->next;
        memcpy(copy->drawing->item,u->drawing->item,u->drawing->count*sizeof(tg_canvas_item));
    }
    if(u->images) {
        copy->images=calloc(1,sizeof *copy->images); if(!copy->images) goto fail;
        copy->images->next=u->images->next; copy->images->bytes=u->images->bytes; copy->images->high_water=u->images->high_water;
        for(size_t k=0;k<u->images->high_water;k++) {
            const tg_image_slot *s=&u->images->slots[k];
            if(!s->photo) continue;
            tg_photo *p=tg_photo_clone(s->photo); if(!p) goto fail;
            copy->images->slots[k]=*s; copy->images->slots[k].photo=p;
        }
    }
    return copy;
fail:
    tg_ui_destroy(copy); return NULL;
}
