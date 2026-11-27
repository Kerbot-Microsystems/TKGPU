/* Original TKGPU path index. No external dependencies.
   Widget IDs are stable through dense-array compaction and snapshots. */
#include "TKGPUWidgetInternal.H"
#include <string.h>
enum { INDEX_THRESHOLD=16 };
static size_t hash_path(const char *path) {
    uint32_t h=0;
    while(*path) h=h*131u+(unsigned char)*path++;
    h^=h>>16; h*=0x9e3779b1u; h^=h>>15;
    return (size_t)h;
}
static int insert(tg_ui *u,const tg_widget *w) {
    size_t mask=TG_PATH_INDEX_CAP-1,at=hash_path(w->path)&mask;
    for(size_t n=0;n<TG_PATH_INDEX_CAP;n++,at=(at+1)&mask)
        if(!u->path_index[at]) { u->path_index[at]=w->id; return 1; }
    return 0;
}
void tg_path_index_rebuild(tg_ui *u) {
    u->path_index_ready=0;
    if(u->path_index_disabled||u->count<INDEX_THRESHOLD) return;
    memset(u->path_index,0,sizeof u->path_index);
    for(size_t k=0;k<u->count;k++) if(!insert(u,&u->items[k])) return;
    u->path_index_ready=1;
}
void tg_path_index_added(tg_ui *u) {
    if(u->path_index_disabled) return;
    if(!u->path_index_ready) {
        if(u->count>=INDEX_THRESHOLD) tg_path_index_rebuild(u);
    } else if(!insert(u,&u->items[u->count-1])) u->path_index_ready=0;
}
tg_id tg_ui_find(const tg_ui *u,const char *path) {
    if(!u||!path) return 0;
    if(u->path_index_ready&&!u->path_index_disabled) {
        size_t mask=TG_PATH_INDEX_CAP-1,at=hash_path(path)&mask;
        for(size_t n=0;n<TG_PATH_INDEX_CAP;n++,at=(at+1)&mask) {
            tg_id id=u->path_index[at];
            if(!id) return 0;
            const tg_widget *w=tg_ui_get(u,id);
            if(w&&!strcmp(w->path,path)) return id;
        }
        return 0;
    }
    for(size_t k=0;k<u->count;k++) if(!strcmp(u->items[k].path,path)) return u->items[k].id;
    return 0;
}
