#include "TKGPUChoice.H"
#include "../TKGPUWidgetInternal.H"
#include "TKGPUText.H"
#include "../../core/TKGPUList.H"
#include "TKGPURaster.H"
#include "TKGPUUtf8.H"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
int tg_choice_kind(tg_kind k) { return k==TG_COMBOBOX||k==TG_SPINBOX; }
void tg_choice_defaults(tg_widget *w) {
    if(!tg_choice_kind(w->kind)) return;
    w->choice_height=10; w->increment=1; strcpy(w->increment_text,"1");
    strcpy(w->minimum_text,"0"); strcpy(w->maximum_text,"0");
}
static int result(const char *s,char *out,size_t cap) {
    if(strlen(s)>=cap) return TG_ERROR;
    strcpy(out,s); return TG_OK;
}
static int number(const char *s,double *v) {
    char *end; errno=0; double x=strtod(s,&end);
    if(errno||end==s||*end||!isfinite(x)||fabs(x)>1e12||strlen(s)>=32) return 0;
    *v=x; return 1;
}
static int assign(tg_widget *w,const char *s) {
    size_t n=strlen(s),chars;
    if(n>=sizeof w->text||tg_utf8_count((const unsigned char *)s,n,&chars)!=TG_OK) return TG_ERROR;
    memcpy(w->text,s,n+1); w->cursor=chars; w->sel_anchor=chars; w->sel_first=w->sel_last=0; return TG_OK;
}
static int scan(const tg_widget *w,int wanted,char *out,int *current) {
    const char *p=w->choices; char item[256]; int n=0,rc;
    if(current) *current=-1;
    while((rc=tg_list_next(&p,item,sizeof item))>0) {
        if(current&&*current<0&&!strcmp(item,w->text)) *current=n;
        if(n==wanted&&out) strcpy(out,item);
        n++;
    }
    return rc<0?-1:n;
}
int tg_choice_option(tg_widget *w,const char *key,const char *s) {
    if(!tg_choice_kind(w->kind)) return -1;
    if(!strcmp(key,"-values")) {
        if(strlen(s)>=sizeof w->choices) return TG_ERROR;
        const char *p=s; char item[256]; int rc;
        while((rc=tg_list_next(&p,item,sizeof item))>0) {
            size_t count; if(tg_utf8_count((const unsigned char *)item,strlen(item),&count)!=TG_OK) return TG_ERROR;
        }
        if(rc<0) return TG_ERROR;
        strcpy(w->choices,s); return TG_OK;
    }
    if(w->kind==TG_COMBOBOX) {
        if(!strcmp(key,"-postcommand")) {
            if(strlen(s)>=sizeof w->choice_postcommand) return TG_ERROR;
            strcpy(w->choice_postcommand,s); return TG_OK;
        }
        if(!strcmp(key,"-height")) {
            char *end; errno=0; long n=strtol(s,&end,10);
            if(errno||end==s||*end||n<1||n>512) return TG_ERROR;
            w->choice_height=(int)n; return TG_OK;
        }
        return -1;
    }
    if(!strcmp(key,"-command")) {
        if(strlen(s)>=sizeof w->command) return TG_ERROR;
        strcpy(w->command,s); return TG_OK;
    }
    if(!strcmp(key,"-format")) {
        /* A restricted floating-point format, never arbitrary printf input. */
        if(*s) {
            if(*s!='%'||strlen(s)>=sizeof w->choice_format) return TG_ERROR;
            const char *p=s+1; unsigned width=0,precision=0;
            while(*p>='0'&&*p<='9') { width=width*10+(unsigned)(*p++-'0'); if(width>32) return TG_ERROR; }
            if(*p=='.') {
                p++;
                if(*p<'0'||*p>'9') return TG_ERROR;
                while(*p>='0'&&*p<='9') { precision=precision*10+(unsigned)(*p++-'0'); if(precision>16) return TG_ERROR; }
            }
            if(strcmp(p,"f")) return TG_ERROR;
        }
        strcpy(w->choice_format,s); return TG_OK;
    }
    if(!strcmp(key,"-from")) { if(!number(s,&w->minimum)) return TG_ERROR; strcpy(w->minimum_text,s); }
    else if(!strcmp(key,"-to")) { if(!number(s,&w->maximum)) return TG_ERROR; strcpy(w->maximum_text,s); }
    else if(!strcmp(key,"-increment")) { double v; if(!number(s,&v)||v<=0) return TG_ERROR; w->increment=v; strcpy(w->increment_text,s); }
    else if(!strcmp(key,"-wrap")) {
        if(!strcmp(s,"1")||!strcmp(s,"true")||!strcmp(s,"yes")) w->wrap=1;
        else if(!strcmp(s,"0")||!strcmp(s,"false")||!strcmp(s,"no")) w->wrap=0;
        else return TG_ERROR;
    } else return -1;
    return TG_OK;
}
int tg_choice_query(const tg_widget *w,const char *key,char *out,size_t cap) {
    if(!tg_choice_kind(w->kind)) return -1;
    if(!strcmp(key,"-values")) return result(w->choices,out,cap);
    if(w->kind==TG_COMBOBOX) {
        if(!strcmp(key,"-postcommand")) return result(w->choice_postcommand,out,cap);
        if(!strcmp(key,"-height")) { int z=snprintf(out,cap,"%d",w->choice_height); return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR; }
        return -1;
    }
    if(!strcmp(key,"-command")) return result(w->command,out,cap);
    if(!strcmp(key,"-format")) return result(w->choice_format,out,cap);
    if(!strcmp(key,"-from")) return result(w->minimum_text,out,cap);
    if(!strcmp(key,"-to")) return result(w->maximum_text,out,cap);
    if(!strcmp(key,"-increment")) return result(w->increment_text,out,cap);
    if(!strcmp(key,"-wrap")) return result(w->wrap?"1":"0",out,cap);
    return -1;
}
int tg_choice_command(tg_widget *w,size_t n,char **a,char *out,size_t cap) {
    if(!tg_choice_kind(w->kind)||n<2) return -1;
    if(n==4&&!strcmp(a[1],"identify")) {
        int x,y; char extra;
        if(sscanf(a[2],"%d%c",&x,&extra)!=1||sscanf(a[3],"%d%c",&y,&extra)!=1) return TG_ERROR;
        if(x<0||y<0||x>=w->rect.w||y>=w->rect.h) return result("",out,cap);
        if(x>=w->rect.w-13) return result(w->kind==TG_COMBOBOX?"downarrow":y<w->rect.h/2?"uparrow":"downarrow",out,cap);
        return result("textarea",out,cap);
    }
    if(!strcmp(a[1],"set")) {
        if(n!=3||!cap||assign(w,a[2])!=TG_OK) return TG_ERROR;
        out[0]=0; return TG_OK;
    }
    if(w->kind!=TG_COMBOBOX||strcmp(a[1],"current")) return -1;
    if(n==2) {
        int current; if(scan(w,-1,NULL,&current)<0) return TG_ERROR;
        int k=snprintf(out,cap,"%d",current); return k>=0&&(size_t)k<cap?TG_OK:TG_ERROR;
    }
    if(n!=3||!cap) return TG_ERROR;
    char *end; errno=0; long index=strtol(a[2],&end,10);
    if(errno||end==a[2]||*end||index<0||index>511) return TG_ERROR;
    char item[256]; int count=scan(w,(int)index,item,NULL);
    if(index>=count||assign(w,item)!=TG_OK) return TG_ERROR;
    out[0]=0; return TG_OK;
}
int tg_choice_step(tg_widget *w,int direction) {
    if(!w||!tg_choice_kind(w->kind)||w->state==TG_STATE_DISABLED) return 0;
    int current,n=scan(w,-1,NULL,&current);
    if(n>0) {
        int k=current<0?(direction>0?0:n-1):current+direction;
        if(k<0||k>=n) k=w->kind==TG_SPINBOX&&w->wrap?(k<0?n-1:0):(k<0?0:n-1);
        char item[256]; if(scan(w,k,item,NULL)<=k) return 0;
        return assign(w,item)==TG_OK;
    }
    if(n<0||w->kind!=TG_SPINBOX||w->minimum>w->maximum) return 0;
    double v;
    if(!number(w->text,&v)) v=w->minimum;
    else v+=direction*w->increment;
    if(v>w->maximum) v=w->wrap?w->minimum:w->maximum;
    if(v<w->minimum) v=w->wrap?w->maximum:w->minimum;
    char item[128]; int k=snprintf(item,sizeof item,w->choice_format[0]?w->choice_format:"%.15g",v);
    return k>0&&(size_t)k<sizeof item&&assign(w,item)==TG_OK;
}
void tg_choice_paint(const tg_widget *w,tg_canvas *c,uint32_t fg) {
    tg_rect r=w->rect; if(r.w<12||r.h<8) return;
    int x=r.x+r.w-11,y=r.y+r.h/2;
    tg_raster_fill(c,r,(tg_rect){x-2,r.y,13,r.h},0x96a6ba);
    if(w->kind==TG_SPINBOX) {
        tg_raster_line(c,r,x,y-3,x+4,y-6,fg);
        tg_raster_line(c,r,x+4,y-6,x+8,y-3,fg);
        y+=3;
    }
    tg_raster_line(c,r,x,y-1,x+4,y+2,fg);
    tg_raster_line(c,r,x+4,y+2,x+8,y-1,fg);
}

/* Native choice overlay, with no allocated child windows or copied choices.
   The bounded list is read from its live owner on the UI thread and snapshots. */
static int limit(int v,int lo,int hi) { return v<lo?lo:v>hi?hi:v; }
static tg_id owner(const tg_ui *u,const tg_widget *w) {
    if(!u->native_toplevels) return 1;
    while(w&&w->kind!=TG_TOPLEVEL) w=tg_ui_get(u,w->parent);
    return w?w->id:1;
}
static int visible(const tg_ui *u) {
    return u->choice_posted&&(!u->native_toplevels||!u->view_root||u->view_root==u->choice_host);
}
static int rows(const tg_ui *u,const tg_widget *w,tg_rect *r) {
    const tg_widget *host=tg_ui_get(u,u->choice_host);
    int count=scan(w,-1,NULL,NULL),row=tg_text_row_height(20);
    if(!host||count<=0||row<=0||host->rect.w<=0) return 0;
    int width=limit(w->rect.w,1,host->rect.w);
    int below=limit(host->rect.h-w->rect.y-w->rect.h,0,host->rect.h);
    int above=limit(w->rect.y,0,host->rect.h);
    int n=count<w->choice_height?count:w->choice_height;
    int up=below<n*row&&above>below,available=(up?above:below)/row;
    if(n>available) n=available;
    if(n<=0) return 0;
    *r=(tg_rect){limit(w->rect.x,0,host->rect.w-width),up?w->rect.y-n*row:w->rect.y+w->rect.h,width,n*row};
    return n;
}
static void scroll_active(tg_ui *u,int count,int n) {
    if(u->choice_active<u->choice_top) u->choice_top=u->choice_active;
    if(u->choice_active>=u->choice_top+n) u->choice_top=u->choice_active-n+1;
    u->choice_top=limit(u->choice_top,0,count>n?count-n:0);
}
tg_id tg_choice_trigger(tg_ui *u,tg_event e) {
    tg_widget *w=tg_lookup(u,e.kind==TG_POINTER_DOWN?e.target:u->focus);
    if(!w||w->kind!=TG_COMBOBOX||w->state==TG_STATE_DISABLED) return 0;
    if(e.kind==TG_POINTER_DOWN&&e.x>=w->rect.x+w->rect.w-13&&e.x<w->rect.x+w->rect.w&&
       e.y>=w->rect.y&&e.y<w->rect.y+w->rect.h) return w->id;
    if(e.kind==TG_KEY_DOWN&&(e.key&TG_MOD_ALT)&&(e.key&TG_KEY_VALUE_MASK)==TG_KEY_ARROW_DOWN) return w->id;
    return 0;
}
int tg_choice_open(tg_ui *u,tg_id id) {
    tg_widget *w=tg_lookup(u,id); int current;
    if(!w||w->kind!=TG_COMBOBOX||w->state==TG_STATE_DISABLED) return 0;
    int count=scan(w,-1,NULL,&current);
    if(count<=0) return 0;
    u->choice_posted=id; u->choice_host=owner(u,w); u->choice_active=current<0?0:current; u->choice_top=0;
    tg_rect r; int n=rows(u,w,&r);
    if(!n) { u->choice_posted=0; return 0; }
    scroll_active(u,count,n);
    u->focus=id; u->pressed=u->entry_drag=0; return 1;
}
tg_id tg_choice_popup_hit(const tg_ui *u,int x,int y) {
    if(!visible(u)) return 0;
    const tg_widget *w=tg_ui_get(u,u->choice_posted); tg_rect r;
    if(!w||!rows(u,w,&r)) return 0;
    return x>=r.x&&y>=r.y&&x<r.x+r.w&&y<r.y+r.h?w->id:0;
}
static int choose(tg_ui *u,tg_widget *w,int index,tg_id *selected) {
    char item[256]; int count=scan(w,index,item,NULL);
    u->choice_posted=0;
    if(index>=0&&index<count&&assign(w,item)==TG_OK) {
        *selected=w->id;
        (void)tg_ui_post(u,(tg_event){tg_virtual_event("ComboboxSelected"),w->id,0,0,0});
    }
    return 1;
}
int tg_choice_popup_input(tg_ui *u,tg_event e,tg_id *selected) {
    *selected=0;
    if(!u->choice_posted) return -1;
    tg_widget *w=tg_lookup(u,u->choice_posted); tg_rect r;
    if(!w||w->state==TG_STATE_DISABLED||!rows(u,w,&r)) { u->choice_posted=0; return -1; }
    if(!visible(u)) { if(e.kind==TG_POINTER_DOWN) u->choice_posted=0; return -1; }
    int count=scan(w,-1,NULL,NULL),n=rows(u,w,&r);
    u->choice_active=limit(u->choice_active,0,count-1);
    u->choice_top=limit(u->choice_top,0,count>n?count-n:0);
    if(e.kind==TG_POINTER_DOWN||e.kind==TG_POINTER_MOVE) {
        if(tg_choice_popup_hit(u,e.x,e.y)) {
            int index=u->choice_top+(e.y-r.y)/tg_text_row_height(20);
            if(e.kind==TG_POINTER_DOWN) return choose(u,w,index,selected);
            int changed=u->choice_active!=index; u->choice_active=index; return changed;
        }
        if(e.kind==TG_POINTER_DOWN) { u->choice_posted=0; return 1; }
        return 0;
    }
    if(e.kind==TG_WHEEL) {
        if(!tg_choice_popup_hit(u,e.x,e.y)) return 0;
        int delta=(int)(int16_t)(e.key&0xffffu);
        if(!delta) return 0;
        int units=delta/120; if(!units) units=delta>0?1:-1;
        int old=u->choice_top;
        u->choice_top=limit(old-units,0,count>n?count-n:0);
        u->choice_active=limit(u->choice_active,u->choice_top,u->choice_top+n-1);
        return old!=u->choice_top;
    }
    if(e.kind==TG_KEY_DOWN) {
        uint32_t key=e.key&TG_KEY_VALUE_MASK;
        if(key==TG_KEY_ESCAPE||key==TG_KEY_TAB) { u->choice_posted=0; return 1; }
        if(key==TG_KEY_RETURN) return choose(u,w,u->choice_active,selected);
        int old=u->choice_active;
        if(key==TG_KEY_ARROW_UP) u->choice_active--;
        else if(key==TG_KEY_ARROW_DOWN) u->choice_active++;
        else if(key==TG_KEY_PRIOR) u->choice_active-=n;
        else if(key==TG_KEY_NEXT) u->choice_active+=n;
        else if(key==TG_KEY_HOME) u->choice_active=0;
        else if(key==TG_KEY_END) u->choice_active=count-1;
        u->choice_active=limit(u->choice_active,0,count-1);
        scroll_active(u,count,n); return old!=u->choice_active;
    }
    if(e.kind==TG_POINTER_UP||e.kind==TG_KEY_UP||e.kind==TG_TEXT_INPUT) return 0;
    return -1;
}
void tg_choice_popup_paint(const tg_ui *u,tg_canvas *c) {
    if(!visible(u)) return;
    const tg_widget *w=tg_ui_get(u,u->choice_posted); tg_rect r;
    if(!w) return;
    int n=rows(u,w,&r); if(!n) return;
    int count=scan(w,-1,NULL,NULL),top=limit(u->choice_top,0,count>n?count-n:0);
    tg_raster_fill(c,r,r,0xf0f2f5); tg_raster_outline(c,r,r,0x61768c);
    const char *cur=w->choices; char item[256]; int index=0,row=tg_text_row_height(20);
    while(index<top+n&&tg_list_next(&cur,item,sizeof item)>0) {
        if(index>=top) {
            tg_rect line={r.x,r.y+(index-top)*row,r.w,row};
            int active=index==u->choice_active;
            if(active) tg_raster_fill(c,r,line,0x397bc4);
            tg_text_draw_row(c,line,line.x+4,item,active?0xffffff:0x101b2b);
        }
        index++;
    }
}
