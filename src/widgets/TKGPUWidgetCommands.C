#include "TKGPUWidgetInternal.H"
#include "TKGPUText.H"
#include "TKGPUUtf8.H"
#include "../core/TKGPUList.H"
#include "controls/TKGPUControls.H"
#include "listbox/TKGPUListbox.H"
#include "text/TKGPUTextWidget.H"
#include "ttk/TKGPUStyle.H"
#include "ttk/TKGPUIndicators.H"
#include "ttk/TKGPUExtra.H"
#include "ttk/TKGPUChoice.H"
#include "ttk/TKGPUNotebook.H"
#include "ttk/TKGPUToggle.H"
#include "ttk/TKGPUContainers.H"
#include "ttk/tree/TKGPUTree.H"
#include "canvas/TKGPUCanvasItems.H"
#include "image/TKGPUImageRegistry.H"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
static int out(char *b,size_t cap,const char *s,int rc) {
    if(!cap) return TG_ERROR;
    snprintf(b,cap,"%s",s); return rc;
}
static int dim(const char *s,int *v) {
    char *end; errno=0; long n=strtol(s,&end,10);
    if(errno||end==s||*end||n<0||n>16384) return 0;
    *v=(int)n; return 1;
}
/* Tk accepts either one distance or separate leading/trailing distances. */
static int pad_value(const char *s,int *v,int *extra) {
    int x,y;
    if(dim(s,&x)) { *v=x; *extra=0; return 1; }
    const char *p=s; char first[32],second[32],rest[2];
    if(tg_list_next(&p,first,sizeof first)!=1||tg_list_next(&p,second,sizeof second)!=1||
       tg_list_next(&p,rest,sizeof rest)!=0||!dim(first,&x)||!dim(second,&y)) return 0;
    *v=x; *extra=y-x; return 1;
}
static void pad_info(char *b,size_t cap,int start,int extra) {
    if(extra) snprintf(b,cap,"{%d %d}",start,start+extra);
    else snprintf(b,cap,"%d",start);
}
static int anchor(const char *s) {
    if(!strcmp(s,"c")) return 5;
    static const char *names[]={"nw","n","ne","w","center","e","sw","s","se"};
    for(int k=0;k<9;k++) if(!strcmp(s,names[k])) return k+1;
    return 0;
}
static const char *anchor_name(int a,int fallback) {
    static const char *names[]={"nw","n","ne","w","center","e","sw","s","se"};
    return names[(a?a:fallback)-1];
}
static int offset_value(const char *s,int *v) {
    char *end; errno=0; long n=strtol(s,&end,10);
    if(errno||end==s||*end||n< -16384||n>16384) return 0;
    *v=(int)n; return 1;
}
static int fraction(const char *s,double *v) {
    char *end; errno=0; double n=strtod(s,&end);
    if(errno||end==s||*end||!(n>=-16&&n<=16)) return 0;
    *v=n; return 1;
}
static int color(const char *s,uint32_t *v) {
    static const struct { const char *name; uint32_t color; } named[]={
        {"black",0},{"white",0xffffff},{"red",0xff0000},{"green",0x00ff00},
        {"blue",0xff},{"gray",0xbebebe},{"grey",0xbebebe},{"yellow",0xffff00},
        {"cyan",0x00ffff},{"magenta",0xff00ff}};
    for(size_t k=0;k<sizeof named/sizeof named[0];k++) if(!strcmp(s,named[k].name)) { *v=named[k].color; return 1; }
    if(s[0]!='#') return 0;
    if(strlen(s)==4) {
        uint32_t n=0;
        for(int k=1;k<4;k++) {
            char ch=s[k]; unsigned digit;
            if(ch>='0'&&ch<='9') digit=(unsigned)(ch-'0');
            else if(ch>='a'&&ch<='f') digit=(unsigned)(ch-'a'+10);
            else if(ch>='A'&&ch<='F') digit=(unsigned)(ch-'A'+10);
            else return 0;
            n=(n<<8)|(digit*17);
        }
        *v=n; return 1;
    }
    if(strlen(s)!=7) return 0;
    uint32_t n=0;
    for(int k=1;k<7;k++) {
        unsigned char c=(unsigned char)s[k]; unsigned d;
        if(c>='0'&&c<='9') d=c-'0';
        else if(c>='a'&&c<='f') d=c-'a'+10;
        else if(c>='A'&&c<='F') d=c-'A'+10;
        else return 0;
        n=(n<<4)|d;
    }
    *v=n; return 1;
}
/* Eight digits are an explicit TKGPU RRGGBBAA extension. */
static int background_color(tg_widget *w,const char *s,uint32_t *rgb) {
    if(strlen(s)==9&&s[0]=='#') {
        if(w->kind==TG_MENU||w->kind==TG_TOPLEVEL) return 0;
        char base[8]; memcpy(base,s,7); base[7]=0;
        char tail[8]={'#','0','0','0','0',s[7],s[8],0}; uint32_t a;
        if(!color(base,rgb)||!color(tail,&a)) return 0;
        w->color_alpha=(unsigned char)a; return 1;
    }
    if(!color(s,rgb)) return 0;
    w->color_alpha=255; return 1;
}
/* Resource subset used by Kervix: global resource, class resource and
   exact widget path. Preserve rule order so equal priority is last-wins. */
static const char *option_lookup(const tg_ui *u,const char *path,const char *klass,const char *resource,const char *resource_class) {
    const char *best=NULL; int priority=-1;
    char a[256],b[256],c[256],d[256],e[TG_WIDGET_PATH_CAP+256],f[TG_WIDGET_PATH_CAP+256];
    snprintf(a,sizeof a,"*%s",resource); snprintf(b,sizeof b,"*%s",resource_class);
    snprintf(c,sizeof c,"*%s.%s",klass,resource); snprintf(d,sizeof d,"*%s.%s",klass,resource_class);
    snprintf(e,sizeof e,"%s.%s",path,resource); snprintf(f,sizeof f,"%s.%s",path,resource_class);
    for(size_t k=0;k<u->option_count;k++) {
        const tg_option_rule *r=&u->options[k];
        if(r->priority>=priority&&(!strcmp(r->pattern,a)||!strcmp(r->pattern,b)||
           !strcmp(r->pattern,c)||!strcmp(r->pattern,d)||!strcmp(r->pattern,e)||!strcmp(r->pattern,f))) {
            priority=r->priority; best=r->value;
        }
    }
    return best;
}
static const char *resource_class_name(tg_kind kind,int themed) {
    static const char *classic[]={"","Frame","Label","Button","Canvas","Entry","Toplevel","Progressbar","Separator","Scale","Scrollbar","Labelframe","Combobox","Spinbox","Notebook","Treeview","Checkbutton","Radiobutton","Panedwindow","Menubutton","Sizegrip","Menu","Listbox","Text","Message"};
    static const char *ttk[]={"","TFrame","TLabel","TButton","Canvas","TEntry","Toplevel","TProgressbar","TSeparator","TScale","TScrollbar","TLabelframe","TCombobox","TSpinbox","TNotebook","Treeview","TCheckbutton","TRadiobutton","TPanedwindow","TMenubutton","TSizegrip","Menu","Listbox","Text","Message"};
    return themed?ttk[kind]:classic[kind];
}
static int option_command(tg_ui *u,size_t n,char **a,char *b,size_t cap) {
    if(strcmp(a[0],"option")) return -1;
    if(n==2&&!strcmp(a[1],"clear")) { u->option_count=0; return out(b,cap,"",TG_OK); }
    if((n==4||n==5)&&!strcmp(a[1],"add")) {
        int priority=80;
        if(n==5) {
            if(!strcmp(a[4],"widgetDefault")) priority=20;
            else if(!strcmp(a[4],"startupFile")) priority=40;
            else if(!strcmp(a[4],"userDefault")) priority=60;
            else if(!strcmp(a[4],"interactive")) priority=80;
            else if(!dim(a[4],&priority)||priority>100) return out(b,cap,"invalid option priority",TG_ERROR);
        }
        if(!*a[2]||strlen(a[2])>=128||strlen(a[3])>=256) return out(b,cap,"option resource too long",TG_ERROR);
        size_t at=u->option_count;
        for(size_t k=0;k<u->option_count;k++) if(u->options[k].priority==priority&&!strcmp(u->options[k].pattern,a[2])) { at=k; break; }
        if(at==u->option_count&&at==TG_OPTION_LIMIT) return out(b,cap,"option database full",TG_ERROR);
        tg_option_rule rule={0}; strcpy(rule.pattern,a[2]); strcpy(rule.value,a[3]); rule.priority=priority;
        if(at<u->option_count) { memmove(u->options+at,u->options+at+1,(u->option_count-at-1)*sizeof rule); u->option_count--; }
        u->options[u->option_count++]=rule; return out(b,cap,"",TG_OK);
    }
    if(n==5&&!strcmp(a[1],"get")) {
        const tg_widget *w=tg_ui_get(u,tg_ui_find(u,a[2]));
        if(!w) return out(b,cap,"unknown option target",TG_ERROR);
        const char *value=option_lookup(u,w->path,resource_class_name(w->kind,w->themed),a[3],a[4]);
        return out(b,cap,value?value:"",TG_OK);
    }
    return out(b,cap,"unsupported option command",TG_ERROR);
}
static int common_option(tg_ui *u,tg_widget *w,const char *key,const char *value) {
    if(w->kind==TG_COMBOBOX&&!strcmp(key,"-height")) return tg_choice_option(w,key,value);
    if(!strcmp(key,"-compound")) {
        if(!strcmp(value,"center")) w->compound_center=1;
        else if(!strcmp(value,"none")||!strcmp(value,"left")||!strcmp(value,"right")||!strcmp(value,"top")||!strcmp(value,"bottom")||!strcmp(value,"text")||!strcmp(value,"image")) w->compound_center=0;
        else return TG_ERROR;
        return TG_OK;
    }
    if(!strcmp(key,"-image")) { if(strlen(value)>=sizeof w->image_name) return TG_ERROR; strcpy(w->image_name,value); return TG_OK; }
    if(!strcmp(key,"-show")) { size_t count; if(strlen(value)>=sizeof w->show||tg_utf8_count((const unsigned char *)value,strlen(value),&count)!=TG_OK||count>1) return TG_ERROR; strcpy(w->show,value); return TG_OK; }
    if(!strcmp(key,"-cursor")) { if(strlen(value)>=sizeof w->cursor_name) return TG_ERROR; strcpy(w->cursor_name,value); return TG_OK; }
    if(!strcmp(key,"-exportselection")||!strcmp(key,"-takefocus")) { int v; if(!strcmp(value,"1")||!strcmp(value,"true")) v=1; else if(!*value||!strcmp(value,"0")||!strcmp(value,"false")) v=0; else return TG_ERROR; if(!strcmp(key,"-takefocus")) w->takefocus=v; else w->exportselection=v; return TG_OK; }
    if(!strcmp(key,"-activebackground")) { if(!color(value,&w->active_bg)) return TG_ERROR; w->has_active_bg=1; return TG_OK; }
    if(!strcmp(key,"-activeforeground")) { if(!color(value,&w->active_fg)) return TG_ERROR; w->has_active_fg=1; return TG_OK; }
    if(!strcmp(key,"-disabledforeground")) { if(!color(value,&w->disabled_fg)) return TG_ERROR; w->has_disabled_fg=1; return TG_OK; }
    if(!strcmp(key,"-highlightthickness")) return dim(value,&w->highlight_width)&&w->highlight_width<=32?TG_OK:TG_ERROR;
    if(!strcmp(key,"-highlightcolor")) { if(!color(value,&w->highlight_color)) return TG_ERROR; w->has_highlight_color=1; return TG_OK; }
    if(!strcmp(key,"-highlightbackground")) { if(!color(value,&w->highlight_background)) return TG_ERROR; w->has_highlight_background=1; return TG_OK; }
    if(!strcmp(key,"-font")) { if(strlen(value)>=sizeof w->font) return TG_ERROR; strcpy(w->font,value); const char *spec=tg_ui_font_spec(u,value); if(strlen(spec)>=sizeof w->font_spec) return TG_ERROR; strcpy(w->font_spec,spec); return TG_OK; }
    if(!strcmp(key,"-justify")) { const char *names[]={"left","center","right"}; for(int k=0;k<3;k++) if(!strcmp(value,names[k])) { w->justify=k; return TG_OK; } return TG_ERROR; }
    if(!strcmp(key,"-anchor")) { int v=anchor(value); if(!v) return TG_ERROR; w->text_anchor=v; return TG_OK; }
    if(!strcmp(key,"-wraplength")) return dim(value,&w->wraplength)?TG_OK:TG_ERROR;
    if(!strcmp(key,"-insertbackground")||!strcmp(key,"-selectbackground")||!strcmp(key,"-selectforeground")||!strcmp(key,"-inactiveselectbackground")||!strcmp(key,"-readonlybackground")||!strcmp(key,"-disabledbackground")||!strcmp(key,"-selectcolor")) { uint32_t c; if(*value&&!color(value,&c)) return TG_ERROR; return TG_OK; }
    if(!strcmp(key,"-insertborderwidth")||!strcmp(key,"-selectborderwidth")||!strcmp(key,"-insertwidth")||!strcmp(key,"-insertofftime")||!strcmp(key,"-insertontime")) { int v; if(!dim(value,&v)) return TG_ERROR; return TG_OK; }
    if(!strcmp(key,"-insertunfocussed")||!strcmp(key,"-tabstyle")||!strcmp(key,"-blockcursor")||!strcmp(key,"-setgrid")||!strcmp(key,"-startline")||!strcmp(key,"-endline")||!strcmp(key,"-indicatoron")) return TG_OK;
    if(w->kind==TG_TEXT&&!strcmp(key,"-wrap")) {
        if(!strcmp(value,"none")) { w->wrap=0; return TG_OK; }
        if(!strcmp(value,"char")) { w->wrap=1; return TG_OK; }
        if(!strcmp(value,"word")) { w->wrap=2; return TG_OK; }
        return TG_ERROR;
    }
    if(!strcmp(key,"-activestyle")||!strcmp(key,"-selectmode")||!strcmp(key,"-listvariable")) return TG_OK;
    if(!strcmp(key,"-yscrollcommand")||!strcmp(key,"-xscrollcommand")||!strcmp(key,"-scrollregion")) {
        /* Canvas options are stateful: never accept them without applying
           them to the candidate widget. Other legacy controls keep their
           existing compatibility behavior. */
        if(w->kind==TG_CANVAS||w->kind==TG_TREEVIEW||((w->kind==TG_TEXT||w->kind==TG_LISTBOX)&&(!strcmp(key,"-yscrollcommand")||!strcmp(key,"-xscrollcommand")))) return tg_canvas_option(w,key,value);
        return TG_OK;
    }
    if(tg_textvar_editable(w->kind)) {
        if(!strcmp(key,"-validate")) {
            static const char *modes[]={"none","focus","focusin","focusout","key","all"};
            for(unsigned k=0;k<6;k++) if(!strcmp(value,modes[k])) { w->validate_mode=(unsigned char)k; return TG_OK; }
            return TG_ERROR;
        }
        if(!strcmp(key,"-validatecommand")||!strcmp(key,"-vcmd")||
           !strcmp(key,"-invalidcommand")||!strcmp(key,"-invcmd")) {
            char *dest=key[1]=='v'?w->validate_command:w->invalid_command;
            if(strlen(value)>=sizeof w->validate_command) return TG_ERROR;
            strcpy(dest,value); return TG_OK;
        }
    } else if(!strcmp(key,"-validate")||!strcmp(key,"-validatecommand")||!strcmp(key,"-vcmd")||!strcmp(key,"-invalidcommand")||!strcmp(key,"-invcmd")) return TG_OK;
    if(!strcmp(key,"-spacing1")||!strcmp(key,"-spacing2")||!strcmp(key,"-spacing3")||!strcmp(key,"-tabs")) return TG_OK;
    if(!strcmp(key,"-labelanchor")&&w->kind==TG_LABELFRAME) return TG_OK;
    if(w->kind==TG_TEXT) {
        int history=tg_document_history_option(NULL,key,value);
        if(history>=0) return history;
    }
    if(!strcmp(key,"-confine")||!strcmp(key,"-undo")||!strcmp(key,"-autoseparators")||!strcmp(key,"-maxundo")) return TG_OK;
    if(!strcmp(key,"-padx")) return dim(value,&w->text_padx)?TG_OK:TG_ERROR;
    if(!strcmp(key,"-pady")) return dim(value,&w->text_pady)?TG_OK:TG_ERROR;
    if(!strcmp(key,"-padding")) {
        int p[4]={0}; int n=sscanf(value,"%d %d %d %d",&p[0],&p[1],&p[2],&p[3]);
        if(n<1) { if(!*value) { w->has_ttk_padding=0; memset(w->ttk_padding,0,sizeof w->ttk_padding); return TG_OK; } return TG_ERROR; }
        for(int k=0;k<n;k++) if(p[k]<0||p[k]>TG_DIM_LIMIT/4) return TG_ERROR;
        w->has_ttk_padding=1;
        w->ttk_padding[0]=p[0]; w->ttk_padding[1]=n>1?p[1]:p[0];
        w->ttk_padding[2]=n>2?p[2]:p[0]; w->ttk_padding[3]=n>3?p[3]:w->ttk_padding[1];
        if(w->kind!=TG_FRAME&&w->kind!=TG_LABELFRAME) { w->text_padx=p[0]; w->text_pady=w->ttk_padding[1]; }
        return TG_OK;
    }
    int canvas=tg_canvas_option(w,key,value); if(canvas>=0) return canvas;
    if(!strcmp(key,"-borderwidth")||!strcmp(key,"-bd")) {
        int v; if(!dim(value,&v)||v>64) return TG_ERROR;
        w->border_width=v; return TG_OK;
    }
    if(!strcmp(key,"-relief")) {
        static const char *names[]={"flat","raised","sunken","groove","ridge","solid"};
        for(int k=0;k<6;k++) if(!strcmp(value,names[k])) { w->relief=k; return TG_OK; }
        return TG_ERROR;
    }
    if(!strcmp(key,"-opacity")||!strcmp(key,"-backgroundalpha")||!strcmp(key,"-groupopacity")) {
        char *end; errno=0; double v=strtod(value,&end);
        if(w->kind==TG_MENU||w->kind==TG_TOPLEVEL||errno||end==value||*end||!(v>=0&&v<=1)) return TG_ERROR;
        unsigned char a=(unsigned char)(v*255.0+0.5);
        if(!strcmp(key,"-opacity")) w->opacity=a;
        else if(!strcmp(key,"-groupopacity")) w->group_opacity=a;
        else w->background_alpha=a;
        return TG_OK;
    }
    if(!strcmp(key,"-foreground")||!strcmp(key,"-fg")) {
        if(!color(value,&w->fg)) return TG_ERROR;
        w->explicit_fg=1; return TG_OK;
    }
    if(!strcmp(key,"-width")||!strcmp(key,"-height")) {
        int n; if(!dim(value,&n)) return TG_ERROR;
        if(!strcmp(key,"-width")) { w->option_width=n; w->has_width=1; }
        else { w->option_height=n; w->has_height=1; }
        return TG_OK;
    }
    return -1;
}
static int character_size(tg_kind k) {
    return k==TG_LABEL||k==TG_BUTTON||k==TG_ENTRY||k==TG_TEXT||k==TG_LISTBOX||
        k==TG_COMBOBOX||k==TG_SPINBOX||k==TG_CHECKBUTTON||k==TG_RADIOBUTTON||k==TG_MENUBUTTON;
}
static void common_apply(tg_ui *u,tg_widget *w,const tg_widget *opt) {
    w->compound_center=opt->compound_center;
    w->validate_mode=opt->validate_mode;
    strcpy(w->validate_command,opt->validate_command); strcpy(w->invalid_command,opt->invalid_command);
    strcpy(w->image_name,opt->image_name); strcpy(w->show,opt->show); strcpy(w->cursor_name,opt->cursor_name); w->exportselection=opt->exportselection; w->takefocus=opt->takefocus;
    w->active_bg=opt->active_bg; w->active_fg=opt->active_fg; w->disabled_fg=opt->disabled_fg; w->has_active_bg=opt->has_active_bg; w->has_active_fg=opt->has_active_fg; w->has_disabled_fg=opt->has_disabled_fg;
    w->highlight_width=opt->highlight_width; w->highlight_color=opt->highlight_color; w->highlight_background=opt->highlight_background; w->has_highlight_color=opt->has_highlight_color; w->has_highlight_background=opt->has_highlight_background;
    strcpy(w->font,opt->font); strcpy(w->font_spec,opt->font_spec); w->text_anchor=opt->text_anchor; w->justify=opt->justify; w->wraplength=opt->wraplength; w->text_padx=opt->text_padx; w->text_pady=opt->text_pady; w->has_ttk_padding=opt->has_ttk_padding; memcpy(w->ttk_padding,opt->ttk_padding,sizeof w->ttk_padding);
    w->fg=opt->fg; w->explicit_fg=opt->explicit_fg; w->relief=opt->relief; w->border_width=opt->border_width;
    w->canvas_region=opt->canvas_region; w->canvas_region_set=opt->canvas_region_set;
    w->canvas_x=opt->canvas_x; w->canvas_y=opt->canvas_y;
    w->canvas_xincrement=opt->canvas_xincrement; w->canvas_yincrement=opt->canvas_yincrement;
    if(strcmp(w->canvas_xscroll,opt->canvas_xscroll)) w->canvas_scroll_sent&=~1u;
    if(strcmp(w->canvas_yscroll,opt->canvas_yscroll)) w->canvas_scroll_sent&=~2u;
    strcpy(w->canvas_xscroll,opt->canvas_xscroll); strcpy(w->canvas_yscroll,opt->canvas_yscroll);
    tg_canvas_view_clamp(w);
    w->ttk_state_set=opt->ttk_state_set; w->ttk_state_clear=opt->ttk_state_clear;
    w->opacity=opt->opacity; w->background_alpha=opt->background_alpha; w->group_opacity=opt->group_opacity; w->color_alpha=opt->color_alpha;
    w->option_width=opt->option_width; w->option_height=opt->option_height;
    w->has_width=opt->has_width; w->has_height=opt->has_height;
    if(u->native_toplevels&&w->themed&&(w->kind==TG_LABEL||w->kind==TG_BUTTON||tg_toggle_kind(w->kind))) {
        const char *style=w->style[0]?w->style:w->kind==TG_LABEL?"TLabel":w->kind==TG_RADIOBUTTON?"TRadiobutton":w->kind==TG_CHECKBUTTON?"TCheckbutton":"TButton";
        char value[384];
        if(!w->font[0]&&tg_style_lookup_value(u,style,"-font",0,w->state==TG_STATE_DISABLED,value,sizeof value)>0) {
            const char *spec=tg_ui_font_spec(u,value);
            if(strlen(spec)<sizeof w->font_spec) strcpy(w->font_spec,spec);
        }
        if(!w->has_ttk_padding&&tg_style_lookup_value(u,style,"-padding",0,0,value,sizeof value)>0) {
            int x=0,y=0,n=sscanf(value,"%d %d",&x,&y);
            if(n>0&&x>=0&&x<=TG_DIM_LIMIT/4&&(n==1||(y>=0&&y<=TG_DIM_LIMIT/4))) {
                w->text_padx=x; w->text_pady=n==1?x:y;
            }
        }
    }
    tg_rect *r=w->layout==TG_GRID?&w->grid_natural:&w->request;
    int place_width=r->w,place_height=r->h;
    if(u->native_toplevels&&w->layout==TG_PLACE) { r->w=w->place_natural.w; r->h=w->place_natural.h; }
    int chars=character_size(w->kind)&&!w->image_name[0];
    if(u->native_toplevels&&w->kind==TG_SEPARATOR) {
        r->w=w->vertical?2:1; r->h=w->vertical?1:2;
    }
    if(w->text[0]&&(w->kind==TG_LABEL||w->kind==TG_BUTTON)) {
        int tw,th;
        if(tg_text_native_enabled()&&tg_text_layout_measure(w->text,w->font_spec,w->wraplength,&tw,&th)==TG_OK) {
            int lines=1;
            r->w=(w->wraplength>0&&tw>w->wraplength?w->wraplength:tw)+2*w->text_padx+6;
            r->h=th*(lines>0?lines:1)+2*w->text_pady+4;
        }
    }
    if(u->native_toplevels&&tg_toggle_kind(w->kind)) {
        int d=tg_toggle_diameter(u,w),tw=0,th=0;
        if(tg_text_font_measure(w->text,w->font_spec,&tw,&th)!=TG_OK) {
            tw=(int)strlen(w->text)*6; th=12;
        }
        r->w=d+4+tw+2*w->text_padx;
        r->h=(d>th?d:th)+2*w->text_pady;
    }
    if(w->has_width) {
        int px=chars?w->option_width*6+6:w->option_width;
        if(u->native_toplevels&&chars&&w->kind==TG_BUTTON) {
            int zero=6,th=12;
            if(tg_text_native_enabled()) (void)tg_text_font_measure("0",w->font_spec,&zero,&th);
            if(zero<1) zero=6;
            if(zero>256) zero=256;
            int inset=w->text_padx+w->border_width+w->highlight_width;
            if(w->option_width>0) {
                int64_t requested=(int64_t)w->option_width*zero+2*inset;
                px=requested>TG_DIM_LIMIT?TG_DIM_LIMIT:(int)requested;
            } else {
                int tw=0;
                if(tg_text_layout_measure(w->text,w->font_spec,w->wraplength,&tw,&th)==TG_OK)
                    px=tw+2*inset;
                else px=r->w;
            }
        }
        r->w=px>16384?16384:px;
    }
    if(w->kind==TG_ENTRY&&w->font[0]&&!w->has_height) { int tw,th; if(tg_text_font_measure("Mg",w->font_spec,&tw,&th)==TG_OK) r->h=th+2*(w->border_width+w->highlight_width+w->text_pady); }
    if(w->has_height) {
        int px=w->kind==TG_TREEVIEW?(w->option_height*22+28):(chars?w->option_height*12+4:w->option_height);
        if(w->kind==TG_TEXT&&tg_text_native_enabled()) {
            int width=0,height=0;
            if(tg_text_font_measure("Mg",w->font_spec,&width,&height)==TG_OK&&height>0&&height<512) {
                /* Match the text painter's document_row_height; bitmap-era
                   12px rows clip native-font chat messages after wrapping. */
                int64_t requested=(int64_t)w->option_height*(height+2)+4;
                px=requested>TG_DIM_LIMIT?TG_DIM_LIMIT:(int)requested;
            }
        }
        if(u->native_toplevels&&chars&&(w->kind==TG_BUTTON||w->kind==TG_LABEL)) {
            int tw=0,th=14;
            if(tg_text_native_enabled()) (void)tg_text_font_measure("Mg",w->font_spec,&tw,&th);
            if(th<1) th=14;
            if(th>256) th=256;
            int inset=w->text_pady+w->border_width+w->highlight_width;
            if(w->option_height>0) {
                int64_t requested=(int64_t)w->option_height*th+2*inset;
                px=requested>TG_DIM_LIMIT?TG_DIM_LIMIT:(int)requested;
            } else {
                px=r->h;
            }
        }
        r->h=px>16384?16384:px;
    }
    if(w->layout==TG_PLACE) {
        if(u->native_toplevels) { w->place_natural.w=r->w; w->place_natural.h=r->h; }
        if(w->place_flags&1) r->w=place_width;
        if(w->place_flags&2) r->h=place_height;
    }
}
static int manager_matches(tg_layout layout,const char *manager) {
    if(!strcmp(manager,"grid")) return layout==TG_GRID;
    if(!strcmp(manager,"place")) return layout==TG_PLACE;
    return layout==TG_PACK_TOP||layout==TG_PACK_LEFT||layout==TG_PACK_BOTTOM||layout==TG_PACK_RIGHT;
}
static int grid_track(tg_ui *u,size_t n,char **a,char *b,size_t cap) {
    if(n<4||(n>5&&n%2)) return out(b,cap,"invalid grid track options",TG_ERROR);
    tg_widget *parent=tg_lookup(u,tg_ui_find(u,a[2]));
    int index;
    if(!parent||!dim(a[3],&index)||index>=256) return out(b,cap,"invalid grid track",TG_ERROR);
    int row=!strcmp(a[1],"rowconfigure");
    uint16_t *weights=row?parent->grid_row_weight:parent->grid_col_weight;
    uint16_t *mins=row?parent->grid_row_min:parent->grid_col_min;
    uint16_t *uniforms=row?parent->grid_row_uniform:parent->grid_col_uniform;
    int group=uniforms[index];
    uint16_t *pads=row?parent->grid_row_pad:parent->grid_col_pad;
    int weight=weights[index],minimum=mins[index],pad=pads[index];
    if(n==4) {
        int z=snprintf(b,cap,"-minsize %d -weight %d -pad %d -uniform {%s}",minimum,weight,pad,group?u->uniform_names[group-1]:"");
        return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR;
    }
    if(n==5) {
        if(!strcmp(a[4],"-uniform")) return out(b,cap,group?u->uniform_names[group-1]:"",TG_OK);
        int v=!strcmp(a[4],"-weight")?weight:!strcmp(a[4],"-minsize")?minimum:!strcmp(a[4],"-pad")?pad:-1;
        if(v<0) goto bad;
        int z=snprintf(b,cap,"%d",v); return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR;
    }
    for(size_t k=4;k<n;k+=2) {
        if(!strcmp(a[k],"-weight")) { if(!dim(a[k+1],&weight)) goto bad; }
        else if(!strcmp(a[k],"-minsize")) { if(!dim(a[k+1],&minimum)) goto bad; }
        else if(!strcmp(a[k],"-pad")) { if(!dim(a[k+1],&pad)) goto bad; }
        else if(!strcmp(a[k],"-uniform")) {
            const char *name=a[k+1]; group=0;
            if(*name) {
                if(strlen(name)>=64) goto bad;
                unsigned at=0; while(at<u->uniform_count&&strcmp(u->uniform_names[at],name)) at++;
                if(at==128) goto bad;
                if(at==u->uniform_count) { strcpy(u->uniform_names[at],name); u->uniform_count++; }
                group=(int)at+1;
            }
        }
        else return out(b,cap,"unsupported grid track option",TG_ERROR);
    }
    weights[index]=(uint16_t)weight; mins[index]=(uint16_t)minimum; uniforms[index]=(uint16_t)group; pads[index]=(uint16_t)pad;
    return out(b,cap,"",TG_OK);
bad: return out(b,cap,"invalid grid track value",TG_ERROR);
}
static const char *manager_name(const tg_ui *u,const tg_widget *w) {
    const tg_widget *p=tg_ui_get(u,w->parent);
    if(p&&p->kind==TG_CANVAS&&w->layout==TG_FREE&&tg_canvas_window_item(u,p->id,w->id)) return "canvas";
    if(p&&(p->kind==TG_NOTEBOOK||p->kind==TG_PANEDWINDOW)) {
        for(unsigned k=0;k<p->tab_count;k++) if(p->tabs[k]==w->id)
            return p->kind==TG_NOTEBOOK?"notebook":"panedwindow";
    }
    return w->layout==TG_GRID?"grid":w->layout==TG_PLACE?"place":
        manager_matches(w->layout,"pack")?"pack":"";
}
static int mapped(const tg_ui *u,const tg_widget *w) {
    if(w->id==1||(u->native_toplevels&&w->kind==TG_TOPLEVEL)) return 1;
    const tg_widget *p=tg_ui_get(u,w->parent);
    if(w->kind==TG_MENU) return tg_menu_mapped(u,w->id);
    if(!p) return 0;
    if(p->kind==TG_NOTEBOOK) return p->selected==w->id&&w->tab_state==1;
    if(p->kind==TG_CANVAS&&w->layout==TG_FREE) {
        const tg_canvas_item *item=tg_canvas_window_item(u,p->id,w->id);
        return item&&!item->hidden;
    }
    return manager_name(u,w)[0]!=0;
}
static int viewable(const tg_ui *u,const tg_widget *w) {
    if(w&&w->kind==TG_MENU) return tg_menu_mapped(u,w->id);
    while(w) {
        if(!mapped(u,w)) return 0;
        if(w->id==1||(u->native_toplevels&&w->kind==TG_TOPLEVEL)) return 1;
        w=tg_ui_get(u,w->parent);
    }
    return 0;
}
static int geometry_info(const tg_ui *u,const tg_widget *w,const char *manager,char *b,size_t cap) {
    if(!manager_matches(w->layout,manager)) return out(b,cap,"widget is not managed by requested manager",TG_ERROR);
    const tg_widget *parent=tg_ui_get(u,w->parent);
    if(!parent||!cap) return TG_ERROR;
    b[0]=0;
    if(!tg_list_append(b,cap,"-in")||!tg_list_append(b,cap,parent->path)) return TG_ERROR;
    char tail[256]; tg_rect q=w->request;
    if(w->layout==TG_GRID) {
        char sticky[5]; size_t n=0;
        if(w->grid_sticky&2) sticky[n++]='n';
        if(w->grid_sticky&4) sticky[n++]='s';
        if(w->grid_sticky&8) sticky[n++]='w';
        if(w->grid_sticky&16) sticky[n++]='e';
        sticky[n]=0;
        char padx[40],pady[40];
        pad_info(padx,sizeof padx,w->grid_padx,w->grid_padx_extra);
        pad_info(pady,sizeof pady,w->grid_pady,w->grid_pady_extra);
        snprintf(tail,sizeof tail," -column %d -row %d -columnspan %d -rowspan %d -padx %s -pady %s -ipadx %d -ipady %d -sticky %s",q.x,q.y,q.w,q.h,padx,pady,w->grid_ipadx,w->grid_ipady,n?sticky:"{}");
    } else if(w->layout==TG_PLACE) {
        char width[24]="{}",height[24]="{}",rw[32]="{}",rh[32]="{}";
        if(w->place_flags&1) snprintf(width,sizeof width,"%d",q.w);
        if(w->place_flags&2) snprintf(height,sizeof height,"%d",q.h);
        if(w->place_flags&4) snprintf(rw,sizeof rw,"%.9g",w->place_rel[2]);
        if(w->place_flags&8) snprintf(rh,sizeof rh,"%.9g",w->place_rel[3]);
        snprintf(tail,sizeof tail," -x %d -y %d -width %s -height %s -relx %.9g -rely %.9g -relwidth %s -relheight %s -anchor %s -bordermode %s",q.x,q.y,width,height,w->place_rel[0],w->place_rel[1],rw,rh,anchor_name(w->place_anchor,1),w->place_border_mode==1?"outside":w->place_border_mode==2?"ignore":"inside");
    } else {
        const char *side=w->layout==TG_PACK_TOP?"top":w->layout==TG_PACK_BOTTOM?"bottom":w->layout==TG_PACK_LEFT?"left":"right";
        const char *fill=w->pack_fill==1?"none":w->pack_fill==2?"x":w->pack_fill==3?"y":w->pack_fill==4?"both":u->native_toplevels?"none":(w->layout==TG_PACK_TOP||w->layout==TG_PACK_BOTTOM)?"x":"y";
        char padx[40],pady[40];
        pad_info(padx,sizeof padx,w->pack_padx,w->pack_padx_extra);
        pad_info(pady,sizeof pady,w->pack_pady,w->pack_pady_extra);
        snprintf(tail,sizeof tail," -side %s -fill %s -expand %d -padx %s -pady %s -ipadx %d -ipady %d -anchor %s",side,fill,w->pack_expand,padx,pady,w->pack_ipadx,w->pack_ipady,anchor_name(w->pack_anchor,5));
    }
    if(strlen(tail)>=cap-strlen(b)) return TG_ERROR;
    strcat(b,tail); return TG_OK;
}
static int geometry_dispatch(tg_ui *u,size_t n,char **a,char *b,size_t cap) {
    if(n<2) return out(b,cap,"missing geometry target",TG_ERROR);
    if(n==3&&!strcmp(a[1],"info")) {
        const tg_widget *w=tg_ui_get(u,tg_ui_find(u,a[2]));
        return w?geometry_info(u,w,a[0],b,cap):out(b,cap,"unknown widget",TG_ERROR);
    }
    if(!strcmp(a[0],"grid")&&!strcmp(a[1],"anchor")) {
        if(n!=3&&n!=4) return out(b,cap,"wrong # args",TG_ERROR);
        tg_widget *p=tg_lookup(u,tg_ui_find(u,a[2])); if(!p) return out(b,cap,"unknown widget",TG_ERROR);
        if(n==3) return out(b,cap,anchor_name(p->grid_anchor,1),TG_OK);
        int value=anchor(a[3]); if(!value) return out(b,cap,"invalid grid anchor",TG_ERROR);
        p->grid_anchor=value; return out(b,cap,"",TG_OK);
    }
    if(!strcmp(a[0],"grid")&&(!strcmp(a[1],"rowconfigure")||!strcmp(a[1],"columnconfigure")))
        return grid_track(u,n,a,b,cap);
    if(!strcmp(a[1],"configure")||!strcmp(a[1],"config")) {
        if(n<3||n%2==0) return out(b,cap,"wrong # args",TG_ERROR);
        char *args[64];
        if(n>64) return out(b,cap,"geometry argument limit",TG_ERROR);
        args[0]=a[0]; for(size_t k=2;k<n;k++) args[k-1]=a[k];
        return tg_ui_command(u,n-1,args,b,cap);
    }
    if(!strcmp(a[1],"forget")||(!strcmp(a[0],"grid")&&!strcmp(a[1],"remove"))) {
        if(n<3) return out(b,cap,"missing geometry target",TG_ERROR);
        for(size_t k=2;k<n;k++) if(!tg_ui_find(u,a[k])) return out(b,cap,"unknown widget",TG_ERROR);
        for(size_t k=2;k<n;k++) {
            tg_widget *w=tg_lookup(u,tg_ui_find(u,a[k]));
            if(manager_matches(w->layout,a[0])) {
                if(w->layout==TG_GRID) {
                    w->grid_removed=!strcmp(a[1],"remove");
                    if(w->grid_removed) w->grid_saved=w->request;
                    else { w->grid_sticky=0; w->grid_padx=0; w->grid_pady=0; w->grid_padx_extra=w->grid_pady_extra=0; w->grid_ipadx=w->grid_ipady=0; }
                    w->request=w->grid_natural;
                }
                if(manager_matches(w->layout,"pack")) {
                    w->pack_order=0; w->pack_fill=w->pack_expand=0;
                    w->pack_padx=w->pack_pady=w->pack_padx_extra=w->pack_pady_extra=0;
                    w->pack_ipadx=w->pack_ipady=w->pack_anchor=0;
                    /* If packed via -in into a different container, restore the widget's
                       structural master from its path so subsequent place/pack works. */
                    char master_path[TG_WIDGET_PATH_CAP];
                    strcpy(master_path,w->path);
                    char *dot=strrchr(master_path,'.');
                    if(dot) {
                        if(dot==master_path) dot[1]=0; else *dot=0;
                        tg_id master_id=tg_ui_find(u,master_path);
                        if(master_id&&master_id!=w->parent&&master_id!=w->id) {
                            tg_widget *old_p=tg_lookup(u,w->parent),*new_p=tg_lookup(u,master_id);
                            if(old_p&&old_p->child_count) old_p->child_count--;
                            if(new_p) new_p->child_count++;
                            w->parent=master_id;
                        }
                    }
                }
                if(u->native_toplevels&&w->layout==TG_PLACE) { w->request=w->place_natural; w->request.x=w->request.y=0; w->place_flags=0; }
                w->layout=TG_FREE; w->rect=(tg_rect){0,0,0,0};
                if(w->dialog.enabled) {
                    w->dialog.visible=0; w->dialog.dragging=0;
                    if(u->dialog_capture==w->id) u->dialog_capture=0;
                }
            } else if(!strcmp(a[0],"grid")&&!strcmp(a[1],"forget")) w->grid_removed=0;
        }
        return out(b,cap,"",TG_OK);
    }
    if(!strcmp(a[1],"slaves")||(!strcmp(a[0],"place")&&!strcmp(a[1],"slaves"))) {
        if(n!=3||!cap) return out(b,cap,"wrong # args",TG_ERROR);
        tg_id parent=tg_ui_find(u,a[2]);
        if(!parent) return out(b,cap,"unknown widget",TG_ERROR);
        b[0]=0;
        size_t order[TG_WIDGET_LIMIT],count=tg_sibling_order(u,parent,order);
        for(size_t j=0;j<count;j++) {
            const tg_widget *w=&u->items[order[j]];
            if(manager_matches(w->layout,a[0])&&!tg_list_append(b,cap,w->path))
                return out(b,cap,"geometry list too long",TG_ERROR);
        }
        return TG_OK;
    }
    return -1;
}
/* Native docking object is addressed by widget path, not a process-global handle. */
static int dock_number(const char *s,int *v) {
    char *end; errno=0; long n=strtol(s,&end,10);
    if(errno||end==s||*end||n< -1000000L||n>1000000L) return 0;
    *v=(int)n; return 1;
}
static int dock_command(tg_ui *u,size_t n,char **a,char *b,size_t cap) {
    if(strcmp(a[0],"tkgpu::dock")) return -1;
    if(n<3) return out(b,cap,"dock requires widget and operation",TG_ERROR);
    tg_widget *w=tg_lookup(u,tg_ui_find(u,a[1]));
    if(!w) return out(b,cap,"unknown dock widget",TG_ERROR);
    tg_dock *d=&w->dock; int v[14];
    if(!strcmp(a[2],"begin")&&n==7) {
        for(size_t k=0;k<4;k++) if(!dock_number(a[k+3],v+k)) goto bad_dock;
        tg_dock_begin(d,v[0],v[1],v[2],v[3]); return out(b,cap,"",TG_OK);
    }
    if(!strcmp(a[2],"move")&&n==17&&d->active) {
        for(size_t k=0;k<14;k++) if(!dock_number(a[k+3],v+k)) goto bad_dock;
        if(v[2]<0||v[3]<0||v[4]<0||v[5]<0) goto bad_dock;
        tg_dock_move(d,v[0],v[1],v[2],v[3],v[4],v[5],v+6);
        int z=snprintf(b,cap,"%d %d %s",d->x,d->y,tg_dock_name(d->preview));
        return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR;
    }
    if(!strcmp(a[2],"end")&&n==3) return out(b,cap,tg_dock_name(tg_dock_end(d)),TG_OK);
    if(!strcmp(a[2],"reset")&&n==4) {
        for(int k=1;k<=4;k++) if(!strcmp(a[3],tg_dock_name((tg_dock_side)k))) {
            tg_dock_reset(d,(tg_dock_side)k); return out(b,cap,"",TG_OK);
        }
    }
bad_dock:
    return out(b,cap,"invalid docking operation",TG_ERROR);
}

static int widget_cget_value(const tg_ui *u,const tg_widget *w,const char *name,char *b,size_t cap) {
    (void)u;
    if(!name||!*name) return TG_ERROR;
    if(w->kind==TG_COMBOBOX&&!strcmp(name,"-height")) return tg_choice_query(w,name,b,cap);
    if(tg_textvar_editable(w->kind)) {
        static const char *modes[]={"none","focus","focusin","focusout","key","all"};
        if(!strcmp(name,"-validate")) return out(b,cap,modes[w->validate_mode],TG_OK);
        if(!strcmp(name,"-validatecommand")||!strcmp(name,"-vcmd")) return out(b,cap,w->validate_command,TG_OK);
        if(!strcmp(name,"-invalidcommand")||!strcmp(name,"-invcmd")) return out(b,cap,w->invalid_command,TG_OK);
    }
    if(w->kind==TG_TEXT) {
        int history=tg_document_history_query(w->document,name,b,cap);
        if(history>=0) return history;
    }
    int canvas=tg_canvas_query(w,name,b,cap); if(canvas>=0) return canvas;
    if(!strcmp(name,"-opacity")||!strcmp(name,"-backgroundalpha")||!strcmp(name,"-groupopacity")) {
        if(w->kind==TG_MENU||w->kind==TG_TOPLEVEL) return TG_ERROR;
        unsigned alpha=!strcmp(name,"-opacity")?w->opacity:!strcmp(name,"-groupopacity")?w->group_opacity:w->background_alpha;
        int z=snprintf(b,cap,"%.9g",alpha/255.0);
        return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR;
    }
    if(!strcmp(name,"-foreground")||!strcmp(name,"-fg")) {
        uint32_t fg=w->explicit_fg?w->fg:character_size(w->kind)&&(w->kind==TG_ENTRY||w->kind==TG_TEXT||w->kind==TG_LISTBOX)?0x101b2b:0xf0f5ff;
        snprintf(b,cap,"#%06lx",(unsigned long)fg); return TG_OK;
    }
    if(!strcmp(name,"-image")) return out(b,cap,w->image_name,TG_OK);
    if(!strcmp(name,"-show")) return out(b,cap,w->show,TG_OK);
    if(!strcmp(name,"-cursor")) return out(b,cap,w->cursor_name,TG_OK);
    if(!strcmp(name,"-font")) return out(b,cap,w->font,TG_OK);
    if(!strcmp(name,"-justify")) return out(b,cap,w->justify==1?"center":w->justify==2?"right":"left",TG_OK);
    if(!strcmp(name,"-wraplength")) { snprintf(b,cap,"%d",w->wraplength); return TG_OK; }
    if(!strcmp(name,"-insertbackground")) return out(b,cap,"#ffffff",TG_OK);
    if(!strcmp(name,"-selectbackground")) return out(b,cap,"#0078d7",TG_OK);
    if(!strcmp(name,"-selectforeground")) return out(b,cap,"#ffffff",TG_OK);
    if(w->kind==TG_TEXT&&!strcmp(name,"-wrap")) return out(b,cap,w->wrap==0?"none":w->wrap==2?"word":"char",TG_OK);
    if(!strcmp(name,"-borderwidth")||!strcmp(name,"-bd")) { snprintf(b,cap,"%d",w->border_width); return TG_OK; }
    if(!strcmp(name,"-relief")) {
        static const char *names[]={"flat","raised","sunken","groove","ridge","solid"};
        return out(b,cap,w->relief>=0&&w->relief<6?names[w->relief]:"flat",TG_OK);
    }
    if(!strcmp(name,"-width")||!strcmp(name,"-height")) {
        int width=!strcmp(name,"-width");
        snprintf(b,cap,"%d",width?w->option_width:w->option_height); return TG_OK;
    }
    if(w->themed&&!strcmp(name,"-style")) return out(b,cap,w->style,TG_OK);
    int container_query=tg_container_query(w,name,b,cap);
    if(container_query>=0) return container_query;
    int toggle_query=tg_toggle_query(w,name,b,cap);
    if(toggle_query>=0) return toggle_query;
    int choice_query=tg_choice_query(w,name,b,cap);
    if(choice_query>=0) return choice_query;
    int extra_query=tg_extra_query(w,name,b,cap);
    if(extra_query>=0) return extra_query;
    int indicator_query=tg_indicator_query(w,name,b,cap);
    if(indicator_query>=0) return indicator_query;
    int query=tg_control_query(w,name,b,cap);
    if(query>=0) return query;
    if(!strcmp(name,"-text")) return out(b,cap,w->text,TG_OK);
    if(!strcmp(name,"-background")||!strcmp(name,"-bg")) {
        if(w->color_alpha==255) snprintf(b,cap,"#%06lx",(unsigned long)w->bg);
        else snprintf(b,cap,"#%06lx%02x",(unsigned long)w->bg,(unsigned)w->color_alpha);
        return TG_OK;
    }
    if(!strcmp(name,"-highlightbackground")) { snprintf(b,cap,"#%06lx",(unsigned long)(w->has_highlight_background?w->highlight_background:w->bg)); return TG_OK; }
    if(!strcmp(name,"-highlightcolor")) { snprintf(b,cap,"#%06lx",(unsigned long)(w->has_highlight_color?w->highlight_color:0x0078d7)); return TG_OK; }
    if(!strcmp(name,"-highlightthickness")) { snprintf(b,cap,"%d",w->highlight_width); return TG_OK; }
    if(!strcmp(name,"-takefocus")) return out(b,cap,"1",TG_OK);
    if(!strcmp(name,"-padx")) { snprintf(b,cap,"%d",w->text_padx); return TG_OK; }
    if(!strcmp(name,"-pady")) { snprintf(b,cap,"%d",w->text_pady); return TG_OK; }
    if(!strcmp(name,"-padding")) { snprintf(b,cap,"%d %d",w->text_padx,w->text_pady); return TG_OK; }
    if(!strcmp(name,"-anchor")) return out(b,cap,"center",TG_OK);
    if(!strcmp(name,"-activestyle")) return out(b,cap,"none",TG_OK);
    if(!strcmp(name,"-selectmode")) return out(b,cap,"browse",TG_OK);
    if(!strcmp(name,"-selectcolor")) return out(b,cap,"#ffffff",TG_OK);
    if(!strcmp(name,"-indicatoron")) return out(b,cap,"1",TG_OK);
    if(!strcmp(name,"-activebackground")) {
        snprintf(b,cap,"#%06lx",(unsigned long)(w->has_active_bg?w->active_bg:w->bg));
        return TG_OK;
    }
    if(!strcmp(name,"-activeforeground")) {
        snprintf(b,cap,"#%06lx",(unsigned long)(w->has_active_fg?w->active_fg:w->fg));
        return TG_OK;
    }
    if(!strcmp(name,"-listvariable")||!strcmp(name,"-yscrollcommand")||!strcmp(name,"-xscrollcommand")) return out(b,cap,"",TG_OK);
    return -1;
}

static int tg_widget_supports_option(const tg_widget *w,const char *opt) {
    if(!strcmp(opt,"-background")||!strcmp(opt,"-bg")) return 1;
    if(!strcmp(opt,"-borderwidth")||!strcmp(opt,"-bd")) return 1;
    if(!strcmp(opt,"-relief")||!strcmp(opt,"-cursor")) return 1;
    if(!strcmp(opt,"-width")||!strcmp(opt,"-height")) return 1;
    if(!strcmp(opt,"-takefocus")||!strcmp(opt,"-highlightthickness")||!strcmp(opt,"-highlightbackground")||!strcmp(opt,"-highlightcolor")) return 1;
    if(!strcmp(opt,"-foreground")||!strcmp(opt,"-fg")||!strcmp(opt,"-font")) {
        return character_size(w->kind)||w->kind==TG_MESSAGE||w->kind==TG_LABEL||w->kind==TG_BUTTON;
    }
    if(!strcmp(opt,"-text")) {
        return w->kind==TG_LABEL||w->kind==TG_BUTTON||w->kind==TG_LABELFRAME||w->kind==TG_MENUBUTTON||w->kind==TG_MESSAGE||tg_toggle_kind(w->kind);
    }
    if(!strcmp(opt,"-validate")||!strcmp(opt,"-validatecommand")||!strcmp(opt,"-invalidcommand")) return tg_textvar_editable(w->kind);
    if(!strcmp(opt,"-textvariable")) return tg_textvar_kind(w->kind);
    if(!strcmp(opt,"-postcommand")) return w->kind==TG_COMBOBOX;
    if(!strcmp(opt,"-format")||!strcmp(opt,"-from")||!strcmp(opt,"-to")||!strcmp(opt,"-increment")) return w->kind==TG_SPINBOX;
    if(!strcmp(opt,"-values")) return tg_choice_kind(w->kind);
    if(!strcmp(opt,"-command")) {
        return w->kind==TG_BUTTON||w->kind==TG_SCROLLBAR||w->kind==TG_SPINBOX||tg_toggle_kind(w->kind);
    }
    if(!strcmp(opt,"-state")) {
        return w->kind==TG_BUTTON||w->kind==TG_SCROLLBAR||w->kind==TG_MENUBUTTON||w->kind==TG_LISTBOX||w->kind==TG_TEXT||tg_toggle_kind(w->kind)||w->kind==TG_ENTRY||tg_choice_kind(w->kind);
    }
    if(!strcmp(opt,"-image")) {
        return w->kind==TG_LABEL||w->kind==TG_BUTTON||tg_toggle_kind(w->kind)||w->kind==TG_MENUBUTTON;
    }
    if(!strcmp(opt,"-show")) return w->kind==TG_ENTRY;
    if(!strcmp(opt,"-insertbackground")||!strcmp(opt,"-selectbackground")||!strcmp(opt,"-selectforeground")) {
        return w->kind==TG_ENTRY||w->kind==TG_TEXT;
    }
    if(!strcmp(opt,"-wrap")||!strcmp(opt,"-undo")||!strcmp(opt,"-autoseparators")||!strcmp(opt,"-maxundo")) return w->kind==TG_TEXT;
    if(!strcmp(opt,"-justify")) return character_size(w->kind)||w->kind==TG_MESSAGE;
    if(!strcmp(opt,"-wraplength")) return w->kind==TG_LABEL||w->kind==TG_MESSAGE;
    if(!strcmp(opt,"-variable")||!strcmp(opt,"-value")||!strcmp(opt,"-onvalue")||!strcmp(opt,"-offvalue")) {
        return tg_toggle_kind(w->kind);
    }
    if(!strcmp(opt,"-selectcolor")||!strcmp(opt,"-indicatoron")) return tg_toggle_kind(w->kind);
    if(!strcmp(opt,"-activebackground")||!strcmp(opt,"-activeforeground")) return w->kind==TG_BUTTON||tg_toggle_kind(w->kind)||w->kind==TG_MENUBUTTON;
    if(!strcmp(opt,"-style")) return w->themed;
    if(!strcmp(opt,"-padx")||!strcmp(opt,"-pady")||!strcmp(opt,"-padding")||!strcmp(opt,"-anchor")) return character_size(w->kind)||w->kind==TG_FRAME||w->kind==TG_LABELFRAME||w->kind==TG_NOTEBOOK;
    if(!strcmp(opt,"-labelanchor")) return w->kind==TG_LABELFRAME;
    return 0;
}

static void tg_format_tcl_val(const char *in,char *out_val,size_t out_cap) {
    if(!in||!*in) { snprintf(out_val,out_cap,"{}"); return; }
    if(strchr(in,' ')||strchr(in,'\t')||strchr(in,'\n')) snprintf(out_val,out_cap,"{%s}",in);
    else snprintf(out_val,out_cap,"%s",in);
}

static int tg_append_option_spec(const tg_ui *u,const tg_widget *w,const char *opt_name,char *b,size_t cap,size_t *used,int bracketed) {
    if(!strcmp(opt_name,"-bg")) {
        const char *syn=bracketed?"{-bg -background} ":"-bg -background";
        size_t len=strlen(syn);
        if(*used+len>=cap) return TG_ERROR;
        strcpy(b+*used,syn); *used+=len; return TG_OK;
    }
    if(!strcmp(opt_name,"-fg")) {
        const char *syn=bracketed?"{-fg -foreground} ":"-fg -foreground";
        size_t len=strlen(syn);
        if(*used+len>=cap) return TG_ERROR;
        strcpy(b+*used,syn); *used+=len; return TG_OK;
    }
    if(!strcmp(opt_name,"-bd")) {
        const char *syn=bracketed?"{-bd -borderwidth} ":"-bd -borderwidth";
        size_t len=strlen(syn);
        if(*used+len>=cap) return TG_ERROR;
        strcpy(b+*used,syn); *used+=len; return TG_OK;
    }
    char val[256]="";
    int rc=widget_cget_value(u,w,opt_name,val,sizeof val);
    if(rc!=TG_OK) val[0]=0;
    const char *dbname=opt_name[0]=='-'?opt_name+1:opt_name;
    char dbclass[64];
    strncpy(dbclass,dbname,sizeof(dbclass)-1); dbclass[sizeof(dbclass)-1]=0;
    if(dbclass[0]>='a'&&dbclass[0]<='z') dbclass[0]-=32;
    char fmt[280];
    tg_format_tcl_val(val,fmt,sizeof fmt);
    char item[512];
    if(bracketed) snprintf(item,sizeof item,"{%s %s %s %s %s} ",opt_name,dbname,dbclass,fmt,fmt);
    else snprintf(item,sizeof item,"%s %s %s %s %s",opt_name,dbname,dbclass,fmt,fmt);
    size_t len=strlen(item);
    if(*used+len>=cap) return TG_ERROR;
    strcpy(b+*used,item); *used+=len; return TG_OK;
}

/* Returns -1 for commands outside this subsystem. */
int tg_ui_command(tg_ui *u,size_t n,char **a,char *b,size_t cap) {
    static const char *names[]={"","frame","label","button","canvas","entry","toplevel","progressbar","separator","scale","scrollbar","labelframe","combobox","spinbox","notebook","treeview","checkbutton","radiobutton","panedwindow","menubutton","sizegrip","menu","listbox","text","message"};
    if(!n) return -1;
    if(!strcmp(a[0],"ttk::notebook::enableTraversal")) {
        tg_widget *w=n==2?tg_lookup(u,tg_ui_find(u,a[1])):NULL;
        if(!w||w->kind!=TG_NOTEBOOK) return out(b,cap,"invalid notebook traversal target",TG_ERROR);
        w->notebook_traversal=1; return out(b,cap,"",TG_OK);
    }
    if(!strcmp(a[0],"raise")&&n==2) {
        tg_widget *w=tg_lookup(u,tg_ui_find(u,a[1]));
        if(!w) return out(b,cap,"unknown raise target",TG_ERROR);
        uint64_t max=0;
        for(size_t k=0;k<u->count;k++) if(u->items[k].parent==w->parent&&u->items[k].z_order>max) max=u->items[k].z_order;
        if(max==UINT64_MAX) return out(b,cap,"raise order exhausted",TG_ERROR);
        w->z_order=max+1; return out(b,cap,"",TG_OK);
    }
    int dialog_rc=tg_base_dialog_command(u,n,a,b,cap); if(dialog_rc>=0) return dialog_rc;
    int dock_rc=dock_command(u,n,a,b,cap); if(dock_rc>=0) return dock_rc;
    int option_rc=option_command(u,n,a,b,cap); if(option_rc>=0) return option_rc;
    int image_rc=tg_image_command(u,n,a,b,cap);
    if(image_rc>=0) return image_rc;
    int style_rc=tg_style_command(u,n,a,b,cap);
    if(style_rc>=0) return style_rc;
    const char *name=a[0];
    if(!strncmp(name,"ttk::",5)&&(!strcmp(name+5,"frame")||!strcmp(name+5,"label")||!strcmp(name+5,"button")||!strcmp(name+5,"entry")||!strcmp(name+5,"progressbar")||!strcmp(name+5,"separator")||!strcmp(name+5,"scale")||!strcmp(name+5,"scrollbar")||!strcmp(name+5,"labelframe")||!strcmp(name+5,"combobox")||!strcmp(name+5,"spinbox")||!strcmp(name+5,"notebook")||!strcmp(name+5,"treeview")||!strcmp(name+5,"checkbutton")||!strcmp(name+5,"radiobutton")||!strcmp(name+5,"panedwindow")||!strcmp(name+5,"menubutton")||!strcmp(name+5,"sizegrip"))) name+=5;
    for(int k=TG_FRAME;k<=TG_MESSAGE;k++) if(!strcmp(name,names[k])) {
        if(n<2||n%2||(k>TG_TOPLEVEL&&k!=TG_MENU&&k!=TG_LISTBOX&&k!=TG_TEXT&&k!=TG_MESSAGE&&k!=TG_SCALE&&k!=TG_SCROLLBAR&&k!=TG_LABELFRAME&&k!=TG_SPINBOX&&k!=TG_CHECKBUTTON&&k!=TG_RADIOBUTTON&&k!=TG_PANEDWINDOW&&k!=TG_MENUBUTTON&&name==a[0])) return out(b,cap,"unsupported widget options",TG_ERROR);
        tg_widget opt={0}; opt.takefocus=-1; opt.opacity=opt.background_alpha=opt.group_opacity=opt.color_alpha=255; opt.kind=(tg_kind)k; opt.themed=name!=a[0]; tg_indicator_defaults(&opt); tg_extra_defaults(&opt); tg_choice_defaults(&opt); tg_toggle_defaults(&opt); tg_container_defaults(&opt);
        if(!opt.themed&&(k==TG_SCALE||k==TG_PANEDWINDOW)) opt.vertical=k==TG_SCALE;
        if(k==TG_TEXT) opt.wrap=1;
        uint32_t bg=0; int has_bg=0;
        if(!opt.themed) {
            const char *klass=resource_class_name(opt.kind,0);
            const char *value=option_lookup(u,a[1],klass,"background","Background");
            if(value&&background_color(&opt,value,&bg)) has_bg=1;
            value=option_lookup(u,a[1],klass,"foreground","Foreground");
            if(value&&color(value,&opt.fg)) opt.explicit_fg=1;
        }
        for(size_t j=2;j<n;j+=2) {
            if(!strcmp(a[j],"-text")&&(k==TG_BUTTON||k==TG_LABEL||k==TG_LABELFRAME||k==TG_MENUBUTTON||k==TG_MESSAGE||tg_toggle_kind((tg_kind)k))) {
                if(strlen(a[j+1])>=sizeof opt.text) goto bad;
                strcpy(opt.text,a[j+1]);
            } else if(!strcmp(a[j],"-background")||!strcmp(a[j],"-bg")) {
                if(!background_color(&opt,a[j+1],&bg)) goto bad;
                has_bg=1;
            } else if(k==TG_MENU&&!strcmp(a[j],"-tearoff")&&!strcmp(a[j+1],"0")) {
                continue;
            } else if(k==TG_TREEVIEW&&(!strcmp(a[j],"-columns")||!strcmp(a[j],"-show")||!strcmp(a[j],"-displaycolumns")||!strcmp(a[j],"-selectmode"))) {
                continue;
            } else if(common_option(u,&opt,a[j],a[j+1])!=TG_OK&&tg_container_option(&opt,a[j],a[j+1])!=TG_OK&&tg_toggle_option(&opt,a[j],a[j+1])!=TG_OK&&tg_choice_option(&opt,a[j],a[j+1])!=TG_OK&&tg_extra_option(&opt,a[j],a[j+1])!=TG_OK&&tg_indicator_option(&opt,a[j],a[j+1])!=TG_OK&&tg_style_option(&opt,a[j],a[j+1])!=TG_OK&&tg_control_option(&opt,a[j],a[j+1])!=TG_OK) {
                snprintf(b,cap,"unsupported option %.120s for %.80s %.511s",a[j],a[0],a[1]);
                return TG_ERROR;
            }
        }
        const char *path=a[1];
        if(path[0]!='.'||!path[1]||path[strlen(path)-1]=='.')
            return out(b,cap,"invalid widget path",TG_ERROR);
        if(strlen(path)>=TG_WIDGET_PATH_CAP)
            return out(b,cap,"widget path exceeds 511 bytes",TG_ERROR);
        if(tg_ui_find(u,path))
            return out(b,cap,"widget path already exists",TG_ERROR);
        if(u->count>=TG_WIDGET_LIMIT)
            return out(b,cap,"widget capacity exhausted (2048 live widgets)",TG_ERROR);
        char parent_path[TG_WIDGET_PATH_CAP];
        strcpy(parent_path,path);
        char *dot=strrchr(parent_path,'.');
        if(dot==parent_path) dot[1]=0; else *dot=0;
        if(!tg_ui_find(u,parent_path))
            return out(b,cap,"widget parent does not exist",TG_ERROR);
        if(opt.image_name[0]&&!tg_image_find(u,opt.image_name))
            return out(b,cap,"unknown image",TG_ERROR);
        tg_id id=tg_ui_add(u,path,(tg_kind)k,opt.text);
        if(!id) return out(b,cap,"widget allocation failed",TG_ERROR);
        tg_widget *w=tg_lookup(u,id);
        w->state=opt.state; w->themed=opt.themed; memcpy(w->style,opt.style,sizeof w->style); memcpy(w->command,opt.command,sizeof w->command); strcpy(w->textvariable,opt.textvariable);
        w->vertical=opt.vertical; w->indeterminate=opt.indeterminate; w->length=opt.length; w->value=opt.value; w->maximum=opt.maximum;
        memcpy(w->value_text,opt.value_text,sizeof w->value_text); memcpy(w->maximum_text,opt.maximum_text,sizeof w->maximum_text);
        w->minimum=opt.minimum; w->first=opt.first; w->last=opt.last; memcpy(w->minimum_text,opt.minimum_text,sizeof w->minimum_text);
        w->resolution=opt.resolution; w->has_resolution=opt.has_resolution;
        w->trough_color=opt.trough_color; w->has_trough_color=opt.has_trough_color;
        w->slider_length=opt.slider_length; w->show_value=opt.show_value;
        memcpy(w->choices,opt.choices,sizeof w->choices); memcpy(w->increment_text,opt.increment_text,sizeof w->increment_text); w->increment=opt.increment; w->wrap=opt.wrap;
        w->choice_height=opt.choice_height; strcpy(w->choice_postcommand,opt.choice_postcommand); strcpy(w->choice_format,opt.choice_format);
        memcpy(w->sashes,opt.sashes,sizeof w->sashes); strcpy(w->menu_path,opt.menu_path);
        if(tg_toggle_kind(w->kind)||w->kind==TG_SCALE||w->kind==TG_PROGRESS) {
            if(opt.explicit_variable||w->kind==TG_RADIOBUTTON) strcpy(w->variable,opt.variable);
            strcpy(w->on_value,opt.on_value); strcpy(w->off_value,opt.off_value); w->explicit_variable=opt.explicit_variable;
            if(!w->variable[0]&&w->kind==TG_CHECKBUTTON) w->alternate=0;
        }
        if(k==TG_SIZEGRIP) w->request=(tg_rect){0,0,16,16};
        if(k==TG_PROGRESS) w->request=(tg_rect){0,0,w->vertical?16:w->length,w->vertical?w->length:16};
        if(k==TG_SEPARATOR) w->request=(tg_rect){0,0,w->vertical?2:100,w->vertical?100:2};
        if(k==TG_SCALE) w->request=(tg_rect){0,0,w->vertical?16:w->length,w->vertical?w->length:16};
        if(k==TG_SCROLLBAR) w->request=(tg_rect){0,0,w->vertical?16:100,w->vertical?100:16};
        if(w->tree) for(size_t j=2;j<n;j+=2) {
            int rc=tg_tree_option(w->tree,a[j],a[j+1]);
            if(rc==TG_ERROR) { tg_ui_remove(u,id); return out(b,cap,"invalid tree option",TG_ERROR); }
        }
        if(has_bg) w->bg=bg;
        common_apply(u,w,&opt);
        if(w->document) for(size_t j=2;j<n;j+=2)
            (void)tg_document_history_option(w->document,a[j],a[j+1]);
        if(w->image_name[0]) { const tg_image_slot *image=tg_image_find(u,w->image_name); if(!image) return out(b,cap,"unknown image",TG_ERROR); tg_rect *size=w->layout==TG_GRID?&w->grid_natural:&w->request; int iw=(w->has_width&&w->option_width>0?w->option_width:tg_photo_width(image->photo))+2*(w->text_padx+w->border_width+w->highlight_width); int ih=(w->has_height&&w->option_height>0?w->option_height:tg_photo_height(image->photo))+2*(w->text_pady+w->border_width+w->highlight_width); if(u->native_toplevels&&w->layout==TG_PLACE) { w->place_natural.w=iw; w->place_natural.h=ih; } if(w->layout!=TG_PLACE||!(w->place_flags&1)) size->w=iw; if(w->layout!=TG_PLACE||!(w->place_flags&2)) size->h=ih; }
        return out(b,cap,a[1],TG_OK);
    }
    if(!strcmp(a[0],"focus")) {
        if(n==1) {
            const tg_widget *w=tg_ui_get(u,tg_ui_focused(u));
            return out(b,cap,w?w->path:"",TG_OK);
        }
        if(n==2&&tg_ui_find(u,a[1])&&tg_ui_focus(u,tg_ui_find(u,a[1]))==TG_OK) return out(b,cap,"",TG_OK);
        return out(b,cap,"invalid focus target",TG_ERROR);
    }
    if(a[0][0]=='.') {
        tg_id id=tg_ui_find(u,a[0]); tg_widget *w=tg_lookup(u,id);
        if(!w||n<2) return out(b,cap,"unknown widget or missing subcommand",TG_ERROR);
        if(w->kind==TG_CANVAS) {
            int rc=tg_canvas_item_command(u,id,n,a,b,cap);
            if(rc>=0) return rc;
        }
        int identify=tg_style_identify(u,w,n,a,b,cap);
        if(identify>=0) return identify==TG_OK?identify:out(b,cap,"invalid layout identify coordinates",TG_ERROR);
        int ttk_state=tg_ttk_state_command(u,w,n,a,b,cap);
        if(ttk_state>=0) return ttk_state==TG_OK?ttk_state:out(b,cap,"invalid ttk state specification",TG_ERROR);
        int container=tg_container_command(u,w,n,a,b,cap);
        if(container>=0) return container==TG_OK?container:out(b,cap,"invalid container command",TG_ERROR);
        int document=tg_document_command(w,n,a,b,cap);
        if(document>=0) return document==TG_OK?document:out(b,cap,"invalid text command",TG_ERROR);
        int listbox=tg_listbox_command(w,n,a,b,cap);
        if(listbox>=0) return listbox==TG_OK?listbox:out(b,cap,"invalid listbox command",TG_ERROR);
        int tree=tg_tree_command(u,w,n,a,b,cap);
        if(tree>=0) return tree==TG_OK?tree:out(b,cap,"invalid tree command",TG_ERROR);
        int notebook=tg_notebook_command(u,w,n,a,b,cap);
        if(notebook>=0) return notebook==TG_OK?notebook:out(b,cap,"invalid notebook command",TG_ERROR);
        int choice=tg_choice_command(w,n,a,b,cap);
        if(choice>=0) return choice==TG_OK?choice:out(b,cap,"invalid choice command",TG_ERROR);
        int extra=tg_extra_command(w,n,a,b,cap);
        if(extra>=0) return extra==TG_OK?extra:out(b,cap,"invalid extra widget command",TG_ERROR);
        int indicator=tg_indicator_command(w,n,a,b,cap);
        if(indicator>=0) return indicator==TG_OK?indicator:out(b,cap,"invalid indicator command",TG_ERROR);
        int edit=tg_entry_command(w,n,a,b,cap);
        if(edit>=0) return edit==TG_OK?edit:out(b,cap,"invalid entry edit",TG_ERROR);
        if(!strcmp(a[1],"config")) {
            char *args[64]; if(n>64) goto bad;
            for(size_t k=0;k<n;k++) args[k]=a[k];
            args[1]="configure"; return tg_ui_command(u,n,args,b,cap);
        }
        if(!strcmp(a[1],"cget")&&n==3) {
            int rc=widget_cget_value(u,w,a[2],b,cap);
            return rc>=0?rc:out(b,cap,"unknown option",TG_ERROR);
        } else if(!strcmp(a[1],"configure")) {
            if(n==2) {
                static const char *const s_spec_options[]={
                    "-background","-bd","-bg","-borderwidth","-cursor","-font",
                    "-foreground","-fg","-height","-highlightbackground","-highlightcolor",
                    "-highlightthickness","-image","-insertbackground","-justify",
                    "-relief","-selectbackground","-selectforeground","-show","-state",
                    "-style","-takefocus","-text","-textvariable","-width","-wrap",
                    "-wraplength","-command","-variable","-value","-onvalue","-offvalue",
                    "-padx","-pady","-anchor","-undo","-autoseparators","-maxundo","-validate","-validatecommand","-invalidcommand","-postcommand","-format","-from","-to","-increment","-values"
                };
                b[0]=0; size_t used=0;
                for(size_t oi=0;oi<sizeof(s_spec_options)/sizeof(s_spec_options[0]);oi++) {
                    const char *opt=s_spec_options[oi];
                    if(!strcmp(opt,"-bg")||!strcmp(opt,"-fg")||!strcmp(opt,"-bd")) {
                        tg_append_option_spec(u,w,opt,b,cap,&used,1);
                        continue;
                    }
                    if(tg_widget_supports_option(w,opt)) {
                        tg_append_option_spec(u,w,opt,b,cap,&used,1);
                    }
                }
                if(used>0&&b[used-1]==' ') b[--used]=0;
                return TG_OK;
            } else if(n==3) {
                b[0]=0; size_t used=0;
                const char *opt=a[2];
                char full_opt[64];
                if(opt[0]!='-') { snprintf(full_opt,sizeof full_opt,"-%s",opt); opt=full_opt; }
                return tg_append_option_spec(u,w,opt,b,cap,&used,0);
            } else if(n>=4&&n%2==0) {
            tg_widget opt=*w;
            const char *text=NULL; uint32_t bg=0; int has_bg=0;
            for(size_t k=2;k<n;k+=2) {
                if(!strcmp(a[k],"-text")&&(w->kind==TG_LABEL||w->kind==TG_BUTTON||w->kind==TG_LABELFRAME||w->kind==TG_MENUBUTTON||w->kind==TG_MESSAGE||tg_toggle_kind(w->kind))) text=a[k+1];
                else if(!strcmp(a[k],"-background")||!strcmp(a[k],"-bg")) {
                    if(!background_color(&opt,a[k+1],&bg)) return out(b,cap,"expected background color",TG_ERROR);
                    has_bg=1;
                } else if(w->tree&&tg_tree_option(w->tree,a[k],a[k+1])==TG_OK) {
                    continue;
                } else if(common_option(u,&opt,a[k],a[k+1])!=TG_OK&&tg_container_option(&opt,a[k],a[k+1])!=TG_OK&&tg_toggle_option(&opt,a[k],a[k+1])!=TG_OK&&tg_choice_option(&opt,a[k],a[k+1])!=TG_OK&&tg_extra_option(&opt,a[k],a[k+1])!=TG_OK&&tg_indicator_option(&opt,a[k],a[k+1])!=TG_OK&&tg_style_option(&opt,a[k],a[k+1])!=TG_OK&&tg_control_option(&opt,a[k],a[k+1])!=TG_OK) {
                    snprintf(b,cap,"unsupported configure option %.120s for %.511s",a[k],a[0]);
                    return TG_ERROR;
                }
            }
            if(tg_ui_configure(u,id,text,has_bg?&bg:NULL)) return out(b,cap,"invalid widget configuration",TG_ERROR);
            w->state=opt.state; w->themed=opt.themed; memcpy(w->style,opt.style,sizeof w->style); memcpy(w->command,opt.command,sizeof w->command); strcpy(w->textvariable,opt.textvariable);
            w->vertical=opt.vertical; w->indeterminate=opt.indeterminate; w->length=opt.length; w->value=opt.value; w->maximum=opt.maximum;
            memcpy(w->value_text,opt.value_text,sizeof w->value_text); memcpy(w->maximum_text,opt.maximum_text,sizeof w->maximum_text);
        w->minimum=opt.minimum; w->first=opt.first; w->last=opt.last; memcpy(w->minimum_text,opt.minimum_text,sizeof w->minimum_text);
        w->resolution=opt.resolution; w->has_resolution=opt.has_resolution;
        w->trough_color=opt.trough_color; w->has_trough_color=opt.has_trough_color;
        w->slider_length=opt.slider_length; w->show_value=opt.show_value;
        memcpy(w->choices,opt.choices,sizeof w->choices); memcpy(w->increment_text,opt.increment_text,sizeof w->increment_text); w->increment=opt.increment; w->wrap=opt.wrap;
        w->choice_height=opt.choice_height; strcpy(w->choice_postcommand,opt.choice_postcommand); strcpy(w->choice_format,opt.choice_format);
        memcpy(w->sashes,opt.sashes,sizeof w->sashes); strcpy(w->menu_path,opt.menu_path);
        if(tg_toggle_kind(w->kind)||w->kind==TG_SCALE||w->kind==TG_PROGRESS) {
            if(opt.explicit_variable||w->kind==TG_RADIOBUTTON) strcpy(w->variable,opt.variable);
            strcpy(w->on_value,opt.on_value); strcpy(w->off_value,opt.off_value); w->explicit_variable=opt.explicit_variable;
            if(!w->variable[0]&&w->kind==TG_CHECKBUTTON) w->alternate=0;
        }
            common_apply(u,w,&opt);
            if(w->document) for(size_t j=2;j<n;j+=2)
                (void)tg_document_history_option(w->document,a[j],a[j+1]);
        if(w->image_name[0]) { const tg_image_slot *image=tg_image_find(u,w->image_name); if(!image) return out(b,cap,"unknown image",TG_ERROR); tg_rect *size=w->layout==TG_GRID?&w->grid_natural:&w->request; int iw=(w->has_width&&w->option_width>0?w->option_width:tg_photo_width(image->photo))+2*(w->text_padx+w->border_width+w->highlight_width); int ih=(w->has_height&&w->option_height>0?w->option_height:tg_photo_height(image->photo))+2*(w->text_pady+w->border_width+w->highlight_width); if(u->native_toplevels&&w->layout==TG_PLACE) { w->place_natural.w=iw; w->place_natural.h=ih; } if(w->layout!=TG_PLACE||!(w->place_flags&1)) size->w=iw; if(w->layout!=TG_PLACE||!(w->place_flags&2)) size->h=ih; }
            return out(b,cap,"",TG_OK);
            }
        }
        return out(b,cap,"unsupported widget subcommand",TG_ERROR);
    }
    if((!strcmp(a[0],"grid")||!strcmp(a[0],"pack"))&&n>=3&&!strcmp(a[1],"propagate")) {
        tg_widget *w=tg_lookup(u,tg_ui_find(u,a[2])); if(!w) return out(b,cap,"unknown propagation target",TG_ERROR);
        int *off=!strcmp(a[0],"grid")?&w->no_grid_propagate:&w->no_pack_propagate;
        if(n==3) return out(b,cap,*off?"0":"1",TG_OK);
        if(n!=4) return out(b,cap,"invalid propagation arguments",TG_ERROR);
        if(!strcmp(a[3],"0")||!strcmp(a[3],"false")) *off=1;
        else if(!strcmp(a[3],"1")||!strcmp(a[3],"true")) *off=0;
        else return out(b,cap,"invalid propagation boolean",TG_ERROR);
        return out(b,cap,"",TG_OK);
    }
    if(!strcmp(a[0],"grid")||!strcmp(a[0],"pack")||!strcmp(a[0],"place")) {
        int rc=geometry_dispatch(u,n,a,b,cap);
        if(rc>=0) return rc;
    }
    if(!strcmp(a[0],"grid")) {
        if(n<2||n%2) return out(b,cap,"wrong # args",TG_ERROR);
        tg_id id=tg_ui_find(u,a[1]); const tg_widget *w=tg_ui_get(u,id);
        if(!w) return out(b,cap,"unknown widget",TG_ERROR);
        tg_rect q=w->layout==TG_GRID?w->request:w->grid_removed?w->grid_saved:(tg_rect){0,0,1,1};
        int sticky=w->grid_sticky,padx=w->grid_padx,pady=w->grid_pady;
        int padx_extra=w->grid_padx_extra,pady_extra=w->grid_pady_extra;
        int ipadx=w->grid_ipadx,ipady=w->grid_ipady;
        if(w->layout!=TG_GRID&&!w->grid_removed) for(size_t j=1;j<u->count;j++) { const tg_widget *s=&u->items[j]; if(s->id!=id&&s->parent==w->parent&&s->layout==TG_GRID) { int end=s->request.y+s->request.h; if(end>q.y) q.y=end; } }
        tg_rect natural=w->layout==TG_GRID?w->grid_natural:w->request;
        for(size_t k=2;k<n;k+=2) {
            int *field=NULL;
            if(!strcmp(a[k],"-column")) field=&q.x;
            else if(!strcmp(a[k],"-row")) field=&q.y;
            else if(!strcmp(a[k],"-columnspan")) field=&q.w;
            else if(!strcmp(a[k],"-rowspan")) field=&q.h;
            else if(!strcmp(a[k],"-padx")) { if(!pad_value(a[k+1],&padx,&padx_extra)) goto bad; continue; }
            else if(!strcmp(a[k],"-pady")) { if(!pad_value(a[k+1],&pady,&pady_extra)) goto bad; continue; }
            else if(!strcmp(a[k],"-ipadx")) field=&ipadx;
            else if(!strcmp(a[k],"-ipady")) field=&ipady;
            else if(!strcmp(a[k],"-sticky")) {
                sticky=1;
                for(const char *p=a[k+1];*p;p++) {
                    if(*p=='n') sticky|=2;
                    else if(*p=='s') sticky|=4;
                    else if(*p=='w') sticky|=8;
                    else if(*p=='e') sticky|=16;
                    else if(*p!=' '&&*p!=',') goto bad;
                }
                continue;
            } else return out(b,cap,"unsupported grid option",TG_ERROR);
            if(!dim(a[k+1],field)) goto bad;
        }
        if(tg_ui_geometry(u,id,TG_GRID,q)) goto bad;
        tg_widget *target=tg_lookup(u,id);
        target->grid_natural=natural; target->grid_removed=0;
        target->grid_sticky=sticky; target->grid_padx=padx; target->grid_pady=pady; target->grid_padx_extra=padx_extra; target->grid_pady_extra=pady_extra; target->grid_ipadx=ipadx; target->grid_ipady=ipady;
        return out(b,cap,"",TG_OK);
    }
    if(!strcmp(a[0],"destroy")) {
        if(n<2) return out(b,cap,"wrong # args",TG_ERROR);
        for(size_t k=1;k<n;k++) {
            tg_id id=tg_ui_find(u,a[k]);
            if(id&&id!=1&&tg_lookup(u,id)) tg_ui_remove(u,id);
        }
        return out(b,cap,"",TG_OK);
    }
    if(!strcmp(a[0],"pack")||!strcmp(a[0],"place")) {
        if(n<2||n%2) return out(b,cap,"wrong # args",TG_ERROR);
        tg_id id=tg_ui_find(u,a[1]); const tg_widget *w=tg_ui_get(u,id);
        if(!w) return out(b,cap,"unknown widget",TG_ERROR);
        int ispack=!strcmp(a[0],"pack");
        int already_packed=manager_matches(w->layout,"pack");
        tg_layout layout=ispack?TG_PACK_TOP:TG_PLACE;
        if(ispack&&(w->layout==TG_PACK_TOP||w->layout==TG_PACK_LEFT||w->layout==TG_PACK_BOTTOM||w->layout==TG_PACK_RIGHT)) layout=w->layout;
        int fill=w->pack_fill,expand=w->pack_expand,padx=w->pack_padx,pady=w->pack_pady;
        int padx_extra=w->pack_padx_extra,pady_extra=w->pack_pady_extra;
        int ipadx=w->pack_ipadx,ipady=w->pack_ipady;
        tg_id relative=0; int after=0;
        tg_rect q=w->layout==TG_GRID?w->grid_natural:w->request;
        int align=ispack?w->pack_anchor:w->place_anchor;
        int border_mode=w->layout==TG_PLACE?w->place_border_mode:0;
        unsigned flags=w->layout==TG_PLACE?w->place_flags:0;
        double rel[4]={0};
        if(w->layout==TG_PLACE) memcpy(rel,w->place_rel,sizeof rel);
        if(ispack&&u->native_toplevels&&w->layout==TG_PLACE) q=w->place_natural;
        if(ispack||w->layout!=TG_PLACE) q.x=q.y=0;
        for(size_t k=2;k<n;k+=2) {
            if(ispack&&!strcmp(a[k],"-side")) {
                if(!strcmp(a[k+1],"top")) layout=TG_PACK_TOP;
                else if(!strcmp(a[k+1],"left")) layout=TG_PACK_LEFT;
                else if(!strcmp(a[k+1],"bottom")) layout=TG_PACK_BOTTOM;
                else if(!strcmp(a[k+1],"right")) layout=TG_PACK_RIGHT;
                else return out(b,cap,"unsupported pack side",TG_ERROR);
            } else if(ispack&&!strcmp(a[k],"-fill")) {
                if(!strcmp(a[k+1],"none")) fill=1;
                else if(!strcmp(a[k+1],"x")) fill=2;
                else if(!strcmp(a[k+1],"y")) fill=3;
                else if(!strcmp(a[k+1],"both")) fill=4;
                else goto bad;
            } else if(ispack&&!strcmp(a[k],"-expand")) {
                if(!strcmp(a[k+1],"1")||!strcmp(a[k+1],"true")||!strcmp(a[k+1],"yes")||!strcmp(a[k+1],"on")) expand=1;
                else if(!strcmp(a[k+1],"0")||!strcmp(a[k+1],"false")||!strcmp(a[k+1],"no")||!strcmp(a[k+1],"off")) expand=0;
                else goto bad;
            } else if(!strcmp(a[k],"-anchor")) { if(!(align=anchor(a[k+1]))) goto bad; }
            else if(ispack&&!strcmp(a[k],"-padx")) { if(!pad_value(a[k+1],&padx,&padx_extra)) goto bad; }
            else if(ispack&&!strcmp(a[k],"-pady")) { if(!pad_value(a[k+1],&pady,&pady_extra)) goto bad; }
            else if(ispack&&(!strcmp(a[k],"-ipadx")||!strcmp(a[k],"-ipady"))) {
                int *field=!strcmp(a[k],"-ipadx")?&ipadx:&ipady;
                if(!dim(a[k+1],field)) goto bad;
            }
            else if(ispack&&!strcmp(a[k],"-in")) {
                tg_id in_id=tg_ui_find(u,a[k+1]);
                if(!in_id||in_id==id) goto bad;
                /* Reject cycles (in_id cannot be a descendant of id). */
                for(tg_id p=in_id;p;p=tg_ui_get(u,p)?tg_ui_get(u,p)->parent:0) if(p==id) goto bad;
                if(in_id!=w->parent) {
                    tg_widget *mut=tg_lookup(u,id),*old_p=tg_lookup(u,w->parent),*new_p=tg_lookup(u,in_id);
                    if(!mut||!new_p) goto bad;
                    if(old_p&&old_p->child_count) old_p->child_count--;
                    new_p->child_count++;
                    mut->parent=in_id;
                    already_packed=0;
                }
            }
            else if(ispack&&(!strcmp(a[k],"-before")||!strcmp(a[k],"-after"))) {
                relative=tg_ui_find(u,a[k+1]); const tg_widget *r=tg_ui_get(u,relative);
                if(!r||r->id==id||r->parent!=w->parent||!manager_matches(r->layout,"pack")) goto bad;
                after=!strcmp(a[k],"-after");
            }
            else if(!ispack&&!strcmp(a[k],"-bordermode")) {
                if(!strcmp(a[k+1],"inside")) border_mode=0;
                else if(!strcmp(a[k+1],"outside")) border_mode=1;
                else if(!strcmp(a[k+1],"ignore")) border_mode=2;
                else goto bad;
            }
            else if(!ispack&&!strcmp(a[k],"-x")) { if(!offset_value(a[k+1],&q.x)) goto bad; }
            else if(!ispack&&!strcmp(a[k],"-y")) { if(!offset_value(a[k+1],&q.y)) goto bad; }
            else if(!ispack&&(!strcmp(a[k],"-width")||!strcmp(a[k],"-height"))) {
                unsigned bit=!strcmp(a[k],"-width")?1:2; int *field=bit==1?&q.w:&q.h;
                if(!*a[k+1]) { flags&=~bit; *field=u->native_toplevels?(w->layout==TG_PLACE?(bit==1?w->place_natural.w:w->place_natural.h):(bit==1?q.w:q.h)):(bit==1?100:32); }
                else { if(!offset_value(a[k+1],field)) goto bad; flags|=bit; }
            } else if(!ispack&&(!strcmp(a[k],"-relx")||!strcmp(a[k],"-rely")||!strcmp(a[k],"-relwidth")||!strcmp(a[k],"-relheight"))) {
                int index=!strcmp(a[k],"-relx")?0:!strcmp(a[k],"-rely")?1:!strcmp(a[k],"-relwidth")?2:3;
                if(index>=2&&!*a[k+1]) { flags&=~(1u<<index); rel[index]=0; }
                else {
                    if(!fraction(a[k+1],&rel[index])) goto bad;
                    if(index>=2) flags|=1u<<index;
                }
            }
            else return out(b,cap,"unsupported geometry option",TG_ERROR);
        }
        if(tg_ui_geometry(u,id,layout,q)) goto bad;
        tg_widget *target=tg_lookup(u,id);
        if(ispack) { if(!already_packed) { uint32_t rank=0; for(size_t j=1;j<u->count;j++) { const tg_widget *s=&u->items[j]; if(s->id==id||s->parent!=target->parent||!manager_matches(s->layout,"pack")) continue; uint32_t n=s->pack_order?s->pack_order:s->id; if(n>rank) rank=n; } target->pack_order=rank+1; } target->pack_fill=fill; target->pack_expand=expand; target->pack_padx=padx; target->pack_pady=pady; target->pack_padx_extra=padx_extra; target->pack_pady_extra=pady_extra; target->pack_ipadx=ipadx; target->pack_ipady=ipady; target->pack_anchor=align; if(relative) tg_pack_reorder(u,id,relative,after); }
        else { target->place_anchor=align; target->place_border_mode=border_mode; target->place_flags=flags; memcpy(target->place_rel,rel,sizeof rel); }
        return out(b,cap,"",TG_OK);
    }
    if(!strcmp(a[0],"winfo")) {
        if(n==4&&!strcmp(a[1],"rgb")) {
            if(!tg_ui_get(u,tg_ui_find(u,a[2]))) return out(b,cap,"unknown widget",TG_ERROR);
            uint32_t rgb=0;
            if(!color(a[3],&rgb)) return out(b,cap,"unknown color name",TG_ERROR);
            unsigned r=((rgb>>16)&0xffu)*257u,g=((rgb>>8)&0xffu)*257u,bl=(rgb&0xffu)*257u;
            int z=snprintf(b,cap,"%u %u %u",r,g,bl);
            return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR;
        }
        if(n!=3) return out(b,cap,"wrong # args",TG_ERROR);
        tg_id id=tg_ui_find(u,a[2]); const tg_widget *w=tg_ui_get(u,id);
        if(!strcmp(a[1],"exists")) return out(b,cap,w?"1":"0",TG_OK);
        if(!w) return out(b,cap,"unknown widget",TG_ERROR);
        if(!strcmp(a[1],"width")) snprintf(b,cap,"%d",w->rect.w);
        else if(!strcmp(a[1],"height")) snprintf(b,cap,"%d",w->rect.h);
        else if(!strcmp(a[1],"rootx")||!strcmp(a[1],"rooty")) snprintf(b,cap,"%d",!strcmp(a[1],"rootx")?w->rect.x:w->rect.y);
        else if(!strcmp(a[1],"x")||!strcmp(a[1],"y")) {
            const tg_widget *parent=tg_ui_get(u,w->parent);
            snprintf(b,cap,"%d",!strcmp(a[1],"x")?w->rect.x-(parent?parent->rect.x:0):w->rect.y-(parent?parent->rect.y:0));
        } else if(!strcmp(a[1],"reqwidth")) snprintf(b,cap,"%d",w->layout==TG_GRID?w->grid_natural.w:u->native_toplevels&&w->layout==TG_PLACE?w->place_natural.w:w->request.w);
        else if(!strcmp(a[1],"reqheight")) snprintf(b,cap,"%d",w->layout==TG_GRID?w->grid_natural.h:u->native_toplevels&&w->layout==TG_PLACE?w->place_natural.h:w->request.h);
        else if(!strcmp(a[1],"manager")) return out(b,cap,manager_name(u,w),TG_OK);
        else if(!strcmp(a[1],"ismapped")) return out(b,cap,mapped(u,w)?"1":"0",TG_OK);
        else if(!strcmp(a[1],"viewable")) return out(b,cap,viewable(u,w)?"1":"0",TG_OK);
        else if(!strcmp(a[1],"geometry")) {
            const tg_widget *p=tg_ui_get(u,w->parent);
            snprintf(b,cap,"%dx%d%+d%+d",w->rect.w,w->rect.h,w->rect.x-(p?p->rect.x:0),w->rect.y-(p?p->rect.y:0));
        } else if(!strcmp(a[1],"toplevel")) {
            const tg_widget *p=w;
            while(p&&p->kind!=TG_TOPLEVEL) p=tg_ui_get(u,p->parent);
            return out(b,cap,p?p->path:".",TG_OK);
        } else if(!strcmp(a[1],"class")) {
            static const char *classic[]={"","Frame","Label","Button","Canvas","Entry","Toplevel","Progressbar","Separator","Scale","Scrollbar","Labelframe","Combobox","Spinbox","Notebook","Treeview","Checkbutton","Radiobutton","Panedwindow","Menubutton","Sizegrip","Menu","Listbox","Text","Message"};
            if(w->id==1) return out(b,cap,"Tk",TG_OK);
            if(w->themed&&w->kind!=TG_TREEVIEW) {
                int z=snprintf(b,cap,"T%s",classic[w->kind]); return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR;
            }
            return out(b,cap,classic[w->kind],TG_OK);
        }
        else if(!strcmp(a[1],"parent")) { const tg_widget *p=tg_ui_get(u,w->parent); return out(b,cap,p?p->path:"",TG_OK); }
        else if(!strcmp(a[1],"name")) { const char *p=strrchr(w->path,'.'); return out(b,cap,w->id==1?".":p?p+1:w->path,TG_OK); }
        else if(!strcmp(a[1],"children")) {
            if(!cap) return TG_ERROR; b[0]=0;
            for(size_t k=1;k<u->count;k++) if(u->items[k].parent==id&&!tg_list_append(b,cap,u->items[k].path)) return out(b,cap,"children list too long",TG_ERROR);
            return TG_OK;
        }
        else return out(b,cap,"unsupported winfo option",TG_ERROR);
        return TG_OK;
    }
    return -1;
bad:
    return out(b,cap,"invalid geometry",TG_ERROR);
}
