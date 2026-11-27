#ifndef TKGPU_TEXT_HISTORY_H
#define TKGPU_TEXT_HISTORY_H
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
/* Private bounded delta journal; no allocations when undo is disabled. */
enum { TG_TEXT_HISTORY_BYTES=16*1024*1024, TG_TEXT_HISTORY_RECORDS=4096 };
typedef struct tg_text_edit {
    struct tg_text_edit *prev,*next;
    size_t at,old_n,new_n,cost;
    uint64_t before,after,group;
    char text[];
} tg_text_edit;
typedef struct {
    tg_text_edit *first,*last,*applied;
    size_t bytes,count,groups;
    uint64_t revision,serial,clean,group;
    int enabled,automatic,max_groups,separate;
} tg_text_history;
static inline char *tg_history_old(tg_text_edit *e) { return e->text; }
static inline char *tg_history_new(tg_text_edit *e) { return tg_history_old(e)+e->old_n+1; }
static inline void tg_history_drop(tg_text_history *h,tg_text_edit *e) {
    if((!e->prev||e->prev->group!=e->group)&&(!e->next||e->next->group!=e->group)) h->groups--;
    if(e->prev) e->prev->next=e->next; else h->first=e->next;
    if(e->next) e->next->prev=e->prev; else h->last=e->prev;
    if(h->applied==e) h->applied=e->prev;
    h->bytes-=e->cost; h->count--; free(e);
}
static inline void tg_history_reset(tg_text_history *h) {
    while(h->last) tg_history_drop(h,h->last);
    h->separate=1;
}
static inline void tg_history_trim(tg_text_history *h) {
    while(h->first&&(h->bytes>TG_TEXT_HISTORY_BYTES||h->count>TG_TEXT_HISTORY_RECORDS||
          (h->max_groups>0&&h->groups>(size_t)h->max_groups))) {
        uint64_t group=h->first->group;
        /* Removing an unapplied prefix would invalidate later redo offsets. */
        if(!h->applied) { tg_history_reset(h); break; }
        do {
            int boundary=h->first==h->applied;
            tg_history_drop(h,h->first);
            if(boundary&&h->first&&h->first->group==group) { tg_history_reset(h); return; }
        } while(h->first&&h->first->group==group);
    }
}
static inline int tg_history_record(tg_text_history *h,size_t at,const char *old,size_t old_n,
                           const char *new_text,size_t new_n) {
    if(!old_n&&!new_n) return 1;
    if(!h->enabled) { tg_history_reset(h); h->revision=++h->serial; return 1; }
    if(old_n>1048576u||new_n>1048576u) return 0;
    size_t cost=sizeof(tg_text_edit)+old_n+new_n+2;
    tg_text_edit *e=malloc(cost); if(!e) return 0;
    memset(e,0,sizeof *e); e->at=at; e->old_n=old_n; e->new_n=new_n; e->cost=cost;
    if(old_n) memcpy(tg_history_old(e),old,old_n);
    tg_history_old(e)[old_n]=0;
    if(new_n) memcpy(tg_history_new(e),new_text,new_n);
    tg_history_new(e)[new_n]=0;
    int had_redo=h->applied!=h->last;
    while(h->last!=h->applied) tg_history_drop(h,h->last);
    tg_text_edit *p=h->last;
    int join=p&&!h->separate&&!had_redo&&(!h->automatic||
        (!p->old_n&&!old_n)||(!p->new_n&&!new_n));
    if(!join) { h->group++; h->groups++; }
    e->group=h->group; e->before=h->revision; e->after=++h->serial;
    e->prev=p; if(p) p->next=e; else h->first=e;
    h->last=h->applied=e; h->bytes+=cost; h->count++; h->revision=e->after; h->separate=0;
    tg_history_trim(h); return 1;
}
static inline tg_text_edit *tg_history_redo(const tg_text_history *h) { return h->applied?h->applied->next:h->first; }
#endif
