#include "TKGPUHistory.H"
#include <stdlib.h>
#include <string.h>
enum { TG_HISTORY_LIMIT=20,TG_HISTORY_WORD=4096 };
struct tg_history {
    char *items[TG_HISTORY_LIMIT];
    uint64_t next;
};
tg_history *tg_history_create(void) {
    tg_history *h=calloc(1,sizeof *h);
    if(h) h->next=1;
    return h;
}
void tg_history_clear(tg_history *h) {
    if(!h) return;
    for(size_t k=0;k<TG_HISTORY_LIMIT;k++) { free(h->items[k]); h->items[k]=NULL; }
    h->next=1;
}
void tg_history_destroy(tg_history *h) { if(h) { tg_history_clear(h); free(h); } }
int tg_history_record(tg_history *h,const char *s) {
    if(!h||!s) return TG_ERROR;
    if(!s[strspn(s," \t\r\n\v\f")]) return TG_OK;
    size_t n=strlen(s);
    if(n>=TG_HISTORY_WORD||h->next>=INT64_MAX) return TG_ERROR;
    char *p=malloc(n+1); if(!p) return TG_ERROR;
    memcpy(p,s,n+1);
    size_t k=(size_t)((h->next-1)%TG_HISTORY_LIMIT);
    free(h->items[k]); h->items[k]=p; h->next++;
    return TG_OK;
}
uint64_t tg_history_next(const tg_history *h) { return h?h->next:1; }
const char *tg_history_event(const tg_history *h,int64_t event) {
    uint64_t last=tg_history_next(h)-1,id;
    if(event<=0) {
        uint64_t offset=(uint64_t)(-(event+1))+1;
        if(offset>last) return NULL;
        id=last-offset;
    } else id=(uint64_t)event;
    if(!id) return "";
    if(!h||id>last||last-id>=TG_HISTORY_LIMIT) return NULL;
    return h->items[(id-1)%TG_HISTORY_LIMIT];
}
