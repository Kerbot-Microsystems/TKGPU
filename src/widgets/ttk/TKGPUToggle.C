#include "TKGPUToggle.H"
#include "TKGPUStyle.H"
#include "TKGPUContainers.H"
#include <stdlib.h>
#include "../TKGPUWidgetInternal.H"
#include "TKGPURaster.H"
#include "TKGPUText.H"
#include <string.h>
int tg_toggle_kind(tg_kind k) { return k==TG_CHECKBUTTON||k==TG_RADIOBUTTON; }
void tg_toggle_defaults(tg_widget *w) {
    if(!tg_toggle_kind(w->kind)) return;
    strcpy(w->on_value,"1"); strcpy(w->off_value,"0");
    if(w->kind==TG_RADIOBUTTON) strcpy(w->variable,"::selectedButton");
    w->alternate=1;
}
static int copy(char *dest,size_t cap,const char *s) {
    if(strlen(s)>=cap) return TG_ERROR;
    strcpy(dest,s); return TG_OK;
}
int tg_toggle_option(tg_widget *w,const char *key,const char *s) {
    if(!tg_toggle_kind(w->kind)) return -1;
    if(!strcmp(key,"-variable")) { w->explicit_variable=1; return copy(w->variable,sizeof w->variable,s); }
    if(!strcmp(key,w->kind==TG_CHECKBUTTON?"-onvalue":"-value")) return copy(w->on_value,sizeof w->on_value,s);
    if(w->kind==TG_CHECKBUTTON&&!strcmp(key,"-offvalue")) return copy(w->off_value,sizeof w->off_value,s);
    return -1;
}
int tg_toggle_query(const tg_widget *w,const char *key,char *out,size_t cap) {
    if(!tg_toggle_kind(w->kind)) return -1;
    if(!strcmp(key,"-variable")) return copy(out,cap,w->variable);
    if(!strcmp(key,w->kind==TG_CHECKBUTTON?"-onvalue":"-value")) return copy(out,cap,w->on_value);
    if(w->kind==TG_CHECKBUTTON&&!strcmp(key,"-offvalue")) return copy(out,cap,w->off_value);
    return -1;
}
void tg_toggle_sync(tg_widget *w,const char *value) {
    if(!tg_toggle_kind(w->kind)) return;
    if(!value) { w->alternate=1; return; }
    w->checked=!strcmp(value,w->on_value); w->alternate=0;
}
int tg_ui_toggle_sync(tg_ui *u,const char *key,const char *value) {
    if(!u||(!u->toggle_count&&!u->menu_toggle_count)) return 0;
    int changed=0;
    for(size_t k=1;k<u->count;k++) {
        tg_widget *w=&u->items[k];
        if(u->menu_toggle_count&&w->menu) changed|=tg_menu_sync_widget(w,key,value);
        if(tg_toggle_kind(w->kind)&&!strcmp(!strncmp(w->variable,"::",2)?w->variable+2:w->variable,!strncmp(key,"::",2)?key+2:key)&&(*key||w->kind==TG_RADIOBUTTON)) {
            int old=w->checked,alt=w->alternate; tg_toggle_sync(w,value);
            changed|=old!=w->checked||alt!=w->alternate;
        }
    }
    return changed;
}
int tg_toggle_diameter(const tg_ui *u,const tg_widget *w) {
    char value[TG_STYLE_VALUE],*end;
    if(w->themed&&tg_style_widget_value(u,w,"-indicatordiameter",value,sizeof value)>0) {
        long n=strtol(value,&end,10); if(!*end&&n>=6&&n<=64) return (int)n;
    }
    return 12;
}
void tg_toggle_paint(const tg_ui *u,const tg_widget *w,tg_canvas *c,uint32_t fg) {
    if(w->themed&&u->native_toplevels) {
        tg_rect r=w->rect;
        int d=tg_toggle_diameter(u,w),pad=w->text_padx;
        if(d>r.h-2) d=r.h-2;
        if(d<4||r.w<d+pad) return;
        int selected=w->checked,alternate=w->alternate;
        (void)tg_ttk_state_matches(u,w,"selected",&selected);
        (void)tg_ttk_state_matches(u,w,"alternate",&alternate);
        uint32_t ink=fg,fill=0xffffff,border=fg;
        (void)tg_style_widget_color(u,w,"-indicatorcolor",&fill);
        (void)tg_style_widget_color(u,w,"-bordercolor",&border);
        tg_rect mark={r.x+pad,r.y+(r.h-d)/2,d,d};
        if(w->kind==TG_RADIOBUTTON) {
            tg_raster_ellipse(c,r,mark,fill,1);
            tg_raster_ellipse(c,r,mark,border,0);
            if(selected) {
                int inset=d/4;
                /* Choose a contrasting center against the theme indicator. */
                unsigned bright=((fill>>16)&255u)*299+((fill>>8)&255u)*587+(fill&255u)*114;
                ink=bright>128000?0x202020:0xffffff;
                tg_raster_ellipse(c,r,(tg_rect){mark.x+inset,mark.y+inset,d-2*inset,d-2*inset},ink,1);
            }
        } else {
            tg_raster_fill(c,r,mark,fill);
            tg_raster_outline(c,r,mark,border);
            if(selected) {
                unsigned bright=((fill>>16)&255u)*299+((fill>>8)&255u)*587+(fill&255u)*114;
                ink=bright>128000?0x202020:0xffffff;
                tg_raster_line(c,r,mark.x+2,mark.y+d/2,mark.x+d/2-1,mark.y+d-3,ink);
                tg_raster_line(c,r,mark.x+d/2-1,mark.y+d-3,mark.x+d-2,mark.y+2,ink);
            }
        }
        if(alternate) tg_raster_fill(c,r,(tg_rect){mark.x+3,mark.y+d/2-1,d-6,2},fg);
        int x=mark.x+d+4,tw=0,th=0;
        if(tg_text_font_measure(w->text,w->font_spec,&tw,&th)==TG_OK)
            (void)tg_text_font_draw(c,r,x,r.y+(r.h-th)/2,w->text,w->font_spec,fg);
        else tg_text_draw(c,r,x,r.y+(r.h-7)/2,w->text,1,fg);
        return;
    }
    tg_rect r=w->rect; if(r.w<14||r.h<12) return;
    tg_rect mark={r.x+3,r.y+(r.h-12)/2,12,12};
    if(w->kind==TG_RADIOBUTTON) {
        tg_raster_ellipse(c,r,mark,fg,0);
        if(w->checked) tg_raster_ellipse(c,r,(tg_rect){mark.x+3,mark.y+3,6,6},fg,1);
    } else {
        tg_raster_outline(c,r,mark,fg);
        if(w->checked) { tg_raster_line(c,r,mark.x+2,mark.y+6,mark.x+5,mark.y+9,fg); tg_raster_line(c,r,mark.x+5,mark.y+9,mark.x+10,mark.y+2,fg); }
    }
    if(w->alternate) tg_raster_fill(c,r,(tg_rect){mark.x+3,mark.y+5,6,2},fg);
    tg_text_draw(c,r,r.x+20,r.y+(r.h-7)/2,w->text,1,fg);
}
