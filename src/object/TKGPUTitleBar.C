#include "TKGPUTitleBar.H"
#include <string.h>
int tg_titlebar_attach(tg_titlebar *bar,const tg_ui *u,const tg_id *ids,size_t n) {
    if(!bar||!u||!ids||!n||n>TG_TITLEBAR_SOURCE_LIMIT) return TG_ERROR;
    for(size_t k=0;k<n;k++) {
        const tg_widget *w=tg_ui_get(u,ids[k]);
        if(!w||w->id==1||(w->kind!=TG_FRAME&&w->kind!=TG_LABEL&&w->kind!=TG_CANVAS)) return TG_ERROR;
    }
    tg_titlebar next={0}; next.count=(unsigned)n; next.enabled=1;
    memcpy(next.sources,ids,n*sizeof *ids); *bar=next; return TG_OK;
}
/* Exact hit, not an ancestor test: controls and search fields remain interactive. */
int tg_titlebar_hit(const tg_titlebar *bar,const tg_ui *u,int x,int y) {
    if(!bar||!bar->enabled||!u) return 0;
    tg_id id=tg_ui_hit(u,x,y);
    for(unsigned k=0;k<bar->count;k++) if(id==bar->sources[k]) return 1;
    return 0;
}
