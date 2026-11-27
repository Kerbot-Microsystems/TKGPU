#include "TKGPUPhoto.H"
#include "TKGPURaster.H"
#include "TKGPUChecked.H"
#include <stdlib.h>
#include <string.h>
struct tg_photo { int w,h; size_t stride,length; unsigned char *pixels; uint64_t revision; };
tg_photo *tg_photo_create(int w,int h) {
    size_t stride,length;
    if(w<0||h<0||w>4096||h>4096||
       tg_size_mul((size_t)w,4,&stride)!=TG_OK||
       tg_size_mul(stride,(size_t)h,&length)!=TG_OK) return NULL;
    tg_photo *p=calloc(1,sizeof(*p));
    if(!p) return NULL;
    p->pixels=calloc(length?length:1,1);
    if(!p->pixels) { free(p); return NULL; }
    p->w=w; p->h=h; p->stride=stride; p->length=length; p->revision=1; return p;
}
tg_photo *tg_photo_clone(const tg_photo *p) {
    if(!p) return NULL;
    tg_photo *copy=tg_photo_create(p->w,p->h);
    if(copy) { if(p->length) memcpy(copy->pixels,p->pixels,p->length); copy->revision=p->revision; }
    return copy;
}
void tg_photo_destroy(tg_photo *p) { if(p) { free(p->pixels); free(p); } }
int tg_photo_width(const tg_photo *p) { return p?p->w:0; }
int tg_photo_height(const tg_photo *p) { return p?p->h:0; }
unsigned char *tg_photo_pixels(tg_photo *p) { return p?p->pixels:NULL; }
int tg_photo_resize(tg_photo *p,int width,int height) {
    if(!p||p->revision==UINT64_MAX) return TG_ERROR;
    tg_photo *next=tg_photo_create(width,height); if(!next) return TG_ERROR;
    int w=width<p->w?width:p->w,h=height<p->h?height:p->h;
    for(int y=0;y<h;y++) memcpy(next->pixels+(size_t)y*next->stride,p->pixels+(size_t)y*p->stride,(size_t)w*4);
    next->revision=p->revision+1; free(p->pixels); *p=*next; free(next); return TG_OK;
}
int tg_photo_put_block(tg_photo *p,const unsigned char *data,int pitch,int pixel_size,const int *offset,int x,int y,int width,int height,int mode) {
    if(!p||!data||!offset||pixel_size<1||pixel_size>16||width<0||height<0||x<0||y<0||x>p->w||y>p->h||width>p->w-x||height>p->h-y||pitch<width*pixel_size||(mode!=0&&mode!=1)||p->revision==UINT64_MAX) return TG_ERROR;
    for(int k=0;k<4;k++) if(offset[k]<0||offset[k]>=pixel_size) return TG_ERROR;
    /* Exact RGBA replacement is a byte copy, not a per-pixel conversion.
       memmove also permits overlapping input and output within a row. */
    if(mode==1&&pixel_size==4&&offset[0]==0&&offset[1]==1&&offset[2]==2&&offset[3]==3&&
       x==0&&y==0&&width==p->w&&height==p->h&&pitch==width*4) {
        if(p->length) memmove(p->pixels,data,p->length);
        p->revision++; return TG_OK;
    }
    int has_alpha=offset[3]!=offset[0]&&offset[3]!=offset[1]&&offset[3]!=offset[2];
    for(int row=0;row<height;row++) for(int col=0;col<width;col++) {
        const unsigned char *src=data+(size_t)row*pitch+(size_t)col*pixel_size;
        unsigned char *dst=p->pixels+(size_t)(y+row)*p->stride+(size_t)(x+col)*4;
        unsigned alpha=has_alpha?src[offset[3]]:255;
        if(mode==1||alpha==255) { for(int k=0;k<3;k++) dst[k]=src[offset[k]]; dst[3]=(unsigned char)alpha; }
        else if(alpha) {
            unsigned out=alpha*255+dst[3]*(255-alpha);
            for(int k=0;k<3;k++) dst[k]=(unsigned char)((src[offset[k]]*alpha*255+dst[k]*dst[3]*(255-alpha)+out/2)/out);
            dst[3]=(unsigned char)((out+127)/255);
        }
    }
    p->revision++; return TG_OK;
}
uint64_t tg_photo_revision(const tg_photo *p) { return p?p->revision:0; }
int tg_photo_fill(tg_photo *p,tg_rect r,uint32_t rgba) {
    if(!p||r.x<0||r.y<0||r.w<0||r.h<0||r.x>p->w||r.y>p->h||
       r.w>p->w-r.x||r.h>p->h-r.y) return TG_ERROR;
    if(!r.w||!r.h) return TG_OK;
    if(p->revision==UINT64_MAX) return TG_ERROR;
    p->revision++;
    for(int y=r.y;y<r.y+r.h;y++) for(int x=r.x;x<r.x+r.w;x++) {
        unsigned char *q=p->pixels+(size_t)y*p->stride+(size_t)x*4;
        q[0]=(unsigned char)(rgba>>24); q[1]=(unsigned char)(rgba>>16);
        q[2]=(unsigned char)(rgba>>8); q[3]=(unsigned char)rgba;
    }
    return TG_OK;
}
int tg_photo_pixel(const tg_photo *p,int x,int y,uint32_t *rgba) {
    if(!p||!rgba||x<0||y<0||x>=p->w||y>=p->h) return TG_ERROR;
    const unsigned char *q=p->pixels+(size_t)y*p->stride+(size_t)x*4;
    *rgba=((uint32_t)q[0]<<24)|((uint32_t)q[1]<<16)|((uint32_t)q[2]<<8)|q[3];
    return TG_OK;
}
int tg_photo_paint(const tg_photo *p,tg_canvas *c,tg_rect clip,int x,int y) {
    if(!p||!c) return TG_ERROR;
    if(!p->w||!p->h) return TG_OK;
    return tg_raster_blit_rgba(c,clip,x,y,p->w,p->h,p->pixels,p->stride,p->length);
}
