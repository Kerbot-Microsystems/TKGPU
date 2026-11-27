#include "TKGPULegacyUnicode.H"
#include "TKGPUUtf8.H"
#include "TKGPURaster.H"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
enum { RECORD=37, HEADER=12, MAX_RUN=4095 };
typedef struct { uint32_t cp; size_t offset; unsigned char width,rows[32]; int x; } glyph;
static uint32_t u32(const unsigned char *p) {
    return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static int lookup(FILE *f,uint32_t count,uint32_t cp,glyph *g) {
    uint32_t lo=0,hi=count; unsigned char r[RECORD];
    while(lo<hi) {
        uint32_t mid=lo+(hi-lo)/2;
        if(fseek(f,(long)(HEADER+mid*RECORD),SEEK_SET)||fread(r,1,RECORD,f)!=RECORD) return 0;
        uint32_t key=u32(r);
        if(key<cp) lo=mid+1;
        else if(key>cp) hi=mid;
        else {
            if(r[4]!=8&&r[4]!=16) return 0;
            g->width=r[4]; memcpy(g->rows,r+5,32); return 1;
        }
    }
    return 0;
}
/* Common Arabic letters and their isolated/initial/medial/final forms.
   This is intentionally a limited joining model, not a full Unicode bidi engine. */
typedef struct { uint16_t cp,iso; unsigned char dual; } arabic_form;
static const arabic_form forms[]={
 {0x0621,0xfe80,0},{0x0622,0xfe81,0},{0x0623,0xfe83,0},{0x0624,0xfe85,0},
 {0x0625,0xfe87,0},{0x0626,0xfe89,1},{0x0627,0xfe8d,0},{0x0628,0xfe8f,1},
 {0x0629,0xfe93,0},{0x062a,0xfe95,1},{0x062b,0xfe99,1},{0x062c,0xfe9d,1},
 {0x062d,0xfea1,1},{0x062e,0xfea5,1},{0x062f,0xfea9,0},{0x0630,0xfeab,0},
 {0x0631,0xfead,0},{0x0632,0xfeaf,0},{0x0633,0xfeb1,1},{0x0634,0xfeb5,1},
 {0x0635,0xfeb9,1},{0x0636,0xfebd,1},{0x0637,0xfec1,1},{0x0638,0xfec5,1},
 {0x0639,0xfec9,1},{0x063a,0xfecd,1},{0x0641,0xfed1,1},{0x0642,0xfed5,1},
 {0x0643,0xfed9,1},{0x0644,0xfedd,1},{0x0645,0xfee1,1},{0x0646,0xfee5,1},
 {0x0647,0xfee9,1},{0x0648,0xfeed,0},{0x0649,0xfeef,0},{0x064a,0xfef1,1}
};
static const arabic_form *form(uint32_t cp) {
    for(size_t i=0;i<sizeof forms/sizeof forms[0];i++) if(forms[i].cp==cp) return forms+i;
    return NULL;
}
static int rtl(uint32_t cp) { return (cp>=0x590&&cp<=0x8ff)||(cp>=0xfb1d&&cp<=0xfefc); }
static int number(uint32_t cp) { return (cp>='0'&&cp<='9')||(cp>=0x660&&cp<=0x669)||(cp>=0x6f0&&cp<=0x6f9); }
static uint32_t shaped(const glyph *g,size_t n,size_t k) {
    const arabic_form *cur=form(g[k].cp);
    if(!cur||cur->cp==0x0621) return g[k].cp;
    const arabic_form *prev=k?form(g[k-1].cp):NULL,*next=k+1<n?form(g[k+1].cp):NULL;
    int before=prev&&prev->dual,after=cur->dual&&next&&next->cp!=0x0621;
    return cur->iso+(before&&after?3:after?2:before?1:0);
}
static int edge(const glyph *g,size_t n,size_t k,int right) {
    if(k==n) return right?g[n-1].x:g[n-1].x+g[n-1].width;
    return right?g[k].x+g[k].width:g[k].x;
}
int tg_legacy_unicode(const char *path,const char *text,int scale,int *w,int *h,
    tg_canvas *c,tg_rect clip,int x,int y,uint32_t rgb,size_t *byte,int *pixel,int hit,size_t selection_end) {
    if(!path||!*path||!text||!*text||!w||!h||scale<1||scale>16) return TG_ERROR;
    size_t len=strlen(text),n=0;
    if(len>MAX_RUN) return TG_ERROR;
    glyph *g=calloc(len,sizeof *g); if(!g) return TG_ERROR;
    FILE *f=fopen(path,"rb"); int rc=TG_ERROR;
    if(!f) { free(g); return rc; }
    unsigned char header[HEADER]; uint32_t count;
    if(fread(header,1,HEADER,f)!=HEADER||memcmp(header,"TGFONT1\0",8)) goto done;
    count=u32(header+8); if(!count||count>0x110000u) goto done;
    if(fseek(f,0,SEEK_END)||ftell(f)!=(long)(HEADER+count*RECORD)) goto done;
    int right=0,base_found=0;
    for(size_t p=0;p<len;) {
        size_t used; uint32_t cp;
        if(tg_utf8_decode((const unsigned char *)text+p,len-p,&cp,&used)!=TG_OK) goto done;
        g[n].cp=cp; g[n++].offset=p; p+=used;
        if(!base_found&&!number(cp)&&cp!=' '&&cp!='\t') { right=rtl(cp); base_found=1; }
    }
    int width=0;
    for(size_t k=0;k<n;k++) {
        uint32_t cp=shaped(g,n,k);
        if(!lookup(f,count,cp,g+k)&&(cp==g[k].cp||!lookup(f,count,g[k].cp,g+k))) goto done;
        width+=g[k].width;
    }
    if(width>16384/scale) goto done;
    int pos=0;
    if(!right) {
        for(size_t k=0;k<n;k++) { g[k].x=pos; pos+=g[k].width; }
    } else {
        for(size_t k=n;k;) {
            size_t end=k,first=--k;
            if(number(g[k].cp)) while(first&&number(g[first-1].cp)) first--;
            for(size_t i=first;i<end;i++) { g[i].x=pos; pos+=g[i].width; }
            k=first;
        }
    }
    *w=width*scale; *h=16*scale;
    if(byte&&pixel) {
        if(hit) {
            int px=*pixel; size_t best=0; double dist=1e30;
            for(size_t k=0;k<=n;k++) {
                int at=edge(g,n,k,right)*scale;
                double d=(double)px-at; if(d<0) d=-d;
                if(d<dist) { dist=d; best=k; }
            }
            *byte=best==n?len:g[best].offset;
        } else {
            size_t k=0; while(k<n&&g[k].offset<*byte) k++;
            *pixel=edge(g,n,k,right)*scale;
        }
    }
    if(c) for(size_t k=0;k<n;k++) {
        int gx=x+g[k].x*scale;
        if(byte&&!pixel) {
            if(g[k].offset>=*byte&&g[k].offset<selection_end)
                tg_raster_fill(c,clip,(tg_rect){gx,y,g[k].width*scale,16*scale},rgb);
            continue;
        }
        for(int row=0;row<16;row++) {
            unsigned bits=((unsigned)g[k].rows[2*row]<<8)|g[k].rows[2*row+1];
            for(int col=0;col<g[k].width;col++)
                if(bits&(0x8000u>>col)) tg_raster_fill(c,clip,(tg_rect){gx+col*scale,y+row*scale,scale,scale},rgb);
        }
    }
    rc=TG_OK;
done:
    fclose(f); free(g); return rc;
}
