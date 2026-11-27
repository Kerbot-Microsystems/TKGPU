/* Original TKGPU command index. Dense registry slots remain authoritative. */
#include "../TKGPUCoreInternal.H"
#include <stdlib.h>
#include <string.h>
#define DEAD UINT32_MAX
#define INDEX_THRESHOLD 16u

static size_t hash_name(const char *s) {
    /* A small unsigned rolling hash; equality always uses the full name. */
    uint32_t hash=0;
    while(*s) hash=hash*131u+(unsigned char)*s++;
    hash^=hash>>16; hash*=0x9e3779b1u; hash^=hash>>15;
    return (size_t)hash;
}
static size_t position(const tg_interp *i,const char *name) {
    size_t mask=i->command_index_capacity-1,at=hash_name(name)&mask;
    for(size_t n=0;n<i->command_index_capacity;n++,at=(at+1)&mask) {
        uint32_t entry=i->command_index[at];
        if(!entry) return i->command_index_capacity;
        if(entry!=DEAD&&entry<=i->command_count&&!strcmp(i->commands[entry-1].name,name)) return at;
    }
    return i->command_index_capacity;
}
size_t tg_command_lookup(const tg_interp *i,const char *name) {
    if(!name) return i->command_count;
    if(i->command_index) {
        size_t at=position(i,name);
        return at==i->command_index_capacity?i->command_count:i->command_index[at]-1;
    }
    size_t k=0;
    while(k<i->command_count&&strcmp(i->commands[k].name,name)) k++;
    return k;
}
static void insert(tg_interp *i,size_t index) {
    size_t mask=i->command_index_capacity-1,at=hash_name(i->commands[index].name)&mask;
    size_t first_dead=i->command_index_capacity;
    while(i->command_index[at]) {
        if(i->command_index[at]==DEAD&&first_dead==i->command_index_capacity) first_dead=at;
        at=(at+1)&mask;
    }
    if(first_dead!=i->command_index_capacity) at=first_dead;
    else i->command_index_used++;
    i->command_index[at]=(uint32_t)(index+1);
}
void tg_command_index_clear(tg_interp *i) {
    free(i->command_index); i->command_index=NULL;
    i->command_index_capacity=i->command_index_used=0;
}
static void rebuild(tg_interp *i,size_t cap) {
    uint32_t *next=calloc(cap,sizeof *next);
    /* Allocation failure must not change correctness; fall back to linear lookup. */
    tg_command_index_clear(i);
    if(!next) return;
    i->command_index=next; i->command_index_capacity=cap;
    for(size_t k=0;k<i->command_count;k++) insert(i,k);
}
void tg_command_index_added(tg_interp *i) {
    if(i->command_count<INDEX_THRESHOLD&&!i->command_index) return;
    if(!i->command_index||(i->command_index_used+1)*2>=i->command_index_capacity) {
        size_t cap=32;
        while(i->command_count*2>=cap) cap*=2;
        rebuild(i,cap);
        return;
    }
    insert(i,i->command_count-1);
}
/* Called before dense swap removal. Update the slot of the last entry in place.
   Tombstones preserve the probe chain and are reclaimed on subsequent insertions. */
void tg_command_index_remove(tg_interp *i,size_t index) {
    if(!i->command_index) return;
    size_t last=i->command_count-1;
    size_t at=position(i,i->commands[index].name);
    size_t moved=index==last?at:position(i,i->commands[last].name);
    if(at==i->command_index_capacity||moved==i->command_index_capacity) {
        tg_command_index_clear(i); return;
    }
    i->command_index[at]=DEAD;
    if(index!=last) i->command_index[moved]=(uint32_t)(index+1);
    if(last<INDEX_THRESHOLD) tg_command_index_clear(i);
}
