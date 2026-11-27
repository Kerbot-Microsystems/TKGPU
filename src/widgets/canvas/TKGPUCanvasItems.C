#include "TKGPUCanvasItems.H"
#include "../TKGPUWidgetInternal.H"
#include <stdlib.h>
#include <string.h>
uint32_t tg_canvas_item_add(tg_ui *u,tg_canvas_item item) {
    if(!u) return 0;
    const tg_widget *w=tg_ui_get(u,item.owner);
    if(!w||w->kind!=TG_CANVAS) return 0;
    if(!u->drawing) {
        u->drawing=calloc(1,sizeof(*u->drawing));
        if(!u->drawing) return 0;
        u->drawing->next=1;
    }
    tg_canvas_items *s=u->drawing;
    if(s->count>=TG_ITEM_LIMIT||!s->next) return 0;
    if(s->count==s->capacity) {
        size_t next_cap=s->capacity?s->capacity*2:256;
        if(next_cap>TG_ITEM_LIMIT) next_cap=TG_ITEM_LIMIT;
        tg_canvas_item *grown=realloc(s->item,next_cap*sizeof(*grown));
        if(!grown) return 0;
        s->item=grown; s->capacity=next_cap;
    }
    item.id=s->next++;
    s->item[s->count++]=item; return item.id;
}
void tg_canvas_items_destroy(tg_ui *u) {
    if(!u||!u->drawing) return;
    free(u->drawing->item);
    free(u->drawing);
    u->drawing=NULL;
}
void tg_canvas_items_remove(tg_ui *u,tg_id owner,uint32_t id) {
    if(!u||!u->drawing) return;
    tg_canvas_items *s=u->drawing; size_t dst=0;
    for(size_t k=0;k<s->count;k++) {
        tg_canvas_item *p=&s->item[k];
        if(p->owner==owner&&(!id||p->id==id)) continue;
        if(dst!=k) s->item[dst]=*p;
        dst++;
    }
    s->count=dst;
}
