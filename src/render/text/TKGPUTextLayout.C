#include "TKGPUText.H"
#include <string.h>
#include <stdlib.h>
typedef struct { size_t begin,end; int width; } text_line;
static int layout(const char *text,const char *font,int wrap,text_line lines[128],int *count,int *width,int *height) {
    if(!text||strlen(text)>=4096) return TG_ERROR;
    size_t start=0,length=strlen(text); int max=0,total=0,lh=12,tmp;
    if(tg_text_font_measure("Mg",font,&tmp,&lh)!=TG_OK) return TG_ERROR;
    while(start<length) {
        size_t end=start,previous=start; int measured=0;
        while(end<length&&text[end]!='\n') {
            size_t word=end; while(end<length&&text[end]!=' '&&text[end]!='\n') end++;
            char buffer[4096]; memcpy(buffer,text+start,end-start); buffer[end-start]=0;
            if(tg_text_font_measure(buffer,font,&measured,&tmp)!=TG_OK) return TG_ERROR;
            if(wrap>0&&measured>wrap&&previous>start) { end=previous; break; }
            previous=end;
            if(end<length&&text[end]==' ') end++;
            if(end==word) break;
        }
        size_t finish=end; while(finish>start&&text[finish-1]==' ') finish--;
        char buffer[4096]; memcpy(buffer,text+start,finish-start); buffer[finish-start]=0;
        if(tg_text_font_measure(buffer,font,&measured,&tmp)!=TG_OK) return TG_ERROR;
        if(total==128) return TG_ERROR;
        lines[total++]=(text_line){start,finish,measured}; if(measured>max) max=measured;
        start=end; if(start<length&&text[start]=='\n') start++; else while(start<length&&text[start]==' ') start++;
    }
    if(!total) { lines[0]=(text_line){0,0,0}; total=1; }
    *count=total; *width=max; *height=total*(lh+2); return TG_OK;
}
int tg_text_layout_measure(const char *text,const char *font,int wrap,int *width,int *height) {
    text_line lines[128]; int count; return layout(text,font,wrap,lines,&count,width,height);
}
int tg_text_layout_draw(tg_canvas *c,tg_rect rect,const char *text,const char *font,int wrap,int justify,int anchor,int padx,int pady,uint32_t rgb) {
    text_line lines[128]; int count,width,height;
    if(layout(text,font,wrap,lines,&count,&width,&height)!=TG_OK) return TG_ERROR;
    int a=anchor?anchor-1:4,lineheight=height/count;
    int available=rect.w-2*padx,block_x=rect.x+padx+(available-width)*(a%3)/2;
    int block_y=rect.y+pady+(rect.h-2*pady-height)*(a/3)/2;
    for(int k=0;k<count;k++) {
        char buffer[4096]; size_t n=lines[k].end-lines[k].begin;
        memcpy(buffer,text+lines[k].begin,n); buffer[n]=0;
        if(!n) continue;
        int x=block_x+(width-lines[k].width)*(justify==1?1:justify==2?2:0)/2;
        if(tg_text_font_draw(c,rect,x,block_y+k*lineheight,buffer,font,rgb)!=TG_OK) return TG_ERROR;
    }
    return TG_OK;
}
