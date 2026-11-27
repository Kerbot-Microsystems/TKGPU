#include "TKGPUExtra.H"
#include "../TKGPUWidgetInternal.H"
#include "TKGPUStyle.H"
#include "TKGPUChoice.H"
#include "TKGPUToggle.H"
#include "TKGPURaster.H"
#include "TKGPUText.H"
#include "TKGPUIndicators.H"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <math.h>
static double clamp(double v,double a,double b) {
    double lo=a<b?a:b,hi=a<b?b:a;
    return v<lo?lo:v>hi?hi:v;
}
static int number(const char *s,double *out) {
    char *end; errno=0; double v=strtod(s,&end);
    if(end==s||*end||errno||!isfinite(v)||fabs(v)>1e12||strlen(s)>=32) return 0;
    *out=v; return 1;
}
static int format(double v,char *out,size_t cap) {
    if(fabs(v)<1e-12) v=0.0;
    int n=snprintf(out,cap,"%.15g",v);
    if(n<0||(size_t)n>=cap) return 0;
    if(!strpbrk(out,".eE")) { if((size_t)n+2>=cap) return 0; strcat(out,".0"); }
    return 1;
}
static int scale_color(const char *s,uint32_t *out) {
    static const struct { const char *name; uint32_t color; } named[]={
        {"black",0},{"white",0xffffff},{"red",0xff0000},{"green",0x00ff00},
        {"blue",0xff},{"gray",0xbebebe},{"grey",0xbebebe},{"yellow",0xffff00},
        {"cyan",0x00ffff},{"magenta",0xff00ff}};
    for(size_t k=0;k<sizeof named/sizeof named[0];k++)
        if(!strcmp(s,named[k].name)) { *out=named[k].color; return 1; }
    if(s[0]!='#') return 0;
    size_t len=strlen(s);
    if(len!=4&&len!=7) return 0;
    uint32_t n=0;
    for(size_t k=1;k<len;k++) {
        unsigned char ch=(unsigned char)s[k]; unsigned d;
        if(ch>='0'&&ch<='9') d=(unsigned)(ch-'0');
        else if(ch>='a'&&ch<='f') d=(unsigned)(ch-'a'+10);
        else if(ch>='A'&&ch<='F') d=(unsigned)(ch-'A'+10);
        else return 0;
        n=len==4?(n<<8)|(d*17u):(n<<4)|d;
    }
    *out=n; return 1;
}
static int scale_format(const tg_widget *w,double v,char *out,size_t cap) {
    if(fabs(v)<1e-12) v=0.0;
    if(!w->themed&&w->resolution>=1.0&&fabs(w->resolution-floor(w->resolution+0.5))<1e-9&&
       fabs(v-floor(v+0.5))<1e-9&&fabs(v)<1e14) {
        double rounded=floor(v+0.5);
        if(fabs(rounded)<1e-12) rounded=0.0;
        int n=snprintf(out,cap,"%.0f",rounded);
        return n>=0&&(size_t)n<cap;
    }
    return format(v,out,cap);
}
static int result(const char *s,char *out,size_t cap) {
    if(strlen(s)>=cap) return TG_ERROR;
    strcpy(out,s); return TG_OK;
}
void tg_extra_defaults(tg_widget *w) {
    if(w->kind==TG_SCALE) {
        w->length=100; w->maximum=1; w->resolution=w->themed?0.0:1.0;
        w->slider_length=14; w->show_value=w->themed?0:1;
        strcpy(w->minimum_text,"0"); strcpy(w->maximum_text,"1.0"); strcpy(w->value_text,"0");
    } else if(w->kind==TG_SCROLLBAR) { w->vertical=1; w->last=1; }
}
int tg_extra_option(tg_widget *w,const char *key,const char *s) {
    if(w->kind!=TG_SCALE&&w->kind!=TG_SCROLLBAR) return -1;
    if(!strcmp(key,"-orient")) {
        if(!strcmp(s,"horizontal")) w->vertical=0;
        else if(!strcmp(s,"vertical")) w->vertical=1;
        else return TG_ERROR;
        return TG_OK;
    }
    if(w->kind!=TG_SCALE) return -1;
    if(!strcmp(key,"-from")) { if(!number(s,&w->minimum)) return TG_ERROR; strcpy(w->minimum_text,s); }
    else if(!strcmp(key,"-to")) { if(!number(s,&w->maximum)) return TG_ERROR; strcpy(w->maximum_text,s); }
    else if(!strcmp(key,"-value")) {
        double v; if(!number(s,&v)) return TG_ERROR;
        w->value=v; strcpy(w->value_text,s);
    } else if(!strcmp(key,"-resolution")) {
        double r; if(!number(s,&r)||r<0) return TG_ERROR;
        w->resolution=r; w->has_resolution=1;
    } else if(!strcmp(key,"-sliderlength")) {
        char *end; errno=0; long v=strtol(s,&end,10);
        if(errno||end==s||*end||v<4||v>4096) return TG_ERROR;
        w->slider_length=(int)v;
    } else if(!strcmp(key,"-showvalue")) {
        if(!strcmp(s,"1")||!strcmp(s,"true")) w->show_value=1;
        else if(!strcmp(s,"0")||!strcmp(s,"false")) w->show_value=0;
        else return TG_ERROR;
    } else if(!strcmp(key,"-troughcolor")) {
        if(!*s) { w->has_trough_color=0; return TG_OK; }
        uint32_t c=0; if(!scale_color(s,&c)) return TG_ERROR;
        w->trough_color=c; w->has_trough_color=1;
    } else if(!strcmp(key,"-length")) {
        char *end; errno=0; long v=strtol(s,&end,10);
        if(errno||end==s||*end||v<1||v>16384) return TG_ERROR;
        w->length=(int)v;
    } else if(!strcmp(key,"-variable")) {
        if(strlen(s)>=sizeof w->variable) return TG_ERROR;
        strcpy(w->variable,s); w->explicit_variable=1;
    } else if(!strcmp(key,"-command")) {
        if(strlen(s)>=sizeof w->command) return TG_ERROR;
        strcpy(w->command,s);
    } else if(!strcmp(key,"-digits")||!strcmp(key,"-sliderrelief")||
              !strcmp(key,"-tickinterval")||!strcmp(key,"-label")||!strcmp(key,"-repeatdelay")||
              !strcmp(key,"-repeatinterval")||!strcmp(key,"-bigincrement")) {
        return TG_OK;
    } else return -1;
    return TG_OK;
}
int tg_extra_query(const tg_widget *w,const char *key,char *out,size_t cap) {
    if(w->kind!=TG_SCALE&&w->kind!=TG_SCROLLBAR) return -1;
    if(!strcmp(key,"-orient")) return result(w->vertical?"vertical":"horizontal",out,cap);
    if(w->kind!=TG_SCALE) return -1;
    if(!strcmp(key,"-from")) return result(w->minimum_text,out,cap);
    if(!strcmp(key,"-to")) return result(w->maximum_text,out,cap);
    if(!strcmp(key,"-value")) return result(w->value_text,out,cap);
    if(!strcmp(key,"-resolution")) return format(w->resolution,out,cap)?TG_OK:TG_ERROR;
    if(!strcmp(key,"-sliderlength")) { int n=snprintf(out,cap,"%d",w->slider_length); return n>=0&&(size_t)n<cap?TG_OK:TG_ERROR; }
    if(!strcmp(key,"-showvalue")) return result(w->show_value?"1":"0",out,cap);
    if(!strcmp(key,"-troughcolor")) {
        if(!w->has_trough_color) return result("",out,cap);
        int n=snprintf(out,cap,"#%06lx",(unsigned long)w->trough_color);
        return n>=0&&(size_t)n<cap?TG_OK:TG_ERROR;
    }
    if(!strcmp(key,"-length")) { int n=snprintf(out,cap,"%d",w->length); return n>=0&&(size_t)n<cap?TG_OK:TG_ERROR; }
    if(!strcmp(key,"-variable")) return result(w->variable,out,cap);
    if(!strcmp(key,"-command")) return result(w->command,out,cap);
    if(!strcmp(key,"-digits")||!strcmp(key,"-sliderrelief")||
       !strcmp(key,"-tickinterval")||!strcmp(key,"-label")) return result("",out,cap);
    return -1;
}
int tg_scale_thumb_size(const tg_widget *w) {
    if(!w||w->kind!=TG_SCALE) return 12;
    int extent=w->vertical?w->rect.h:w->rect.w;
    if(extent<=0) return 12;
    int pref=w->slider_length>0?w->slider_length:14;
    if(pref>18) pref=14;
    return extent<pref?extent:pref;
}
double tg_scale_fraction(const tg_widget *w) {
    if(!w||w->kind!=TG_SCALE) return 0.0;
    double span=w->maximum-w->minimum;
    return span?clamp((w->value-w->minimum)/span,0.0,1.0):0.0;
}
static double scale_quantize(const tg_widget *w,double v) {
    v=clamp(v,w->minimum,w->maximum);
    if(w->resolution>0.0) {
        double base=w->minimum;
        double steps=floor((v-base)/w->resolution+0.5);
        v=clamp(base+steps*w->resolution,w->minimum,w->maximum);
    }
    if(fabs(v)<1e-12) v=0.0;
    return v;
}
int tg_scale_set_value(tg_widget *w,double v) {
    if(!w||w->kind!=TG_SCALE) return 0;
    v=scale_quantize(w,v);
    char text[32];
    if(!scale_format(w,v,text,sizeof text)) return 0;
    int changed=(fabs(w->value-v)>1e-12)||strcmp(w->value_text,text)!=0;
    w->value=v; strcpy(w->value_text,text);
    return changed;
}
int tg_scale_set_at(tg_widget *w,int x,int y,int offset) {
    if(!w||w->kind!=TG_SCALE||w->state==TG_STATE_DISABLED) return 0;
    int extent=w->vertical?w->rect.h:w->rect.w;
    if(extent<=0) return 0;
    int thumb=tg_scale_thumb_size(w);
    int travel=extent-thumb;
    if(travel<=0) return 0;
    int pos=w->vertical?(y-w->rect.y):(x-w->rect.x);
    int thumb_pos=pos-offset;
    if(thumb_pos<0) thumb_pos=0;
    if(thumb_pos>travel) thumb_pos=travel;
    double f=(double)thumb_pos/(double)travel;
    double v=w->minimum+f*(w->maximum-w->minimum);
    return tg_scale_set_value(w,v);
}
int tg_scale_step(tg_widget *w,double delta) {
    if(!w||w->kind!=TG_SCALE||w->state==TG_STATE_DISABLED) return 0;
    double step=w->resolution>0.0?w->resolution:(fabs(w->maximum-w->minimum)/50.0);
    if(step<=0.0) step=1.0;
    double dir=(w->maximum>=w->minimum)?delta:-delta;
    return tg_scale_set_value(w,w->value+dir*step);
}
int tg_scale_sync_widget(tg_widget *w,const char *value) {
    double v;
    if(!w||w->kind!=TG_SCALE||!value||!number(value,&v)) return 0;
    return tg_scale_set_value(w,v);
}
int tg_scale_sync(tg_ui *u,const char *key,const char *value) {
    if(!u||!key||!*key) return 0;
    const char *norm_key=!strncmp(key,"::",2)?key+2:key;
    int changed=0;
    for(size_t k=1;k<u->count;k++) {
        tg_widget *w=&u->items[k];
        if((w->kind!=TG_SCALE&&w->kind!=TG_PROGRESS)||!w->variable[0]) continue;
        const char *norm_var=!strncmp(w->variable,"::",2)?w->variable+2:w->variable;
        if(!strcmp(norm_var,norm_key)) {
            if(w->kind==TG_PROGRESS) changed|=tg_indicator_option(w,"-value",value)==TG_OK;
            else changed|=tg_scale_sync_widget(w,value);
        }
    }
    return changed;
}
int tg_extra_command(tg_widget *w,size_t n,char **a,char *out,size_t cap) {
    if(n<2) return -1;
    if(w->themed&&(!strcmp(a[1],"state")||!strcmp(a[1],"instate"))) {
        int query=!strcmp(a[1],"instate");
        if(!query&&n==2) {
            char states[64]="";
            if(w->state==TG_STATE_DISABLED) strcpy(states,"disabled");
            else if(w->state==TG_STATE_READONLY) strcpy(states,"readonly");
            if(tg_toggle_kind(w->kind)) {
                if(w->checked) strcat(states,*states?" selected":"selected");
                if(w->alternate) strcat(states,*states?" alternate":"alternate");
            }
            return result(states,out,cap);
        }
        if(n!=3) return TG_ERROR;
        const char *s=a[2]; int neg=*s=='!'; if(neg) s++;
        if(!*s&&!query&&!neg) return result("",out,cap);
        if(tg_toggle_kind(w->kind)&&(!strcmp(s,"selected")||!strcmp(s,"alternate"))) {
            int *flag=!strcmp(s,"selected")?&w->checked:&w->alternate;
            if(query) return result((*flag!=0)!=neg?"1":"0",out,cap);
            if((*flag!=0)!=neg) return result("",out,cap);
            char changed[16]; snprintf(changed,sizeof changed,"%s%s",neg?"":"!",s);
            if(result(changed,out,cap)!=TG_OK) return TG_ERROR;
            *flag=!neg; return TG_OK;
        }
        int target=!strcmp(s,"disabled")?TG_STATE_DISABLED:!strcmp(s,"readonly")&&(w->kind==TG_ENTRY||tg_choice_kind(w->kind))?TG_STATE_READONLY:-1;
        if(target<0) return TG_ERROR;
        int active=w->state==target;
        if(query) return result(active!=neg?"1":"0",out,cap);
        if(active!=neg) return result("",out,cap);
        if(!neg&&w->state!=TG_STATE_NORMAL) return TG_ERROR;
        char changed[16]; snprintf(changed,sizeof changed,"%s%s",neg?"":"!",s);
        if(result(changed,out,cap)!=TG_OK) return TG_ERROR;
        w->state=neg?TG_STATE_NORMAL:target; return TG_OK;
    }
    if(w->kind!=TG_SCALE&&w->kind!=TG_SCROLLBAR) return -1;
    if(!strcmp(a[1],"get")&&n==2) {
        if(w->kind==TG_SCALE) return result(w->value_text,out,cap);
        char x[32],y[32]; if(!format(w->first,x,sizeof x)||!format(w->last,y,sizeof y)) return TG_ERROR;
        int k=snprintf(out,cap,"%s %s",x,y); return k>=0&&(size_t)k<cap?TG_OK:TG_ERROR;
    }
    if(w->kind==TG_SCROLLBAR&&(!strcmp(a[1],"fraction")||!strcmp(a[1],"delta")||!strcmp(a[1],"identify"))) {
        double x,y;
        if(n!=4||!number(a[2],&x)||!number(a[3],&y)) return TG_ERROR;
        int extent=w->vertical?w->rect.h:w->rect.w;
        double pos=w->vertical?y:x;
        if(!strcmp(a[1],"identify")) {
            if(x<0||y<0||x>=w->rect.w||y>=w->rect.h||extent<=0) return result("",out,cap);
            int first=(int)(extent*w->first),last=(int)(extent*w->last);
            if(first>=extent) first=extent-1;
            if(last<=first) last=first+1;
            return result(pos<first?"trough1":pos>=last?"trough2":"slider",out,cap);
        }
        double v=extent>0?pos/extent:0;
        if(!strcmp(a[1],"fraction")) v=clamp(v,0,1);
        return format(v,out,cap)?TG_OK:TG_ERROR;
    }
    if(strcmp(a[1],"set")) return -1;
    if(!cap) return TG_ERROR;
    double v,z;
    if(w->kind==TG_SCALE) {
        if(n!=3||!number(a[2],&v)) return TG_ERROR;
        if(w->state==TG_STATE_DISABLED) { out[0]=0; return TG_OK; }
        v=scale_quantize(w,v);
        char text[32]; if(!scale_format(w,v,text,sizeof text)) return TG_ERROR;
        w->value=v; strcpy(w->value_text,text);
    } else {
        if(n!=4||!number(a[2],&v)||!number(a[3],&z)) return TG_ERROR;
        w->first=clamp(v,0,1); w->last=clamp(z,w->first,1);
    }
    out[0]=0; return TG_OK;
}
void tg_extra_paint(const tg_ui *u,const tg_widget *w,tg_canvas *c,uint32_t fg) {
    tg_rect r=w->rect; if(r.w<=0||r.h<=0) return;
    if(w->kind==TG_LABELFRAME) {
        int draw_border=1;
        if(w->themed&&u) {
            char val[64];
            if(tg_style_widget_value(u,w,"-borderwidth",val,sizeof val)>0&&atoi(val)==0) draw_border=0;
            if(tg_style_widget_value(u,w,"-relief",val,sizeof val)>0&&!strcmp(val,"flat")) draw_border=0;
        } else if(w->border_width==0&&w->relief==1) {
            draw_border=0;
        }
        if(draw_border) {
            uint32_t border_color=0xc5d1e0;
            if(w->themed&&u) (void)tg_style_widget_color(u,w,"-bordercolor",&border_color);
            tg_raster_outline(c,r,r,border_color);
        }
        if(w->text[0]) {
            uint32_t label_fg=fg;
            char label_font[256]="";
            if(w->themed&&u) {
                char label_style[96];
                snprintf(label_style,sizeof label_style,"%s.Label",w->style[0]?w->style:"TLabelframe");
                (void)tg_style_lookup_color(u,label_style,"-foreground",0,w->state==TG_STATE_DISABLED,&label_fg);
                (void)tg_style_lookup_value(u,label_style,"-font",0,w->state==TG_STATE_DISABLED,label_font,sizeof label_font);
            }
            if(!tg_text_native_enabled()||tg_text_font_draw(c,r,r.x+6,r.y+2,w->text,label_font[0]?label_font:w->font_spec,label_fg)!=TG_OK)
                tg_text_draw(c,r,r.x+6,r.y+3,w->text,1,label_fg);
        }
        return;
    }
    if(w->kind!=TG_SCALE&&w->kind!=TG_SCROLLBAR) return;
    int extent=w->vertical?r.h:r.w;
    int start,end;
    if(w->kind==TG_SCROLLBAR) {
        start=(int)(extent*w->first); end=(int)(extent*w->last);
        if(start>=extent) start=extent-1;
        if(end<=start) end=start+1;
        tg_rect bar=w->vertical?(tg_rect){r.x,r.y+start,r.w,end-start}:(tg_rect){r.x+start,r.y,end-start,r.h};
        tg_raster_fill(c,r,bar,fg);
        return;
    }
    double f=tg_scale_fraction(w);
    int thumb=tg_scale_thumb_size(w);
    start=(int)((extent-thumb)*f+0.5);
    if(start<0) start=0;
    if(start+thumb>extent) start=extent-thumb;
    end=start+thumb;
    uint32_t track_color=w->has_trough_color?w->trough_color:fg;
    if(w->has_trough_color) {
        int thick=w->vertical?(r.w>=6?4:2):(r.h>=6?4:2);
        tg_rect track=w->vertical
            ?(tg_rect){r.x+(r.w-thick)/2,r.y,thick,r.h}
            :(tg_rect){r.x,r.y+(r.h-thick)/2,r.w,thick};
        tg_raster_fill(c,r,track,track_color);
        if(start>0) {
            tg_rect active=w->vertical
                ?(tg_rect){r.x+(r.w-thick)/2,r.y,thick,start}
                :(tg_rect){r.x,r.y+(r.h-thick)/2,start,thick};
            tg_raster_fill(c,r,active,fg);
        }
    } else {
        tg_rect track=w->vertical?(tg_rect){r.x+r.w/2,r.y,1,r.h}:(tg_rect){r.x,r.y+r.h/2,r.w,1};
        tg_raster_fill(c,r,track,fg);
    }
    tg_rect bar=w->vertical?(tg_rect){r.x,r.y+start,r.w,end-start}:(tg_rect){r.x+start,r.y,end-start,r.h};
    tg_raster_fill(c,r,bar,fg);
}
