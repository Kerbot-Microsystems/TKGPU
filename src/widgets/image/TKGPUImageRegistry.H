#ifndef TKGPU_IMAGE_REGISTRY_H
#define TKGPU_IMAGE_REGISTRY_H
#include "TKGPUUi.H"
#include "TKGPUPhoto.H"
enum { TG_IMAGE_LIMIT=4096 };
typedef struct { char name[128]; tg_photo *photo; uint32_t id; void *owner; } tg_image_slot;
typedef struct tg_image_registry {
    tg_image_slot slots[TG_IMAGE_LIMIT]; uint32_t next; size_t bytes,high_water;
} tg_image_registry;
const tg_image_slot *tg_image_find(const tg_ui *u,const char *name);
const tg_photo *tg_image_lookup(const tg_ui *u,uint32_t id);
uint32_t tg_image_add(tg_ui *u,const char *name,int w,int h);
int tg_image_delete(tg_ui *u,const char *name);
void tg_images_destroy(tg_ui *u);
int tg_image_command(tg_ui *u,size_t n,char **a,char *b,size_t cap);
#endif
