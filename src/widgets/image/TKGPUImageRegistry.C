#include "TKGPUImageRegistry.H"
#include "../TKGPUWidgetInternal.H"
#include <stdlib.h>
#include <string.h>
#define IMAGE_BYTES_LIMIT ((size_t)512*1024*1024)
const tg_image_slot *tg_image_find(const tg_ui *u,const char *name) {
    if(!u||!u->images||!name) return NULL;
    size_t limit=u->images->high_water;
    for(size_t k=0;k<limit;k++) {
        const tg_image_slot *s=&u->images->slots[k];
        if(s->photo&&!strcmp(s->name,name)) return s;
    }
    return NULL;
}
const tg_photo *tg_image_lookup(const tg_ui *u,uint32_t id) {
    if(!u||!u->images||!id) return NULL;
    size_t limit=u->images->high_water;
    for(size_t k=0;k<limit;k++)
        if(u->images->slots[k].id==id) return u->images->slots[k].photo;
    return NULL;
}
uint32_t tg_image_add(tg_ui *u,const char *name,int w,int h) {
    if(!u||!name||!*name||name[0]=='.'||strlen(name)>=128||
       w<0||h<0||w>4096||h>4096||tg_image_find(u,name)) return 0;
    if(!u->images) {
        u->images=calloc(1,sizeof(*u->images));
        if(!u->images) return 0;
        u->images->next=1;
    }
    tg_image_registry *r=u->images;
    size_t bytes=(size_t)w*h*4;
    if(!r->next||bytes>IMAGE_BYTES_LIMIT-r->bytes) return 0;
    for(size_t k=0;k<TG_IMAGE_LIMIT;k++) if(!r->slots[k].photo) {
        tg_photo *p=tg_photo_create(w,h);
        if(!p) return 0;
        tg_image_slot *s=&r->slots[k]; strcpy(s->name,name);
        s->photo=p; s->id=r->next++; r->bytes+=bytes;
        if(k+1>r->high_water) r->high_water=k+1;
        return s->id;
    }
    return 0;
}
int tg_image_delete(tg_ui *u,const char *name) {
    if(!u||!u->images||!name) return TG_ERROR;
    size_t limit=u->images->high_water;
    for(size_t k=0;k<limit;k++) {
        tg_image_slot *s=&u->images->slots[k];
        if(s->photo&&!strcmp(s->name,name)) {
            u->images->bytes-=(size_t)tg_photo_width(s->photo)*tg_photo_height(s->photo)*4;
            tg_photo_destroy(s->photo); memset(s,0,sizeof(*s));
            while(u->images->high_water&&!u->images->slots[u->images->high_water-1].photo)
                u->images->high_water--;
            return TG_OK;
        }
    }
    return TG_ERROR;
}
void tg_images_destroy(tg_ui *u) {
    if(!u||!u->images) return;
    size_t limit=u->images->high_water;
    for(size_t k=0;k<limit;k++) tg_photo_destroy(u->images->slots[k].photo);
    free(u->images); u->images=NULL;
}
