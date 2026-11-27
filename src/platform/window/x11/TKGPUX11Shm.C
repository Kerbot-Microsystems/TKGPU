#include "TKGPUX11Shm.H"
#include <stdlib.h>
#include <string.h>
#ifdef TKGPU_X11_SHM
#include <sys/ipc.h>
#include <sys/shm.h>
#endif

unsigned long tg_x11_channel_pixel(unsigned value, unsigned long mask) {
    if(!mask) return 0;
    unsigned shift=0;
    while(!(mask&1ul)) { mask>>=1; shift++; }
    return (unsigned long)(((uint64_t)value*mask+127)/255)<<shift;
}

static void destroy_slot(tg_x11_slot *s, Display *display, int shm_supported) {
#ifdef TKGPU_X11_SHM
    if(s->shm_attached) {
        XShmDetach(display,&s->shm_info);
        s->shm_attached=0;
    }
    if(s->shm_info.shmaddr&&s->shm_info.shmaddr!=(char *)-1) {
        shmdt(s->shm_info.shmaddr);
        s->shm_info.shmaddr=NULL;
    }
    if(s->shm_info.shmid>=0) {
        shmctl(s->shm_info.shmid,IPC_RMID,NULL);
        s->shm_info.shmid=-1;
    }
    if(s->image) {
        if(shm_supported) s->image->data=NULL;
        XDestroyImage(s->image); s->image=NULL;
    }
#else
    (void)display; (void)shm_supported;
    if(s->image) { XDestroyImage(s->image); s->image=NULL; }
#endif
}

static int init_slot(tg_x11_slot *s, Display *display, int cap_w, int cap_h, int *shm_supported) {
    int screen=DefaultScreen(display);
    Visual *visual=DefaultVisual(display,screen);
    unsigned depth=(unsigned)DefaultDepth(display,screen);
    XImage *image=NULL;
#ifdef TKGPU_X11_SHM
    s->shm_info.shmid=-1; s->shm_info.shmaddr=(char *)-1; s->shm_info.readOnly=False;
    if(*shm_supported) {
        image=XShmCreateImage(display,visual,depth,ZPixmap,NULL,&s->shm_info,(unsigned)cap_w,(unsigned)cap_h);
        if(image&&image->bytes_per_line>0&&(size_t)image->bytes_per_line<=SIZE_MAX/(size_t)cap_h) {
            size_t total=(size_t)cap_h*(size_t)image->bytes_per_line;
            s->shm_info.shmid=shmget(IPC_PRIVATE,total,IPC_CREAT|0600);
            if(s->shm_info.shmid>=0) {
                s->shm_info.shmaddr=(char *)shmat(s->shm_info.shmid,NULL,0);
                if(s->shm_info.shmaddr!=(char *)-1) {
                    image->data=s->shm_info.shmaddr;
                    if(XShmAttach(display,&s->shm_info)) {
                        XSync(display,False);
                        shmctl(s->shm_info.shmid,IPC_RMID,NULL);
                        s->shm_info.shmid=-1;
                        s->shm_attached=1;
                    }
                }
            }
        }
        if(!s->shm_attached) {
            if(image) { image->data=NULL; XDestroyImage(image); image=NULL; }
            if(s->shm_info.shmaddr&&s->shm_info.shmaddr!=(char *)-1) shmdt(s->shm_info.shmaddr);
            if(s->shm_info.shmid>=0) shmctl(s->shm_info.shmid,IPC_RMID,NULL);
            s->shm_info.shmaddr=NULL; s->shm_info.shmid=-1;
            *shm_supported=0;
        }
    }
#endif
    if(!image) {
        image=XCreateImage(display,visual,depth,ZPixmap,0,NULL,(unsigned)cap_w,(unsigned)cap_h,32,0);
        if(!image) return 0;
        if(image->bytes_per_line<=0||(size_t)image->bytes_per_line>SIZE_MAX/(size_t)cap_h) {
            XDestroyImage(image); return 0;
        }
        image->data=calloc((size_t)cap_h,(size_t)image->bytes_per_line);
        if(!image->data) { XDestroyImage(image); return 0; }
    }
    uint32_t endian=1;
    int order=*(const unsigned char *)&endian?LSBFirst:MSBFirst;
    s->direct_rgb=image->bits_per_pixel==32&&image->byte_order==order&&
        image->red_mask==0x00ff0000ul&&image->green_mask==0x0000ff00ul&&
        image->blue_mask==0x000000fful&&(size_t)image->bytes_per_line>=(size_t)cap_w*4;
    s->image=image;
    return 1;
}

void tg_x11_presenter_init(tg_x11_presenter *p, Display *display) {
    memset(p,0,sizeof *p);
#ifdef TKGPU_X11_SHM
    p->slots[0].shm_info.shmid=-1; p->slots[0].shm_info.shmaddr=(char *)-1;
    p->slots[1].shm_info.shmid=-1; p->slots[1].shm_info.shmaddr=(char *)-1;
    p->shm_supported=XShmQueryExtension(display)!=0;
    if(p->shm_supported) p->shm_completion_type=XShmGetEventBase(display)+ShmCompletion;
#else
    (void)display;
#endif
}

void tg_x11_presenter_destroy(tg_x11_presenter *p, Display *display) {
    if(p->in_flight>0) { XSync(display,False); p->in_flight=0; }
    destroy_slot(&p->slots[0],display,p->shm_supported);
    destroy_slot(&p->slots[1],display,p->shm_supported);
    p->cap_w=0; p->cap_h=0; p->active=0;
}

int tg_x11_presenter_ensure(tg_x11_presenter *p, Display *display, int width, int height) {
    if(p->slots[0].image&&p->cap_w>=width&&p->cap_h>=height&&
       p->cap_w<=width*2+128&&p->cap_h<=height*2+128) return 1;
    int cap_w=(width+63)&~63;
    int cap_h=(height+63)&~63;
    if(cap_w<width) cap_w=width;
    if(cap_h<height) cap_h=height;
    if(cap_w>16384) cap_w=16384;
    if(cap_h>16384) cap_h=16384;
    tg_x11_presenter_destroy(p,display);
    if(!init_slot(&p->slots[0],display,cap_w,cap_h,&p->shm_supported)) return 0;
    if(p->shm_supported&&!init_slot(&p->slots[1],display,cap_w,cap_h,&p->shm_supported)) {
        destroy_slot(&p->slots[0],display,0);
        if(!init_slot(&p->slots[0],display,cap_w,cap_h,&p->shm_supported)) return 0;
    }
    p->cap_w=cap_w; p->cap_h=cap_h; p->active=0;
    return 1;
}

XImage *tg_x11_presenter_image(const tg_x11_presenter *p) {
    return p->slots[p->active].image;
}

int tg_x11_presenter_direct_rgb(const tg_x11_presenter *p) {
    return p->slots[p->active].direct_rgb;
}

void tg_x11_presenter_upload(tg_x11_presenter *p, Display *display, Drawable target, GC gc,
                             const uint32_t *pixels, int stride_w, tg_rect region) {
#ifdef TKGPU_X11_SHM
    if(p->shm_supported&&p->slots[1].shm_attached) {
        XEvent ev;
        (void)XEventsQueued(display,QueuedAfterReading);
        while(p->in_flight>0&&XCheckTypedEvent(display,p->shm_completion_type,&ev)) {
            p->in_flight--;
        }
        if(p->in_flight>=2) {
            XSync(display,False);
            p->in_flight=0;
        }
        p->active^=1;
    }
#endif
    tg_x11_slot *s=&p->slots[p->active];
    XImage *image=s->image;
    int dx=region.x,dy=region.y,dw=region.w,dh=region.h;
    if(s->direct_rgb) {
        size_t row_bytes=(size_t)dw*sizeof *pixels;
        for(int y=dy;y<dy+dh;y++) {
            memcpy(image->data+(size_t)y*image->bytes_per_line+(size_t)dx*4,
                   pixels+(size_t)y*stride_w+dx,row_bytes);
        }
    } else for(int y=dy;y<dy+dh;y++) for(int x=dx;x<dx+dw;x++) {
        uint32_t rgb=pixels[(size_t)y*stride_w+x];
        unsigned long pixel=tg_x11_channel_pixel((rgb>>16)&255,image->red_mask)|
            tg_x11_channel_pixel((rgb>>8)&255,image->green_mask)|
            tg_x11_channel_pixel(rgb&255,image->blue_mask);
        XPutPixel(image,x,y,pixel);
    }
#ifdef TKGPU_X11_SHM
    if(s->shm_attached) {
        XShmPutImage(display,target,gc,image,dx,dy,dx,dy,(unsigned)dw,(unsigned)dh,True);
        p->in_flight++;
        return;
    }
#endif
    XPutImage(display,target,gc,image,dx,dy,dx,dy,(unsigned)dw,(unsigned)dh);
}

void tg_x11_presenter_blit_existing(tg_x11_presenter *p, Display *display, Drawable target, GC gc,
                                    int width, int height) {
    tg_x11_slot *s=&p->slots[p->active];
#ifdef TKGPU_X11_SHM
    if(s->shm_attached) {
        XShmPutImage(display,target,gc,s->image,0,0,0,0,(unsigned)width,(unsigned)height,False);
        p->in_flight++;
        return;
    }
#endif
    XPutImage(display,target,gc,s->image,0,0,0,0,(unsigned)width,(unsigned)height);
}
