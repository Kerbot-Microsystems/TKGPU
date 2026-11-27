#include "TKGPUListbox.H"
#include "../../core/TKGPUList.H"
#include "TKGPURaster.H"
#include "TKGPUText.H"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <math.h>
enum { ROWS=1024, BYTES=256 };
#define HEIGHT tg_text_row_height(12)
typedef struct { char text[BYTES]; int selected; } row;
struct tg_listbox { row items[ROWS]; int count,active,anchor,top; };
tg_listbox *tg_listbox_create(void) { return calloc(1,sizeof(tg_listbox)); }
tg_listbox *tg_listbox_clone(const tg_listbox *s) {
    if(!s) return NULL;
    tg_listbox *d=malloc(sizeof *d); if(d) *d=*s; return d;
}
void tg_listbox_destroy(tg_listbox *s) { free(s); }
static int clamp(int n,int lo,int hi) { return n<lo?lo:n>hi?hi:n; }
static int listbox_row_height(const tg_widget *w) {
    if(w&&tg_text_native_enabled()&&strcmp(tg_text_native_backend(),"bitmap")!=0) {
        int tw=0,th=0;
        if(tg_text_font_measure("Mg",w->font_spec,&tw,&th)==TG_OK&&th>0&&th<512) return th+4;
    }
    return HEIGHT;
}
static int visible(const tg_widget *w) {
    int h=listbox_row_height(w);
    int avail=w->rect.h>2?w->rect.h-2:w->rect.h;
    int n=avail>0?avail/h:1;
    return n>0?n:1;
}
static int max_top(const tg_widget *w) {
    int n=w->listbox->count-visible(w); return n>0?n:0;
}
void tg_listbox_view_fractions(const tg_widget *w,double *first,double *last) {
    *first=0.0; *last=1.0;
    if(!w||!w->listbox||w->listbox->count<=0) return;
    const tg_listbox *s=w->listbox;
    int top=clamp(s->top,0,max_top(w));
    *first=(double)top/s->count;
    *last=(double)clamp(top+visible(w),0,s->count)/s->count;
}
static void normalize(tg_widget *w) {
    tg_listbox *s=w->listbox;
    int last=s->count?s->count-1:0;
    s->active=clamp(s->active,0,last); s->anchor=clamp(s->anchor,0,last);
    s->top=clamp(s->top,0,max_top(w));
}
static int integer(const char *p,int *out) {
    char *end; errno=0; long n=strtol(p,&end,10);
    if(end==p||*end||errno) return 0;
    *out=n<-1000000?-1000000:n>1000000?1000000:(int)n; return 1;
}
static int index_of(const tg_widget *w,const char *p,int *out) {
    const tg_listbox *s=w->listbox;
    if(!strcmp(p,"end")) { *out=s->count; return 1; }
    if(!strcmp(p,"active")) { *out=s->active; return 1; }
    if(!strcmp(p,"anchor")) { *out=s->anchor; return 1; }
    if(*p=='@') {
        int y; if(!integer(p+1,&y)) return 0;
        int h=listbox_row_height(w);
        *out=clamp(clamp(s->top,0,max_top(w))+clamp(y,0,16384)/h,0,s->count?s->count-1:0); return 1;
    }
    return integer(p,out);
}
static int item_index(const tg_widget *w,const char *p,int *out) {
    if(!index_of(w,p,out)) return 0;
    if(!strcmp(p,"end")) *out=w->listbox->count-1;
    return 1;
}
static int result(const char *p,char *out,size_t cap) {
    if(strlen(p)>=cap) return TG_ERROR;
    strcpy(out,p); return TG_OK;
}
static int num(int x,char *out,size_t cap) {
    int n=snprintf(out,cap,"%d",x); return n>=0&&(size_t)n<cap?TG_OK:TG_ERROR;
}
static void reveal(tg_widget *w,int at) {
    tg_listbox *s=w->listbox;
    if(at<s->top) s->top=at;
    if(at>=s->top+visible(w)) s->top=at-visible(w)+1;
    normalize(w);
}
int tg_listbox_command(tg_widget *w,size_t n,char **a,char *out,size_t cap) {
    if(w->kind!=TG_LISTBOX||n<2) return -1;
    tg_listbox *s=w->listbox;
    if(!s||!cap) return TG_ERROR;
    int first,last; const char *op=a[1];
    if(!strcmp(op,"size")&&n==2) return num(s->count,out,cap);
    if(!strcmp(op,"index")&&n==3&&index_of(w,a[2],&first))
        return num(clamp(first,0,s->count),out,cap);
    if(!strcmp(op,"nearest")&&n==3&&integer(a[2],&first)) {
        int h=listbox_row_height(w);
        return num(clamp(clamp(s->top,0,max_top(w))+clamp(first,0,16384)/h,0,s->count?s->count-1:0),out,cap);
    }
    if(!strcmp(op,"curselection")&&n==2) {
        out[0]=0;
        for(int k=0;k<s->count;k++) if(s->items[k].selected) {
            char b[16]; snprintf(b,sizeof b,"%d",k);
            if(!tg_list_append(out,cap,b)) return TG_ERROR;
        }
        return TG_OK;
    }
    if(!strcmp(op,"insert")&&n>=3&&index_of(w,a[2],&first)) {
        if(w->state==TG_STATE_DISABLED) return result("",out,cap);
        size_t add=n-3;
        if(add>(size_t)(ROWS-s->count)) add=(size_t)(ROWS-s->count);
        if(!add) return result("",out,cap);
        first=clamp(first,0,s->count);
        memmove(s->items+first+add,s->items+first,(size_t)(s->count-first)*sizeof(row));
        for(size_t k=0;k<add;k++) {
            row *r=&s->items[first+k]; memset(r,0,sizeof *r); snprintf(r->text,sizeof r->text,"%s",a[k+3]);
        }
        if(s->count&&s->active>=first) s->active+=(int)add;
        if(s->count&&s->anchor>=first) s->anchor+=(int)add;
        if(s->count&&s->top>first) s->top+=(int)add;
        s->count+=(int)add; normalize(w); return result("",out,cap);
    }
    if((!strcmp(op,"get")||!strcmp(op,"delete"))&&(n==3||n==4)&&item_index(w,a[2],&first)) {
        last=first;
        if(n==4&&!item_index(w,a[3],&last)) return TG_ERROR;
        if(!strcmp(op,"get")&&n==3)
            return result(first>=0&&first<s->count?s->items[first].text:"",out,cap);
        if(first<0) first=0;
        if(last>=s->count) last=s->count-1;
        if(!strcmp(op,"get")) {
            out[0]=0;
            for(int k=first;k<=last;k++) if(!tg_list_append(out,cap,s->items[k].text)) return TG_ERROR;
            return TG_OK;
        }
        if(w->state==TG_STATE_DISABLED||first>last) return result("",out,cap);
        int removed=last-first+1;
        memmove(s->items+first,s->items+last+1,(size_t)(s->count-last-1)*sizeof(row));
        s->count-=removed;
        memset(s->items+s->count,0,(size_t)removed*sizeof(row));
        if(s->active>=first) s->active=s->active<=last?first:s->active-removed;
        if(s->anchor>=first) s->anchor=s->anchor<=last?first:s->anchor-removed;
        if(s->top>=first) s->top=s->top<=last?first:s->top-removed;
        normalize(w); return result("",out,cap);
    }
    if((!strcmp(op,"activate")||!strcmp(op,"see"))&&n==3&&item_index(w,a[2],&first)) {
        first=clamp(first,0,s->count?s->count-1:0);
        if(!strcmp(op,"activate")) s->active=first; else reveal(w,first);
        return result("",out,cap);
    }
    if(!strcmp(op,"selection")&&(n==4||n==5)&&item_index(w,a[3],&first)) {
        const char *sub=a[2];
        if(!strcmp(sub,"includes")&&n==4) return num(first>=0&&first<s->count&&s->items[first].selected,out,cap);
        if(!strcmp(sub,"anchor")&&n==4) {
            s->anchor=clamp(first,0,s->count?s->count-1:0); return result("",out,cap);
        }
        int set=!strcmp(sub,"set");
        if(!set&&strcmp(sub,"clear")) return TG_ERROR;
        last=first;
        if(n==5&&!item_index(w,a[4],&last)) return TG_ERROR;
        if(w->state==TG_STATE_DISABLED) return result("",out,cap);
        if(first<0) first=0;
        if(last>=s->count) last=s->count-1;
        for(int k=first;k<=last;k++) s->items[k].selected=set;
        return result("",out,cap);
    }
    if(!strcmp(op,"yview")) {
        if(n==2) {
            double lo,hi; tg_listbox_view_fractions(w,&lo,&hi);
            int m=snprintf(out,cap,"%.12g %.12g",lo,hi); return m>=0&&(size_t)m<cap?TG_OK:TG_ERROR;
        }
        if(n==3&&item_index(w,a[2],&first)) s->top=first;
        else if(n==4&&!strcmp(a[2],"moveto")) {
            char *end; errno=0; double v=strtod(a[3],&end);
            if(end==a[3]||*end||errno||!isfinite(v)) return TG_ERROR;
            if(v<0) v=0;
            if(v>1) v=1;
            s->top=(int)(v*s->count+0.5);
        } else if(n==5&&!strcmp(a[2],"scroll")&&integer(a[3],&first)) {
            int step=(!strcmp(a[4],"units")||!strcmp(a[4],"unit"))?1:(!strcmp(a[4],"pages")||!strcmp(a[4],"page"))?clamp(visible(w)-1,1,ROWS):0;
            if(!step) return TG_ERROR;
            s->top=clamp(s->top,0,max_top(w))+first*step;
        } else return TG_ERROR;
        normalize(w); return result("",out,cap);
    }
    return -1;
}
void tg_listbox_paint(const tg_widget *w,tg_canvas *c,uint32_t fg) {
    const tg_listbox *s=w->listbox; if(!s) return;
    tg_rect r=w->rect;
    int inset=w->highlight_width>0?w->highlight_width:0;
    tg_rect inner={r.x+inset,r.y+inset,r.w>2*inset?r.w-2*inset:0,r.h>2*inset?r.h-2*inset:0};
    if(inner.w<=0||inner.h<=0) return;
    int h=listbox_row_height(w);
    int top=clamp(s->top,0,max_top(w));
    int vis=visible(w);
    int use_native=tg_text_native_enabled()&&strcmp(tg_text_native_backend(),"bitmap")!=0;
    for(int k=top;k<s->count&&k-top<vis;k++) {
        tg_rect line={inner.x,inner.y+(k-top)*h,inner.w,h};
        if(line.y+line.h>inner.y+inner.h) break;
        if(s->items[k].selected) tg_raster_fill(c,inner,line,0x397bc4);
        tg_rect text_clip={line.x+4,line.y,line.w>8?line.w-8:0,line.h};
        tg_rect clipped;
        if(text_clip.w>0&&tg_rect_intersect(inner,text_clip,&clipped)) {
            uint32_t ink=s->items[k].selected?0xffffff:fg;
            if(use_native) {
                int tw=0,th=0;
                int ty=line.y+1;
                if(tg_text_font_measure("Mg",w->font_spec,&tw,&th)==TG_OK&&th>0&&th<line.h)
                    ty=line.y+(line.h-th)/2;
                (void)tg_text_font_draw(c,clipped,line.x+4,ty,s->items[k].text,w->font_spec,ink);
            } else {
                tg_text_draw_row(c,clipped,line.x+3,s->items[k].text,ink);
            }
        }
    }
}
static int choose(tg_widget *w,int at) {
    tg_listbox *s=w->listbox;
    if(!s||!s->count||w->state==TG_STATE_DISABLED) return 0;
    at=clamp(at,0,s->count-1);
    for(int k=0;k<s->count;k++) s->items[k].selected=(k==at);
    s->anchor=s->active=at; reveal(w,at); return 1;
}
int tg_listbox_click(tg_widget *w,int x,int y) {
    if(x<w->rect.x||y<w->rect.y||x>=w->rect.x+w->rect.w||y>=w->rect.y+w->rect.h||!w->listbox) return 0;
    int inset=w->highlight_width>0?w->highlight_width:0;
    int h=listbox_row_height(w);
    normalize(w); int at=w->listbox->top+((y-w->rect.y-inset)>0?(y-w->rect.y-inset)/h:0);
    return at<w->listbox->count?choose(w,at):0;
}
int tg_listbox_step(tg_widget *w,int delta) {
    return w->listbox?choose(w,w->listbox->active+delta):0;
}
