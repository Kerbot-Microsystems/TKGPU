#include "TKGPUStyle.H"
#include "../TKGPUWidgetInternal.H"
#include "../../core/TKGPUList.H"
#include "../image/TKGPUImageRegistry.H"
#include "TKGPUText.H"
#include "TKGPURaster.H"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static int parse_color(const char *s,uint32_t *out) {
    if(!strcmp(s,"black")) { *out=0; return 1; }
    if(!strcmp(s,"white")) { *out=0xffffff; return 1; }
    if(!strcmp(s,"red")) { *out=0xff0000; return 1; }
    if(!strcmp(s,"blue")) { *out=0x0000ff; return 1; }
    if(strlen(s)!=7||*s!='#') return 0;
    uint32_t v=0;
    for(int k=1;k<7;k++) {
        unsigned char c=(unsigned char)s[k]; unsigned d;
        if(c>='0'&&c<='9') d=c-'0';
        else if(c>='A'&&c<='F') d=c-'A'+10;
        else if(c>='a'&&c<='f') d=c-'a'+10;
        else return 0;
        v=(v<<4)|d;
    }
    *out=v; return 1;
}
static int alpha_color(const char *key,const char *s,uint32_t *rgb,unsigned *alpha) {
    *alpha=255;
    if(strlen(s)==9&&s[0]=='#'&&(!strcmp(key,"-background")||!strcmp(key,"-fieldbackground"))) {
        char base[8]; memcpy(base,s,7); base[7]=0;
        char tail[8]={'#','0','0','0','0',s[7],s[8],0}; uint32_t a;
        if(!parse_color(base,rgb)||!parse_color(tail,&a)) return 0;
        *alpha=(unsigned)a; return 1;
    }
    return parse_color(s,rgb);
}
static const char *base(tg_kind kind) {
    switch(kind) {
    case TG_FRAME: return "TFrame";
    case TG_LABEL: return "TLabel";
    case TG_BUTTON: return "TButton";
    case TG_ENTRY: return "TEntry";
    case TG_PROGRESS: return "Horizontal.TProgressbar";
    case TG_SEPARATOR: return "TSeparator";
    case TG_SCALE: return "Horizontal.TScale";
    case TG_SCROLLBAR: return "Horizontal.TScrollbar";
    case TG_LABELFRAME: return "TLabelframe";
    case TG_COMBOBOX: return "TCombobox";
    case TG_SPINBOX: return "TSpinbox";
    case TG_NOTEBOOK: return "TNotebook";
    case TG_TREEVIEW: return "Treeview";
    case TG_CHECKBUTTON: return "TCheckbutton";
    case TG_RADIOBUTTON: return "TRadiobutton";
    case TG_PANEDWINDOW: return "TPanedwindow";
    case TG_MENUBUTTON: return "TMenubutton";
    case TG_SIZEGRIP: return "TSizegrip";
    default: return "";
    }
}

enum { ACTIVE=1,DISABLED=2,FOCUS=4,PRESSED=8,SELECTED=16,READONLY=32,ALTERNATE=64,HOVER=512 };
static const char *state_names[]={"active","disabled","focus","pressed","selected","readonly","alternate","background","invalid","hover"};
static const char *theme(const tg_ui *u) { return u->theme[0]?u->theme:"clam"; }
const char *tg_style_current_theme(const tg_ui *u) { return theme(u); }
static const char *const builtin_themes[]={"default","clam","alt","classic"};
static int known_theme(const tg_ui *u,const char *name) {
    for(size_t k=0;k<sizeof builtin_themes/sizeof builtin_themes[0];k++)
        if(!strcmp(name,builtin_themes[k])) return 1;
    for(unsigned k=0;k<u->theme_count;k++) if(!strcmp(name,u->themes[k].name)) return 1;
    return 0;
}
static const char *parent_theme(const tg_ui *u,const char *name) {
    for(unsigned k=0;k<u->theme_count;k++)
        if(!strcmp(name,u->themes[k].name)) return u->themes[k].parent;
    return NULL;
}
int tg_style_select_theme(tg_ui *u,const char *name) {
    if(!u||!name||!known_theme(u,name)) return TG_ERROR;
    strcpy(u->theme,name); return TG_OK;
}
static const tg_style *find_in(const tg_ui *u,const char *name,const char *which) {
    for(size_t k=0;k<u->style_count;k++)
        if(!strcmp(u->styles[k].name,name)&&!strcmp(u->styles[k].theme,which)) return &u->styles[k];
    return NULL;
}
static const tg_style *find(const tg_ui *u,const char *name) { return find_in(u,name,theme(u)); }
static const tg_style_property *property(const tg_style *s,const char *key) {
    if(s) for(size_t k=0;k<s->count;k++) if(!strcmp(s->properties[k].key,key)) return &s->properties[k];
    return NULL;
}
static int allowed(const char *key) {
    if(!key || key[0] != '-' || strlen(key) < 2 || strlen(key) >= sizeof(((tg_style_property *)0)->key)) return 0;
    return 1;
}
static int color_key(const char *key) { return strstr(key,"color")||strstr(key,"background")||strstr(key,"foreground"); }
static int states(const char *s,unsigned *yes,unsigned *no) {
    const char *const *names=state_names;
    char word[64]; int rc; *yes=*no=0;
    while((rc=tg_list_next(&s,word,sizeof word))>0) {
        int negative=word[0]=='!'; unsigned k=0;
        while(k<sizeof state_names/sizeof state_names[0]&&strcmp(word+negative,names[k])) k++;
        if(k==sizeof state_names/sizeof state_names[0]) return 0;
        if(negative) *no|=1u<<k; else *yes|=1u<<k;
    }
    return rc==0&&!(*yes&*no);
}
static int map_value(const char *map,const char *key,unsigned actual,char *out,size_t cap,int validate) {
    char spec[128],value[TG_STYLE_VALUE]; int rc;
    while((rc=tg_list_next(&map,spec,sizeof spec))>0) {
        unsigned yes,no,alpha; uint32_t color;
        if(!states(spec,&yes,&no)||tg_list_next(&map,value,sizeof value)!=1||
           (color_key(key)&&!alpha_color(key,value,&color,&alpha))) return -1;
        if(!validate&&(actual&yes)==yes&&!(actual&no)) {
            if(strlen(value)>=cap) return -1;
            strcpy(out,value); return 1;
        }
    }
    return rc<0?-1:0;
}
static const char *next_base(const char *name) {
    if(!strcmp(name,".")) return NULL;
    const char *dot=strchr(name,'.'); return dot?dot+1:".";
}
static int resolve(const tg_ui *u,const char *name,const char *key,unsigned actual,char *out,size_t cap) {
    /* A child theme can override an inherited style property without
       copying the entire parent. Themes are created only with an existing
       parent, so the immutable parent chain is acyclic. */
    for(const char *which=theme(u);which;which=parent_theme(u,which))
        for(const char *p=name;p;p=next_base(p)) {
            const tg_style_property *q=property(find_in(u,p,which),key);
            if(q&&q->has_map) { int rc=map_value(q->map,key,actual,out,cap,0); if(rc) return rc; }
        }
    for(const char *which=theme(u);which;which=parent_theme(u,which))
        for(const char *p=name;p;p=next_base(p)) {
            const tg_style_property *q=property(find_in(u,p,which),key);
            if(q&&q->has_value) {
                if(strlen(q->value)>=cap) return -1;
                strcpy(out,q->value); return 1;
            }
        }
    return 0;
}
int tg_style_lookup_value(const tg_ui *u,const char *name,const char *key,int selected,int disabled,char *out,size_t cap) {
    return resolve(u,name,key,(selected?SELECTED:0)|(disabled?DISABLED:0),out,cap);
}
int tg_style_lookup_color(const tg_ui *u,const char *name,const char *key,int selected,int disabled,uint32_t *out) {
    char value[TG_STYLE_VALUE]; unsigned alpha;
    return tg_style_lookup_value(u,name,key,selected,disabled,value,sizeof value)>0&&alpha_color(key,value,out,&alpha);
}
int tg_style_option(tg_widget *w,const char *name,const char *value) {
    if(strcmp(name,"-style")||!w->themed) return -1;
    if(strlen(value)>=sizeof w->style) return TG_ERROR;
    strcpy(w->style,value); return TG_OK;
}
static unsigned widget_states(const tg_ui *u,const tg_widget *w) {
    unsigned actual=0;
    if(w->state==TG_STATE_DISABLED) actual|=DISABLED;
    if(w->state==TG_STATE_READONLY) actual|=READONLY;
    if(u->hover==w->id) actual|=ACTIVE|HOVER;
    if(u->pressed==w->id) actual|=PRESSED;
    if(u->focus==w->id) actual|=FOCUS;
    if(w->checked) actual|=SELECTED;
    if(w->alternate) actual|=ALTERNATE;
    return (actual|w->ttk_state_set)&~w->ttk_state_clear;
}
static const char *widget_style_name(const tg_widget *w) {
    if(w->style[0]) return w->style;
    if(w->vertical) {
        if(w->kind==TG_PROGRESS) return "Vertical.TProgressbar";
        if(w->kind==TG_SCALE) return "Vertical.TScale";
        if(w->kind==TG_SCROLLBAR) return "Vertical.TScrollbar";
    }
    return base(w->kind);
}
int tg_style_widget_value(const tg_ui *u,const tg_widget *w,const char *key,char *out,size_t cap) {
    return resolve(u,widget_style_name(w),key,widget_states(u,w),out,cap);
}
int tg_style_widget_color(const tg_ui *u,const tg_widget *w,const char *key,uint32_t *out) {
    char value[TG_STYLE_VALUE]; unsigned alpha;
    return tg_style_widget_value(u,w,key,value,sizeof value)>0&&alpha_color(key,value,out,&alpha);
}
static int append_states(char *out,size_t cap,unsigned mask,unsigned values) {
    if(!cap) return TG_ERROR;
    out[0]=0;
    for(unsigned k=0;k<sizeof state_names/sizeof state_names[0];k++) {
        unsigned bit=1u<<k;
        if(!(mask&bit)) continue;
        char word[32];
        snprintf(word,sizeof word,"%s%s",(values&bit)?"":"!",state_names[k]);
        if(!tg_list_append(out,cap,word)) return TG_ERROR;
    }
    return TG_OK;
}
int tg_ttk_state_matches(const tg_ui *u,const tg_widget *w,const char *spec,int *matches) {
    unsigned yes,no;
    if(!w||!w->themed||!states(spec,&yes,&no)) return TG_ERROR;
    unsigned bits=widget_states(u,w);
    *matches=(bits&yes)==yes&&!(bits&no); return TG_OK;
}
int tg_ttk_state_command(tg_ui *u,tg_widget *w,size_t n,char **a,char *out,size_t cap) {
    if(!w->themed||n<2) return -1;
    if(!strcmp(a[1],"instate")) {
        int match;
        if(n!=3||cap<2||tg_ttk_state_matches(u,w,a[2],&match)!=TG_OK) return TG_ERROR;
        strcpy(out,match?"1":"0"); return TG_OK;
    }
    if(strcmp(a[1],"state")) return -1;
    unsigned old=widget_states(u,w);
    if(n==2) return append_states(out,cap,old,old);
    unsigned yes,no;
    if(n!=3||!states(a[2],&yes,&no)) return TG_ERROR;
    unsigned next=(old|yes)&~no;
    if(append_states(out,cap,old^next,old)!=TG_OK) return TG_ERROR;
    unsigned mask=yes|no;
    /* Keep behavioral flags in sync with the controls and retain the second
       state bit if both readonly and disabled are present. */
    if(mask&(DISABLED|READONLY)) {
        w->state=(next&DISABLED)?TG_STATE_DISABLED:(next&READONLY)?TG_STATE_READONLY:TG_STATE_NORMAL;
        mask|=DISABLED|READONLY;
    }
    if(mask&SELECTED) w->checked=(next&SELECTED)!=0;
    if(mask&ALTERNATE) w->alternate=(next&ALTERNATE)!=0;
    unsigned override=mask&~(SELECTED|ALTERNATE);
    w->ttk_state_set=(w->ttk_state_set&~override)|(next&override);
    w->ttk_state_clear=(w->ttk_state_clear&~override)|(~next&override);
    return TG_OK;
}
void tg_style_colors_alpha(const tg_ui *u,const tg_widget *w,uint32_t *bg,uint32_t *fg,unsigned *alpha) {
    if(!w->themed) return;
    const char *name=widget_style_name(w);
    unsigned actual=widget_states(u,w);
    char v[TG_STYLE_VALUE];
    const char *key=w->kind==TG_ENTRY||w->kind==TG_COMBOBOX||w->kind==TG_SPINBOX?"-fieldbackground":"-background";
    int rc=resolve(u,name,key,actual,v,sizeof v);
    if(!rc&&strcmp(key,"-background")) rc=resolve(u,name,"-background",actual,v,sizeof v);
    if(rc>0) (void)alpha_color(key,v,bg,alpha);
    else if(w->state==TG_STATE_DISABLED) { *bg=0x808080; *alpha=255; }
    else if(w->kind==TG_BUTTON&&u->pressed==w->id) { *bg=0x23568f; *alpha=255; }
    if(resolve(u,name,"-foreground",actual,v,sizeof v)>0) (void)parse_color(v,fg);
}
void tg_style_colors(const tg_ui *u,const tg_widget *w,uint32_t *bg,uint32_t *fg) {
    unsigned alpha=255; tg_style_colors_alpha(u,w,bg,fg,&alpha);
}
static int result(char *out,size_t cap,const char *s,int rc) {
    if(!cap||strlen(s)>=cap) return TG_ERROR;
    strcpy(out,s); return rc;
}
static const char *const simple_elements[]={
    "Button.border","Button.focus","Button.padding","Button.label",
    "Label.border","Label.padding","Label.label","Frame.border",
    "Entry.border","Entry.body",
    "Combobox.border","Combobox.body",
    "Spinbox.border","Spinbox.body",
    "Progressbar.border","Progressbar.body",
    "Separator.border","Separator.body",
    "Scale.border","Scale.body",
    "Scrollbar.border","Scrollbar.body",
    "Labelframe.border","Labelframe.body",
    "Notebook.border","Notebook.body",
    "Treeview.border","Treeview.body",
    "Checkbutton.border","Checkbutton.body",
    "Radiobutton.border","Radiobutton.body",
    "Panedwindow.border","Panedwindow.body",
    "Menubutton.border","Menubutton.body",
    "Sizegrip.border","Sizegrip.body"
};
static const tg_style_element *element_find(const tg_ui *u,const char *name) {
    for(const char *which=theme(u);which;which=parent_theme(u,which))
        for(unsigned k=0;k<u->element_count;k++)
            if(!strcmp(u->elements[k].theme,which)&&!strcmp(u->elements[k].name,name)) return &u->elements[k];
    return NULL;
}
static int simple_element(const char *name) {
    for(unsigned k=0;k<sizeof simple_elements/sizeof simple_elements[0];k++)
        if(!strcmp(name,simple_elements[k])) return 1;
    return 0;
}
static const char *simple_layout(const char *name) {
    for(const char *p=name;p;p=next_base(p)) {
        if(!strcmp(p,"TButton")) return "Button.border -sticky nsew -children {Button.focus -sticky nsew -children {Button.padding -sticky nsew -children {Button.label -sticky nsew}}}";
        if(!strcmp(p,"TLabel")) return "Label.border -sticky nsew -children {Label.padding -sticky nsew -children {Label.label -sticky nsew}}";
        if(!strcmp(p,"TFrame")) return "Frame.border -sticky nsew";
        if(!strcmp(p,"TEntry")) return "Entry.border -sticky nsew -children {Entry.body -sticky nsew}";
        if(!strcmp(p,"TCombobox")) return "Combobox.border -sticky nsew -children {Combobox.body -sticky nsew}";
        if(!strcmp(p,"TSpinbox")) return "Spinbox.border -sticky nsew -children {Spinbox.body -sticky nsew}";
        if(!strcmp(p,"TProgressbar")) return "Progressbar.border -sticky nsew -children {Progressbar.body -sticky nsew}";
        if(!strcmp(p,"TSeparator")) return "Separator.border -sticky nsew -children {Separator.body -sticky nsew}";
        if(!strcmp(p,"TScale")) return "Scale.border -sticky nsew -children {Scale.body -sticky nsew}";
        if(!strcmp(p,"TScrollbar")) return "Scrollbar.border -sticky nsew -children {Scrollbar.body -sticky nsew}";
        if(!strcmp(p,"TLabelframe")) return "Labelframe.border -sticky nsew -children {Labelframe.body -sticky nsew}";
        if(!strcmp(p,"TNotebook")) return "Notebook.border -sticky nsew -children {Notebook.body -sticky nsew}";
        if(!strcmp(p,"Treeview")) return "Treeview.border -sticky nsew -children {Treeview.body -sticky nsew}";
        if(!strcmp(p,"TCheckbutton")) return "Checkbutton.border -sticky nsew -children {Checkbutton.body -sticky nsew}";
        if(!strcmp(p,"TRadiobutton")) return "Radiobutton.border -sticky nsew -children {Radiobutton.body -sticky nsew}";
        if(!strcmp(p,"TPanedwindow")) return "Panedwindow.border -sticky nsew -children {Panedwindow.body -sticky nsew}";
        if(!strcmp(p,"TMenubutton")) return "Menubutton.border -sticky nsew -children {Menubutton.body -sticky nsew}";
        if(!strcmp(p,"TSizegrip")) return "Sizegrip.border -sticky nsew -children {Sizegrip.body -sticky nsew}";
    }
    return NULL;
}
static const tg_style *layout_find(const tg_ui *u,const char *name) {
    for(const char *which=theme(u);which;which=parent_theme(u,which))
        for(const char *p=name;p;p=next_base(p)) {
            const tg_style *s=find_in(u,p,which);
            if(s&&s->has_layout) return s;
        }
    return NULL;
}
static int sticky_bits(const char *s,unsigned *bits) {
    *bits=0; const char *alphabet="nsew";
    for(;*s;s++) {
        const char *p=strchr(alphabet,*s);
        if(!p) return 0;
        *bits|=1u<<(p-alphabet);
    }
    return 1;
}
static int layout_parse(const tg_ui *u,tg_style *s,const char *input,int depth,int *root) {
    if(depth>8) return 0;
    char word[TG_LAYOUT_BYTES],value[TG_LAYOUT_BYTES]; int rc,previous=-1;
    *root=-1;
    while((rc=tg_list_next(&input,word,sizeof word))>0) {
        if(strlen(word)>=sizeof s->nodes[0].name||
           (!simple_element(word)&&!element_find(u,word))||s->node_count==TG_LAYOUT_NODES) return 0;
        int index=s->node_count++;
        tg_style_node *node=&s->nodes[index];
        memset(node,0,sizeof *node); strcpy(node->name,word); node->sticky=15;
        node->child=node->next=-1;
        if(previous<0) *root=index; else s->nodes[previous].next=index;
        previous=index; unsigned seen=0;
        for(;;) {
            const char *next=input;
            rc=tg_list_next(&next,word,sizeof word);
            if(rc<0) return 0;
            if(!rc||word[0]!='-') break;
            input=next;
            if(tg_list_next(&input,value,sizeof value)!=1) return 0;
            if(!strcmp(word,"-sticky")&&!(seen&1)) {
                if(!sticky_bits(value,&node->sticky)) return 0;
                seen|=1;
            } else if(!strcmp(word,"-children")&&!(seen&2)) {
                if(!layout_parse(u,s,value,depth+1,&node->child)) return 0;
                seen|=2;
            } else if(!strcmp(word,"-side")&&!(seen&4)) {
                if(!strcmp(value,"left")) node->side=1;
                else if(!strcmp(value,"right")) node->side=2;
                else if(!strcmp(value,"top")) node->side=3;
                else if(!strcmp(value,"bottom")) node->side=4;
                else return 0;
                seen|=4;
            } else return 0;
        }
    }
    return rc==0;
}
static int style_store(tg_ui *u,const tg_style *old,const tg_style *s) {
    if(old) { u->styles[old-u->styles]=*s; return 1; }
    if(u->style_count==TG_STYLE_LIMIT) return 0;
    if(u->style_count==u->style_capacity) {
        size_t cap=u->style_capacity?u->style_capacity*2:4;
        if(cap>TG_STYLE_LIMIT) cap=TG_STYLE_LIMIT;
        tg_style *next=realloc(u->styles,cap*sizeof *next);
        if(!next) return 0;
        u->styles=next; u->style_capacity=cap;
    }
    u->styles[u->style_count++]=*s; return 1;
}
static int image_spec(const tg_ui *u,const char *input,unsigned actual,char *out,size_t cap,int validate) {
    char first[128],spec[128],image[128]; int rc,matched=0;
    if(tg_list_next(&input,first,sizeof first)!=1||strlen(first)>=cap||
       (validate&&!tg_image_find(u,first))) return 0;
    strcpy(out,first);
    while((rc=tg_list_next(&input,spec,sizeof spec))>0) {
        unsigned yes,no;
        if(!states(spec,&yes,&no)||tg_list_next(&input,image,sizeof image)!=1||strlen(image)>=cap||
           (validate&&!tg_image_find(u,image))) return 0;
        if(!matched&&(actual&yes)==yes&&!(actual&no)) { strcpy(out,image); matched=1; }
    }
    return rc==0;
}
static const tg_photo *element_photo(const tg_ui *u,const tg_style_element *el,unsigned state) {
    char name[128]; if(!el||!image_spec(u,el->images,state,name,sizeof name,0)) return NULL;
    const tg_image_slot *slot=tg_image_find(u,name); return slot?slot->photo:NULL;
}
static int min_size(int n,int max) { return n<max?n:max; }
static tg_rect inset_rect(tg_rect r,int n) {
    int x=min_size(n,r.w/2),y=min_size(n,r.h/2);
    return (tg_rect){r.x+x,r.y+y,r.w-2*x,r.h-2*y};
}
static int layout_is_simple(const tg_widget *w) {
    return w->kind==TG_BUTTON||w->kind==TG_LABEL||w->kind==TG_FRAME;
}
typedef struct { int x,y; const char *name; } layout_hit;
static int inside_rect(tg_rect r,int x,int y) {
    return r.w>0&&r.h>0&&x>=r.x&&y>=r.y&&x-r.x<r.w&&y-r.y<r.h;
}
/* Painting and identify traverse the exact same parcels. For image and label
   elements only their clipped visible footprint is hit; containers use the
   full parcel. A later/deeper matching element wins, as in the paint order. */
static void layout_draw(const tg_ui *u,const tg_widget *w,tg_canvas *c,const tg_style *style,
                        int index,tg_rect remaining,uint32_t bg,uint32_t fg,unsigned alpha,unsigned state,int decoration,layout_hit *hit) {
    for(;index>=0;index=style->nodes[index].next) {
        const tg_style_node *node=&style->nodes[index];
        const tg_style_element *el=element_find(u,node->name);
        const tg_photo *photo=element_photo(u,el,state);
        int tw=0,th=0,pad=el?el->padding:0;
        int label=!strcmp(node->name,"Button.label")||!strcmp(node->name,"Label.label");
        if(label) {
            if(!tg_text_native_enabled()||tg_text_font_measure(w->text,w->font_spec,&tw,&th)!=TG_OK)
                (void)tg_text_measure(w->text,1,&tw,&th);
        } else if(photo) { tw=tg_photo_width(photo); th=tg_photo_height(photo); }
        tg_rect parcel=remaining;
        if(node->side==1||node->side==2) {
            int width=min_size(tw+2*pad,remaining.w);
            if(node->side==2) parcel.x+=remaining.w-width;
            else remaining.x+=width;
            parcel.w=width; remaining.w-=width;
        } else if(node->side==3||node->side==4) {
            int height=min_size(th+2*pad,remaining.h);
            if(node->side==4) parcel.y+=remaining.h-height;
            else remaining.y+=height;
            parcel.h=height; remaining.h-=height;
        }
        if(parcel.w<=0||parcel.h<=0) continue;
        if(!el&&strstr(node->name,".border")) {
            if(c) {
                if(!decoration) tg_canvas_fill_alpha(c,parcel,bg,alpha);
                uint32_t border=fg; (void)tg_style_widget_color(u,w,"-bordercolor",&border);
                tg_raster_outline(c,w->rect,parcel,border);
            }
            pad=1;
        }
        if(!el&&strstr(node->name,".padding")) {
            int px=w->text_padx,py=w->text_pady; pad=px>py?px:py;
        }
        if(!el&&strstr(node->name,".focus")&&(state&FOCUS)) {
            if(c) tg_raster_outline(c,w->rect,parcel,fg);
            pad=1;
        }
        tg_rect inside=inset_rect(parcel,pad);
        if(hit&&!photo&&!label&&!el&&inside_rect(parcel,hit->x,hit->y)) hit->name=node->name;
        if(photo||label) {
            int x=inside.x+(inside.w-tw)/2,y=inside.y+(inside.h-th)/2;
            if((node->sticky&12)==8) x=inside.x;
            else if((node->sticky&12)==4) x=inside.x+inside.w-tw;
            if((node->sticky&3)==1) y=inside.y;
            else if((node->sticky&3)==2) y=inside.y+inside.h-th;
            if(hit&&inside_rect(inside,hit->x,hit->y)&&
               inside_rect((tg_rect){x,y,tw,th},hit->x,hit->y)) hit->name=node->name;
            if(c) {
                if(photo) (void)tg_photo_paint(photo,c,inside,x,y);
                else if(!tg_text_native_enabled()||tg_text_font_draw(c,inside,x,y,w->text,w->font_spec,fg)!=TG_OK)
                    tg_text_draw(c,inside,x,y,w->text,1,fg);
            }
        }
        if(node->child>=0) layout_draw(u,w,c,style,node->child,inside,bg,fg,alpha,state,decoration,hit);
    }
}
int tg_style_layout_paint(const tg_ui *u,const tg_widget *w,tg_canvas *c,uint32_t bg,uint32_t fg,unsigned alpha) {
    if(!w->themed||!layout_is_simple(w)) return 0;
    const tg_style *s=layout_find(u,widget_style_name(w));
    if(!s) return 0;
    if(s->node_count) layout_draw(u,w,c,s,0,w->rect,bg,fg,alpha,widget_states(u,w),0,NULL);
    return 1;
}
void tg_style_layout_decorate(const tg_ui *u,const tg_widget *w,tg_canvas *c,uint32_t bg,uint32_t fg,unsigned alpha) {
    if(!w->themed||layout_is_simple(w)) return;
    const tg_style *s=layout_find(u,widget_style_name(w));
    /* Compound widgets retain their established semantic content and hit
       geometry. Explicit layout trees supply independent image/border
       decoration; body nodes reserve the native content position. */
    if(s&&s->node_count) layout_draw(u,w,c,s,0,w->rect,bg,fg,alpha,widget_states(u,w),1,NULL);
}

int tg_style_identify(const tg_ui *u,const tg_widget *w,size_t n,char **a,char *out,size_t cap) {
    /* Leave compound-specific identify subcommands (tree rows, notebook
       tabs, etc.) to their existing handlers. */
    if(!w->themed||w->kind==TG_PANEDWINDOW||w->kind==TG_NOTEBOOK||w->kind==TG_TREEVIEW||n!=4||strcmp(a[1],"identify")) return -1;
    const tg_style *s=layout_find(u,widget_style_name(w));
    if(!s) return -1;
    char *endx,*endy; long x=strtol(a[2],&endx,10),y=strtol(a[3],&endy,10);
    if(endx==a[2]||endy==a[3]||*endx||*endy||x<-TG_DIM_LIMIT||x>TG_DIM_LIMIT||y<-TG_DIM_LIMIT||y>TG_DIM_LIMIT)
        return TG_ERROR;
    if(x<0||y<0||x>=w->rect.w||y>=w->rect.h||!s->node_count) return result(out,cap,"",TG_OK);
    layout_hit hit={w->rect.x+(int)x,w->rect.y+(int)y,NULL};
    layout_draw(u,w,NULL,s,0,w->rect,0,0,255,widget_states(u,w),!layout_is_simple(w),&hit);
    return result(out,cap,hit.name?hit.name:"",TG_OK);
}
int tg_style_command(tg_ui *u,size_t n,char **a,char *out,size_t cap) {
    if(strcmp(a[0],"ttk::style")) return -1;
    if(n>=3&&!strcmp(a[1],"layout")) {
        if(!simple_layout(a[2])||strlen(a[2])>=64) goto bad;
        const tg_style *old=find(u,a[2]);
        if(n==3) {
            const tg_style *inherited=layout_find(u,a[2]);
            return result(out,cap,inherited?inherited->layout:simple_layout(a[2]),TG_OK);
        }
        if(n!=4||strlen(a[3])>=TG_LAYOUT_BYTES) goto bad;
        tg_style draft={0}; if(old) draft=*old;
        else { strcpy(draft.name,a[2]); strcpy(draft.theme,theme(u)); }
        draft.node_count=0; int root;
        if(!layout_parse(u,&draft,a[3],0,&root)) goto bad;
        draft.has_layout=1; strcpy(draft.layout,a[3]);
        if(!style_store(u,old,&draft)) goto bad;
        return result(out,cap,"",TG_OK);
    }
    if(n>=3&&!strcmp(a[1],"element")) {
        if(n==3&&!strcmp(a[2],"names")) {
            if(!cap) goto bad;
            out[0]=0;
            for(unsigned k=0;k<sizeof simple_elements/sizeof simple_elements[0];k++)
                if(!tg_list_append(out,cap,simple_elements[k])) goto bad;
            for(unsigned k=0;k<u->element_count;k++)
                if(element_find(u,u->elements[k].name)==&u->elements[k]&&
                   !tg_list_append(out,cap,u->elements[k].name)) goto bad;
            return TG_OK;
        }
        if(n==4&&!strcmp(a[2],"options")) {
            if(element_find(u,a[3])) return result(out,cap,"",TG_OK);
            if(simple_element(a[3])) return result(out,cap,"",TG_OK);
            goto bad;
        }
        if((n==6||n==8)&&!strcmp(a[2],"create")&&!strcmp(a[4],"image")) {
            if(!*a[3]||strlen(a[3])>=64||strlen(a[5])>=384||simple_element(a[3])||
               u->element_count==TG_ELEMENT_LIMIT) goto bad;
            for(unsigned k=0;k<u->element_count;k++)
                if(!strcmp(u->elements[k].theme,theme(u))&&!strcmp(u->elements[k].name,a[3])) goto bad;
            char name[128]; int padding=0;
            if(!image_spec(u,a[5],0,name,sizeof name,1)) goto bad;
            if(n==8) {
                char *end; long p=strtol(a[7],&end,10);
                if(strcmp(a[6],"-padding")||end==a[7]||*end||p<0||p>256) goto bad;
                padding=(int)p;
            }
            tg_style_element *el=&u->elements[u->element_count++];
            strcpy(el->name,a[3]); strcpy(el->theme,theme(u)); strcpy(el->images,a[5]); el->padding=padding;
            return result(out,cap,a[3],TG_OK);
        }
        goto bad;
    }
    if(n>=3&&(!strcmp(a[1],"configure")||!strcmp(a[1],"map"))) {
        int map=!strcmp(a[1],"map");
        if(!*a[2]||strlen(a[2])>=64) goto bad;
        const tg_style *old=find(u,a[2]);
        if(n==3) {
            if(!cap) goto bad;
            out[0]=0;
            if(old) for(size_t k=0;k<old->count;k++) {
                const tg_style_property *q=&old->properties[k];
                if((map?q->has_map:q->has_value)&&(!tg_list_append(out,cap,q->key)||
                    !tg_list_append(out,cap,map?q->map:q->value))) goto bad;
            }
            return TG_OK;
        }
        if(n==4) {
            const tg_style_property *q=property(old,a[3]);
            return result(out,cap,q&&(map?q->has_map:q->has_value)?(map?q->map:q->value):"",TG_OK);
        }
        if(n%2!=1) goto bad;
        tg_style s={0}; if(old) s=*old; else { strcpy(s.name,a[2]); strcpy(s.theme,theme(u)); }
        for(size_t k=3;k<n;k+=2) {
            if(!allowed(a[k])||strlen(a[k+1])>=TG_STYLE_VALUE) goto bad;
            uint32_t color; unsigned alpha;
            if(map) { char dummy[1]; if(map_value(a[k+1],a[k],0,dummy,sizeof dummy,1)<0) goto bad; }
            else if(color_key(a[k])&&!alpha_color(a[k],a[k+1],&color,&alpha)) goto bad;
            tg_style_property *q=NULL;
            for(size_t j=0;j<s.count;j++) if(!strcmp(s.properties[j].key,a[k])) { q=&s.properties[j]; break; }
            if(!q) {
                if(s.count==TG_STYLE_OPTIONS) goto bad;
                q=&s.properties[s.count++]; strcpy(q->key,a[k]);
            }
            if(map) { q->has_map=1; strcpy(q->map,a[k+1]); }
            else { q->has_value=1; strcpy(q->value,a[k+1]); }
        }
        if(old) u->styles[old-u->styles]=s;
        else {
            if(u->style_count==TG_STYLE_LIMIT) goto bad;
            if(u->style_count==u->style_capacity) {
                size_t cap=u->style_capacity?u->style_capacity*2:4;
                if(cap>TG_STYLE_LIMIT) cap=TG_STYLE_LIMIT;
                tg_style *next=realloc(u->styles,cap*sizeof *next);
                if(!next) goto bad;
                u->styles=next; u->style_capacity=cap;
            }
            u->styles[u->style_count++]=s;
        }
        return result(out,cap,"",TG_OK);
    }
    if(n>=4&&n<=6&&!strcmp(a[1],"lookup")) {
        unsigned yes=0,no=0; if(n>=5&&!states(a[4],&yes,&no)) goto bad;
        int rc=resolve(u,a[2],a[3],yes,out,cap);
        if(rc<0) goto bad;
        if(!rc) return result(out,cap,n==6?a[5]:"",TG_OK);
        uint32_t color; unsigned alpha;
        if(color_key(a[3])&&alpha_color(a[3],out,&color,&alpha)) {
            char b[10];
            if(alpha==255) snprintf(b,sizeof b,"#%06lx",(unsigned long)color);
            else snprintf(b,sizeof b,"#%06lx%02x",(unsigned long)color,alpha);
            return result(out,cap,b,TG_OK);
        }
        return TG_OK;
    }
    /* Theme settings are scoped to this UI, not process-global. */
    if(n==3&&!strcmp(a[1],"theme")) {
        if(!strcmp(a[2],"names")) {
            if(result(out,cap,"default clam alt classic",TG_OK)!=TG_OK) goto bad;
            for(unsigned k=0;k<u->theme_count;k++)
                if(!tg_list_append(out,cap,u->themes[k].name)) goto bad;
            return TG_OK;
        }
        if(!strcmp(a[2],"use")) return result(out,cap,theme(u),TG_OK);
    }
    if(n==4&&!strcmp(a[1],"theme")&&!strcmp(a[2],"use")) {
        if(tg_style_select_theme(u,a[3])!=TG_OK) goto bad;
        return result(out,cap,"",TG_OK);
    }
    if((n==4||n==6)&&!strcmp(a[1],"theme")&&!strcmp(a[2],"create")) {
        const char *name=a[3],*parent=n==6?a[5]:"default";
        if(n==6&&strcmp(a[4],"-parent")) goto bad;
        if(!*name||strlen(name)>=sizeof u->themes[0].name||known_theme(u,name)||
           !known_theme(u,parent)||u->theme_count>=TG_THEME_LIMIT) goto bad;
        tg_theme *next=&u->themes[u->theme_count++];
        strcpy(next->name,name); strcpy(next->parent,parent);
        return result(out,cap,"",TG_OK);
    }
bad:
    return result(out,cap,"unsupported style command or option",TG_ERROR);
}
