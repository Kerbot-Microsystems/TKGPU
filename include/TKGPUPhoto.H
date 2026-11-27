#ifndef TKGPU_PHOTO_H
#define TKGPU_PHOTO_H
#include "TKGPURuntime.H"
typedef struct tg_photo tg_photo;
tg_photo *tg_photo_create(int w,int h);
tg_photo *tg_photo_clone(const tg_photo *p);
void tg_photo_destroy(tg_photo *p);
unsigned char *tg_photo_pixels(tg_photo *p);
int tg_photo_resize(tg_photo *,int,int);
int tg_photo_put_block(tg_photo *,const unsigned char *,int,int,const int *,int,int,int,int,int);
int tg_photo_width(const tg_photo *p);
int tg_photo_height(const tg_photo *p);
uint64_t tg_photo_revision(const tg_photo *p);
int tg_photo_fill(tg_photo *p,tg_rect rect,uint32_t rgba);
int tg_photo_pixel(const tg_photo *p,int x,int y,uint32_t *rgba);
int tg_photo_paint(const tg_photo *p,tg_canvas *c,tg_rect clip,int x,int y);
#endif
