#include "TKGPUIndicators.H"
#include "TKGPURaster.H"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
static int is_indicator(const tg_widget *w) { return w->kind==TG_PROGRESS||w->kind==TG_SEPARATOR; }
void tg_indicator_defaults(tg_widget *w) {
    if(!is_indicator(w)) return;
    w->length=100; w->maximum=100;
    strcpy(w->value_text,"0.0"); strcpy(w->maximum_text,"100");
}
static int number(const char *s,double *v) {
    char *end; errno=0; double n=strtod(s,&end);
    if(end==s||*end||errno||!isfinite(n)||n<-1e12||n>1e12||strlen(s)>=32) return 0;
    *v=n; return 1;
}
int tg_indicator_option(tg_widget *w,const char *key,const char *s) {
    if(!is_indicator(w)) return -1;
    if(!strcmp(key,"-orient")) {
        if(!strcmp(s,"horizontal")) w->vertical=0;
        else if(!strcmp(s,"vertical")) w->vertical=1;
        else return TG_ERROR;
    } else if(w->kind==TG_PROGRESS&&!strcmp(key,"-mode")) {
        if(!strcmp(s,"determinate")) w->indeterminate=0;
        else if(!strcmp(s,"indeterminate")) w->indeterminate=1;
        else return TG_ERROR;
    } else if(w->kind==TG_PROGRESS&&!strcmp(key,"-variable")) {
        if(strlen(s)>=sizeof w->variable) return TG_ERROR;
        strcpy(w->variable,s); w->explicit_variable=1;
    } else if(w->kind==TG_PROGRESS&&!strcmp(key,"-value")) {
        if(!number(s,&w->value)) return TG_ERROR;
        strcpy(w->value_text,s);
    } else if(w->kind==TG_PROGRESS&&!strcmp(key,"-maximum")) {
        double v; if(!number(s,&v)||v<=0) return TG_ERROR;
        w->maximum=v; strcpy(w->maximum_text,s);
    } else if(w->kind==TG_PROGRESS&&!strcmp(key,"-length")) {
        char *end; errno=0; long n=strtol(s,&end,10);
        if(errno||end==s||*end||n<1||n>16384) return TG_ERROR;
        w->length=(int)n;
    } else return -1;
    return TG_OK;
}
int tg_indicator_query(const tg_widget *w,const char *key,char *out,size_t cap) {
    if(!is_indicator(w)) return -1;
    const char *s=NULL; char buf[32];
    if(!strcmp(key,"-orient")) s=w->vertical?"vertical":"horizontal";
    else if(w->kind==TG_PROGRESS&&!strcmp(key,"-mode")) s=w->indeterminate?"indeterminate":"determinate";
    else if(w->kind==TG_PROGRESS&&!strcmp(key,"-value")) s=w->value_text;
    else if(w->kind==TG_PROGRESS&&!strcmp(key,"-variable")) s=w->variable;
    else if(w->kind==TG_PROGRESS&&!strcmp(key,"-maximum")) s=w->maximum_text;
    else if(w->kind==TG_PROGRESS&&!strcmp(key,"-length")) { snprintf(buf,sizeof buf,"%d",w->length); s=buf; }
    if(!s) return -1;
    if(strlen(s)>=cap) return TG_ERROR;
    strcpy(out,s); return TG_OK;
}
int tg_indicator_command(tg_widget *w,size_t n,char **a,char *out,size_t cap) {
    if(w->kind!=TG_PROGRESS||strcmp(a[1],"step")) return -1;
    double step=1;
    if(n<2||n>3||(n==3&&!number(a[2],&step))||!cap) return TG_ERROR;
    double v=fmod(w->value+step,w->maximum);
    char buf[32]; int k=snprintf(buf,sizeof buf,"%.15g",v);
    if(k<0||(size_t)k>=sizeof buf-2) return TG_ERROR;
    if(!strchr(buf,'.')&&!strchr(buf,'e')&&!strchr(buf,'E')) strcat(buf,".0");
    w->value=v; strcpy(w->value_text,buf); out[0]=0; return TG_OK;
}
void tg_indicator_paint(const tg_widget *w,tg_canvas *c,uint32_t fg) {
    tg_rect r=w->rect;
    if(r.w<=0||r.h<=0) return;
    if(w->kind==TG_SEPARATOR) {
        tg_rect line=w->vertical?(tg_rect){r.x+r.w/2,r.y,1,r.h}:(tg_rect){r.x,r.y+r.h/2,r.w,1};
        tg_raster_fill(c,r,line,fg); return;
    }
    if(w->kind!=TG_PROGRESS||w->maximum<=0) return;
    int extent=w->vertical?r.h:r.w, start=0, count=0;
    if(w->indeterminate) {
        count=extent/4; if(count<1) count=1;
        double phase=fabs(fmod(w->value,2*w->maximum))/w->maximum;
        if(phase>1) phase=2-phase;
        start=(int)((extent-count)*phase);
    } else {
        double ratio=w->value/w->maximum;
        if(ratio<0) ratio=0;
        if(ratio>1) ratio=1;
        count=(int)(extent*ratio);
    }
    tg_rect bar=w->vertical?(tg_rect){r.x,r.y+r.h-start-count,r.w,count}:(tg_rect){r.x+start,r.y,count,r.h};
    tg_raster_fill(c,r,bar,fg);
}
