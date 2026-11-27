#include "TKGPUText.H"
#include "TKGPURaster.H"
#include "TKGPUUtf8.H"
#include "TKGPUFallbackFont.H"
#include <string.h>
#include <limits.h>
static uint32_t next(const unsigned char **p,size_t *n) {
    uint32_t cp; size_t used;
    if(tg_utf8_decode(*p,*n,&cp,&used)!=TG_OK) { cp='?'; used=1; }
    *p+=used; *n-=used; return cp;
}
int tg_text_measure(const char *text,int scale,int *width,int *height) {
    if(!text||!width||!height||scale<1||scale>16) return TG_ERROR;
    if(tg_text_native_enabled()&&*text&&tg_text_native_measure(text,scale,width,height)==TG_OK) return TG_OK;
    const unsigned char *p=(const unsigned char *)text;
    size_t n=strlen(text),count=0;
    while(n) { (void)next(&p,&n); if(++count>(size_t)INT_MAX/(6*scale)) return TG_ERROR; }
    *width=count?(int)(count*6*scale)-scale:0; *height=7*scale; return TG_OK;
}
void tg_text_draw(tg_canvas *c,tg_rect clip,int x,int y,const char *text,int scale,uint32_t rgb) {
    if(c&&tg_text_native_enabled()&&tg_text_native_draw(c,clip,x,y,text,scale,rgb)==TG_OK) return;
    int w,h;
    if(!c||tg_text_measure(text,scale,&w,&h)!=TG_OK) return;
    if(x<-65536||x>65536||y<-65536||y>65536||w>65536) return;
    const unsigned char *p=(const unsigned char *)text;
    size_t n=strlen(text);
    while(n) {
        const unsigned char *rows=tg_fallback_glyph(next(&p,&n));
        for(int row=0;row<7;row++) {
            int col=0;
            while(col<5) {
                while(col<5&&!(rows[row]&(1u<<(4-col)))) col++;
                int first=col;
                while(col<5&&(rows[row]&(1u<<(4-col)))) col++;
                if(col>first) tg_raster_fill(c,clip,(tg_rect){x+first*scale,y+row*scale,(col-first)*scale,scale},rgb);
            }
        }
        x+=6*scale;
    }
}

int tg_text_row_height(int bitmap_height) {
    return tg_text_native_enabled()&&strcmp(tg_text_native_backend(),"bitmap")&&bitmap_height<24?24:bitmap_height;
}
void tg_text_draw_row(tg_canvas *c,tg_rect row,int x,const char *text,uint32_t rgb) {
    int width,height;
    if(tg_text_measure(text,1,&width,&height)!=TG_OK) return;
    int y=row.y+(row.h-height)/2;
    tg_text_draw(c,row,x,y,text,1,rgb);
}
