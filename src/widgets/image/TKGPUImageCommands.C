#include "TKGPUImageRegistry.H"
#include "../TKGPUWidgetInternal.H"
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static int out(char *b,size_t cap,const char *s,int rc) {
    if(!cap) return TG_ERROR;
    snprintf(b,cap,"%s",s); return rc;
}
static int number(const char *s,int *v) {
    char *end; errno=0; long n=strtol(s,&end,10);
    if(errno||end==s||*end||n<0||n>4096) return 0;
    *v=(int)n; return 1;
}
static int color(const char *s,uint32_t *rgba) {
    if(!strcmp(s,"red")) { *rgba=0xff0000ffu; return 1; }
    if(!strcmp(s,"black")) { *rgba=0x000000ffu; return 1; }
    if(!strcmp(s,"white")) { *rgba=0xffffffffu; return 1; }
    if(!strcmp(s,"blue")) { *rgba=0x0000ffffu; return 1; }
    if(strlen(s)!=7||*s!='#') return 0;
    uint32_t v=0;
    for(int k=1;k<7;k++) {
        unsigned char c=(unsigned char)s[k]; unsigned d;
        if(c>='0'&&c<='9') d=c-'0';
        else if(c>='a'&&c<='f') d=c-'a'+10;
        else if(c>='A'&&c<='F') d=c-'A'+10;
        else return 0;
        v=(v<<4)|d;
    }
    *rgba=(v<<8)|255u; return 1;
}
int tg_image_command(tg_ui *u,size_t n,char **a,char *b,size_t cap) {
    if(!n) return -1;
    if(!strcmp(a[0],"image")) {
        if(n>=4&&!strcmp(a[1],"create")&&!strcmp(a[2],"photo")&&(n-4)%2==0) {
            int w=0,h=0;
            for(size_t k=4;k<n;k+=2) {
                if(!strcmp(a[k],"-width")) { if(!number(a[k+1],&w)) goto bad; }
                else if(!strcmp(a[k],"-height")) { if(!number(a[k+1],&h)) goto bad; }
                else goto bad;
            }
            if(strlen(a[3])>=cap||!tg_image_add(u,a[3],w,h)) goto bad;
            return out(b,cap,a[3],TG_OK);
        }
        if(n!=3) goto bad;
        if(!strcmp(a[1],"delete"))
            return out(b,cap,"",tg_image_delete(u,a[2]));
        const tg_image_slot *s=tg_image_find(u,a[2]);
        if(!s) goto bad;
        if(!strcmp(a[1],"width")) snprintf(b,cap,"%d",tg_photo_width(s->photo));
        else if(!strcmp(a[1],"height")) snprintf(b,cap,"%d",tg_photo_height(s->photo));
        else if(!strcmp(a[1],"type")) snprintf(b,cap,"photo");
        else goto bad;
        return TG_OK;
    }
    const tg_image_slot *s=tg_image_find(u,a[0]);
    if(!s) return -1;
    if(n>=2&&(!strcmp(a[1],"configure")||!strcmp(a[1],"config"))) {
        if(n==2) return out(b,cap,"",TG_OK);
        if(n==3) {
            if(!strcmp(a[2],"-width")) { snprintf(b,cap,"%d",tg_photo_width(s->photo)); return TG_OK; }
            if(!strcmp(a[2],"-height")) { snprintf(b,cap,"%d",tg_photo_height(s->photo)); return TG_OK; }
            return out(b,cap,"",TG_OK);
        }
        if((n-2)%2!=0) goto bad;
        int w=tg_photo_width(s->photo),h=tg_photo_height(s->photo),resized=0;
        for(size_t k=2;k<n;k+=2) {
            if(!strcmp(a[k],"-width")) { if(!number(a[k+1],&w)) goto bad; resized=1; }
            else if(!strcmp(a[k],"-height")) { if(!number(a[k+1],&h)) goto bad; resized=1; }
            else if(!strcmp(a[k],"-gamma")||!strcmp(a[k],"-palette")||!strcmp(a[k],"-format")||!strcmp(a[k],"-data")||!strcmp(a[k],"-file")) continue;
            else goto bad;
        }
        if(resized&&(w!=tg_photo_width(s->photo)||h!=tg_photo_height(s->photo))) {
            int old_w=tg_photo_width(s->photo),old_h=tg_photo_height(s->photo);
            size_t before=(size_t)old_w*old_h*4,after=(size_t)w*h*4;
            if(tg_photo_resize(s->photo,w,h)!=TG_OK) goto bad;
            if(u->images) {
                if(after>=before) u->images->bytes+=after-before;
                else if(u->images->bytes>=before-after) u->images->bytes-=before-after;
            }
        }
        return out(b,cap,"",TG_OK);
    }
    if(n==2&&!strcmp(a[1],"blank")) {
        return out(b,cap,"",tg_photo_fill(s->photo,(tg_rect){0,0,tg_photo_width(s->photo),tg_photo_height(s->photo)},0));
    }
    if(n==4&&!strcmp(a[1],"get")) {
        int x,y; uint32_t rgba;
        if(!number(a[2],&x)||!number(a[3],&y)||tg_photo_pixel(s->photo,x,y,&rgba)!=TG_OK) goto bad;
        snprintf(b,cap,"%u %u %u",(unsigned)(rgba>>24),(unsigned)((rgba>>16)&255),(unsigned)((rgba>>8)&255)); return TG_OK;
    }
    if((n==6||n==8)&&!strcmp(a[1],"put")&&!strcmp(a[3],"-to")) {
        int x,y,r,d; uint32_t rgba;
        if(!color(a[2],&rgba)||!number(a[4],&x)||!number(a[5],&y)) goto bad;
        r=x+1; d=y+1;
        if(n==8&&(!number(a[6],&r)||!number(a[7],&d))) goto bad;
        if(tg_photo_fill(s->photo,(tg_rect){x,y,r-x,d-y},rgba)!=TG_OK) goto bad;
        return out(b,cap,"",TG_OK);
    }
bad:
    return out(b,cap,"unsupported image command, name or bounds",TG_ERROR);
}
