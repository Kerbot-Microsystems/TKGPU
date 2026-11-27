#ifndef TKGPU_LEGACY_UNICODE_H
#define TKGPU_LEGACY_UNICODE_H
#include "TKGPUText.H"
int tg_legacy_unicode(const char *path,const char *text,int scale,int *w,int *h,
    tg_canvas *c,tg_rect clip,int x,int y,uint32_t rgb,size_t *byte,int *pixel,int hit,size_t selection_end);
#endif
