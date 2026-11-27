#include "../TKGPUCoreInternal.H"
#include "../TKGPUList.H"
#include "../../widgets/TKGPUWidgetInternal.H"
#include "../../widgets/canvas/TKGPUCanvasItems.H"
#include "../../widgets/text/TKGPUTextWidget.H"
#include "../../widgets/listbox/TKGPUListbox.H"
#include "../../widgets/ttk/tree/TKGPUTree.H"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
enum { BIND_LIMIT=16384, SCRIPT_LIMIT=8192 };
#define BIND_DRAG ((tg_event_kind)100)
#define BIND_DOUBLE ((tg_event_kind)101)
#define BIND_RIGHT_DOWN ((tg_event_kind)102)
#define BIND_RIGHT_UP ((tg_event_kind)103)
#define BIND_WHEEL_UP ((tg_event_kind)104)
#define BIND_WHEEL_DOWN ((tg_event_kind)105)
#define BIND_MID_DOWN ((tg_event_kind)106)
#define BIND_MID_UP ((tg_event_kind)107)
#define BIND_MID_DRAG ((tg_event_kind)108)
#define BIND_RIGHT_DRAG ((tg_event_kind)109)
#define BIND_KEY_DOWN_BASE 0x20000000
#define BIND_KEY_UP_BASE 0x40000000
static tg_event_kind key_sequence(const char *s) {
    size_t length=strlen(s); if(length<3||length>=80||s[0]!='<'||s[length-1]!='>') return 0;
    char buffer[80]; memcpy(buffer,s+1,length-2); buffer[length-2]=0; char *name=buffer; unsigned mods=0;
    for(;;) {
        if(!strncmp(name,"Control-",8)) { mods|=TG_MOD_CONTROL; name+=8; }
        else if(!strncmp(name,"Shift-",6)) { mods|=TG_MOD_SHIFT; name+=6; }
        else if(!strncmp(name,"Alt-",4)) { mods|=TG_MOD_ALT; name+=4; }
        else break;
    }
    unsigned base=BIND_KEY_DOWN_BASE;
    if(!strcmp(name,"Key")||!strcmp(name,"KeyPress")||!strcmp(name,"KeyRelease")) return (tg_event_kind)((!strcmp(name,"KeyRelease")?BIND_KEY_UP_BASE:BIND_KEY_DOWN_BASE)|mods);
    if(!strncmp(name,"KeyRelease-",11)) { base=BIND_KEY_UP_BASE; name+=11; }
    else if(!strncmp(name,"KeyPress-",9)) name+=9;
    else if(!strncmp(name,"Key-",4)) name+=4;
    unsigned key=!strcmp(name,"Left")?TG_KEY_LEFT:!strcmp(name,"Right")?TG_KEY_RIGHT:
        !strcmp(name,"Home")?TG_KEY_HOME:!strcmp(name,"End")?TG_KEY_END:
        !strcmp(name,"BackSpace")?TG_KEY_BACKSPACE:!strcmp(name,"Delete")?TG_KEY_DELETE:
        !strcmp(name,"Return")?TG_KEY_RETURN:!strcmp(name,"Escape")?TG_KEY_ESCAPE:
        !strcmp(name,"Tab")?TG_KEY_TAB:!strcmp(name,"Insert")?TG_KEY_INSERT:!strcmp(name,"Up")?TG_KEY_ARROW_UP:!strcmp(name,"Down")?TG_KEY_ARROW_DOWN:
        !strcmp(name,"Prior")?TG_KEY_PRIOR:!strcmp(name,"Next")?TG_KEY_NEXT:
        (name[0]=='F'&&name[1]>='1'&&name[1]<='9'&&!name[2])?(TG_KEY_F1+(name[1]-'1')):
        !strcmp(name,"F10")?TG_KEY_F10:!strcmp(name,"F11")?TG_KEY_F11:!strcmp(name,"F12")?TG_KEY_F12:
        !strcmp(name,"space")?32:!strcmp(name,"slash")?'/':!strcmp(name,"equal")?'=':
        !strcmp(name,"plus")?'+':!strcmp(name,"minus")?'-':!strcmp(name,"underscore")?'_':
        strlen(name)==1?(unsigned char)name[0]:0;
    if(!key) {
        uint32_t h=5381;
        for(size_t k=0;name[k];k++) h=((h<<5)+h)+(unsigned char)name[k];
        key=0x120000|(h&0xffff);
    }
    return key?(tg_event_kind)(base|mods|key):0;
}
typedef struct tg_binding {
    struct tg_binding *next;
    tg_id target;
    char *tag;
    tg_event_kind kind;
    char *script;
} binding;
typedef struct tg_canvas_binding {
    struct tg_canvas_binding *next;
    tg_id canvas_id;
    char tag_or_id[256];
    tg_event_kind kind;
    char *script;
} canvas_binding;
typedef struct tg_bindtag_entry {
    struct tg_bindtag_entry *next;
    tg_id id;
    char *tags;
} bindtag_entry;
static const char *widget_class_name(const tg_widget *w) {
    tg_kind kind=w->kind;
    if(w->themed) switch(kind) {
    case TG_FRAME: return "TFrame";
    case TG_LABEL: return "TLabel";
    case TG_BUTTON: return "TButton";
    case TG_ENTRY: return "TEntry";
    case TG_SCALE: return "TScale";
    case TG_SCROLLBAR: return "TScrollbar";
    case TG_LABELFRAME: return "TLabelframe";
    case TG_SPINBOX: return "TSpinbox";
    case TG_CHECKBUTTON: return "TCheckbutton";
    case TG_RADIOBUTTON: return "TRadiobutton";
    case TG_PANEDWINDOW: return "TPanedwindow";
    case TG_MENUBUTTON: return "TMenubutton";
    case TG_SIZEGRIP: return "TSizegrip";
    default: break;
    }
    switch(kind) {
    case TG_FRAME: return "Frame";
    case TG_LABEL: return "Label";
    case TG_BUTTON: return "Button";
    case TG_CANVAS: return "Canvas";
    case TG_ENTRY: return "Entry";
    case TG_TOPLEVEL: return "Toplevel";
    case TG_PROGRESS: return "TProgressbar";
    case TG_SEPARATOR: return "TSeparator";
    case TG_SCALE: return "Scale";
    case TG_SCROLLBAR: return "Scrollbar";
    case TG_LABELFRAME: return "Labelframe";
    case TG_COMBOBOX: return "TCombobox";
    case TG_SPINBOX: return "Spinbox";
    case TG_NOTEBOOK: return "TNotebook";
    case TG_TREEVIEW: return "Treeview";
    case TG_CHECKBUTTON: return "Checkbutton";
    case TG_RADIOBUTTON: return "Radiobutton";
    case TG_PANEDWINDOW: return "Panedwindow";
    case TG_MENUBUTTON: return "Menubutton";
    case TG_LISTBOX: return "Listbox";
    case TG_TEXT: return "Text";
    case TG_MESSAGE: return "Message";
    default: return "Tk";
    }
}
static const char *widget_toplevel_path(const tg_ui *u,const tg_widget *w) {
    const tg_widget *p=w;
    while(p&&p->id!=1&&p->kind!=TG_TOPLEVEL) p=tg_ui_get(u,p->parent);
    return (p&&p->path[0])?p->path:".";
}
static tg_event_kind sequence(const char *s) {
    if(!s||s[0]!='<') return key_sequence(s);
    size_t len=strlen(s);
    if(len>=4&&s[1]=='<'&&s[len-1]=='>'&&s[len-2]=='>') {
        uint32_t h=0x55555555;
        for(size_t k=2;k<len-2;k++) h=(h*33)^(unsigned char)s[k];
        return (tg_event_kind)(0x30000000|(h&0x000fffff));
    }
    if(len>=3&&len<80&&s[len-1]=='>') {
        char buf[80]; memcpy(buf,s+1,len-2); buf[len-2]=0;
        char *p=buf; unsigned mods=0;
        for(;;) {
            if(!strncmp(p,"Control-",8)) { mods|=TG_MOD_CONTROL; p+=8; }
            else if(!strncmp(p,"Shift-",6)) { mods|=TG_MOD_SHIFT; p+=6; }
            else if(!strncmp(p,"Alt-",4)) { mods|=TG_MOD_ALT; p+=4; }
            else break;
        }
        tg_event_kind base=0;
        if(!strcmp(p,"Button-3")||!strcmp(p,"ButtonPress-3")) base=BIND_RIGHT_DOWN;
        else if(!strcmp(p,"ButtonRelease-3")) base=BIND_RIGHT_UP;
        else if(!strcmp(p,"MouseWheel")) base=TG_WHEEL;
        else if(!strcmp(p,"Button-4")) base=BIND_WHEEL_UP;
        else if(!strcmp(p,"Button-5")) base=BIND_WHEEL_DOWN;
        else if(!strcmp(p,"Button-1")||!strcmp(p,"ButtonPress-1")||!strcmp(p,"ButtonPress")) base=TG_POINTER_DOWN;
        else if(!strcmp(p,"ButtonRelease-1")||!strcmp(p,"ButtonRelease")) base=TG_POINTER_UP;
        else if(!strcmp(p,"Button-2")||!strcmp(p,"ButtonPress-2")) base=BIND_MID_DOWN;
        else if(!strcmp(p,"ButtonRelease-2")) base=BIND_MID_UP;
        else if(!strcmp(p,"Motion")) base=TG_POINTER_MOVE;
        else if(!strcmp(p,"B1-Motion")) base=BIND_DRAG;
        else if(!strcmp(p,"B2-Motion")) base=BIND_MID_DRAG;
        else if(!strcmp(p,"B3-Motion")) base=BIND_RIGHT_DRAG;
        else if(!strcmp(p,"Double-Button-1")||!strcmp(p,"Double-ButtonPress-1")) base=BIND_DOUBLE;
        else if(!mods&&!strcmp(p,"Enter")) base=TG_ENTER;
        else if(!mods&&!strcmp(p,"Leave")) base=TG_LEAVE;
        else if(!mods&&(!strcmp(p,"Key")||!strcmp(p,"KeyPress"))) base=TG_KEY_DOWN;
        else if(!mods&&!strcmp(p,"KeyRelease")) base=TG_KEY_UP;
        else if(!mods&&!strcmp(p,"Configure")) base=TG_CONFIGURE;
        else if(!mods&&!strcmp(p,"FocusIn")) base=TG_FOCUS_IN;
        else if(!mods&&!strcmp(p,"FocusOut")) base=TG_FOCUS_OUT;
        else if(!mods&&!strcmp(p,"Destroy")) base=TG_CLOSE;
        else if(!mods&&!strcmp(p,"Map")) base=TG_MAP;
        else if(!mods&&!strcmp(p,"Unmap")) base=TG_UNMAP;
        if(base) return (tg_event_kind)((unsigned)base|mods);
    }
    return key_sequence(s);
}
static int error(tg_interp *i) { return tg_set_result(i,"unsupported bind/event option",TG_ERROR); }
static binding **find_tag(tg_interp *i,tg_id id,const char *tag,tg_event_kind k) {
    binding **p=&i->bindings;
    while(*p) {
        if((*p)->kind==k) {
            if(id>0&&(*p)->target==id&&!(*p)->tag) return p;
            if(!id&&!tag&&!(*p)->target&&!(*p)->tag) return p;
            if(!id&&tag&&(*p)->tag&&!strcmp((*p)->tag,tag)) return p;
        }
        p=&(*p)->next;
    }
    return p;
}
static binding **find(tg_interp *i,tg_id id,tg_event_kind k) {
    return find_tag(i,id,NULL,k);
}
static void remove_binding(binding **p) {
    binding *b=*p; *p=b->next; free(b->script); free(b->tag); free(b);
}
static canvas_binding **find_canvas_binding(tg_interp *i,tg_id cid,const char *tag_or_id,tg_event_kind k) {
    canvas_binding **p=&i->canvas_bindings;
    while(*p) {
        if((*p)->canvas_id==cid&&(*p)->kind==k&&!strcmp((*p)->tag_or_id,tag_or_id)) return p;
        p=&(*p)->next;
    }
    return p;
}
static int canvas_item_matches_tag(const tg_canvas_item *item,const char *tag_or_id) {
    if(!strcmp(tag_or_id,"all")) return 1;
    char *end=NULL;
    unsigned long id=strtoul(tag_or_id,&end,10);
    if(tag_or_id[0]>='0'&&tag_or_id[0]<='9'&&end&&!*end) return (uint32_t)id==item->id;
    const char *cursor=item->tags; char word[256]; int rc;
    while((rc=tg_list_next(&cursor,word,sizeof word))>0) {
        if(!strcmp(word,tag_or_id)) return 1;
    }
    return 0;
}
void tg_binding_clear(tg_interp *i) {
    while(i->bindings) remove_binding(&i->bindings);
    while(i->canvas_bindings) {
        canvas_binding *cb=i->canvas_bindings; i->canvas_bindings=cb->next; free(cb->script); free(cb);
    }
    while(i->bindtags) {
        bindtag_entry *e=i->bindtags; i->bindtags=e->next; free(e->tags); free(e);
    }
}
void tg_binding_prune(tg_interp *i) {
    if(i->binding_capture&&!tg_ui_get(i->ui,i->binding_capture)) i->binding_capture=0;
    if(i->last_click_target&&!tg_ui_get(i->ui,i->last_click_target)) i->last_click_target=0;
    binding **p=&i->bindings;
    while(*p) {
        if((*p)->target&&!tg_ui_get(i->ui,(*p)->target)) remove_binding(p);
        else p=&(*p)->next;
    }
    canvas_binding **cb=&i->canvas_bindings;
    while(*cb) {
        if(!tg_ui_get(i->ui,(*cb)->canvas_id)) {
            canvas_binding *dead=*cb; *cb=dead->next; free(dead->script); free(dead);
        } else cb=&(*cb)->next;
    }
    bindtag_entry **b=&i->bindtags;
    while(*b) {
        if(!tg_ui_get(i->ui,(*b)->id)) {
            bindtag_entry *e=*b; *b=e->next; free(e->tags); free(e);
        } else b=&(*b)->next;
    }
}
static int number(const char *s,int *value) {
    char *end; long v=strtol(s,&end,10);
    if(!*s||*end||v< -32768||v>32767) return 0;
    *value=(int)v; return 1;
}
int tg_binding_command(tg_interp *i,size_t n,char **a) {
    if(!strcmp(a[0],"event")) {
        if(n<4||n%2||strcmp(a[1],"generate")) return error(i);
        tg_id id=tg_ui_find(i->ui,a[2]); tg_event_kind k=sequence(a[3]);
        if(!id||!k||k==BIND_DOUBLE) return error(i);
        tg_event event={k,id,0,0,0};
        if(!TG_IS_VIRTUAL(k)&&(((unsigned)k&BIND_KEY_DOWN_BASE)||((unsigned)k&BIND_KEY_UP_BASE))) {
            unsigned base=((unsigned)k&BIND_KEY_UP_BASE)?BIND_KEY_UP_BASE:BIND_KEY_DOWN_BASE;
            event.kind=base==BIND_KEY_UP_BASE?TG_KEY_UP:TG_KEY_DOWN; event.key=(unsigned)k&~base;
        } else if(!TG_IS_VIRTUAL(k)) {
            unsigned mods=(unsigned)k&(TG_MOD_CONTROL|TG_MOD_SHIFT|TG_MOD_ALT);
            tg_event_kind raw=(tg_event_kind)((unsigned)k&~mods);
            event.kind=raw; event.key=mods;
            if(raw==BIND_RIGHT_DOWN||raw==BIND_RIGHT_UP) { event.kind=raw==BIND_RIGHT_DOWN?TG_POINTER_DOWN:TG_POINTER_UP; event.key=3u|mods; }
            else if(raw==BIND_MID_DOWN||raw==BIND_MID_UP) { event.kind=raw==BIND_MID_DOWN?TG_POINTER_DOWN:TG_POINTER_UP; event.key=2u|mods; }
            else if(raw==BIND_DRAG||raw==BIND_MID_DRAG||raw==BIND_RIGHT_DRAG) { event.kind=TG_POINTER_MOVE; event.key=mods; }
            else if(raw==BIND_WHEEL_UP||raw==BIND_WHEEL_DOWN) { event.kind=TG_WHEEL; event.key=((uint32_t)(uint16_t)(int16_t)(raw==BIND_WHEEL_UP?120:-120))|mods; }
        }
        for(size_t j=4;j<n;j+=2) {
            int v;
            if(!strcmp(a[j],"-when")&&!strcmp(a[j+1],"tail")) continue;
            if(!number(a[j+1],&v)) return error(i);
            if(!strcmp(a[j],"-x")||(!strcmp(a[j],"-width")&&k==TG_CONFIGURE)) event.x=v;
            else if(!strcmp(a[j],"-y")||(!strcmp(a[j],"-height")&&k==TG_CONFIGURE)) event.y=v;
            else if(!strcmp(a[j],"-delta")&&event.kind==TG_WHEEL) event.key=(uint32_t)(uint16_t)(int16_t)v;
            else if(!strcmp(a[j],"-keycode")&&v>=0) event.key=(uint32_t)v;
            else return error(i);
        }
        /* event generate coordinates are widget-local, whereas the native
           queue stores coordinates relative to its owning toplevel. */
        if(event.kind==TG_POINTER_DOWN||event.kind==TG_POINTER_UP||
           event.kind==TG_POINTER_MOVE||event.kind==TG_ENTER||
           event.kind==TG_LEAVE||event.kind==TG_WHEEL) {
            const tg_widget *widget=tg_ui_get(i->ui,id);
            if(widget) { event.x+=widget->rect.x; event.y+=widget->rect.y; }
        }
        return tg_ui_post(i->ui,event)==TG_OK?tg_set_result(i,"",TG_OK):error(i);
    }
    if(!strcmp(a[0],"bindtags")) {
        if(n<2) return error(i);
        tg_id id=tg_ui_find(i->ui,a[1]);
        if(!id) return error(i);
        const tg_widget *w=tg_ui_get(i->ui,id);
        if(!w) return error(i);
        if(n==2) {
            for(bindtag_entry *e=i->bindtags;e;e=e->next) {
                if(e->id==id) return tg_set_result(i,e->tags,TG_OK);
            }
            char b[2*TG_WIDGET_PATH_CAP+64];
            if(w->id==1||w->kind==TG_TOPLEVEL)
                snprintf(b,sizeof b,"%s %s all",w->path,w->id==1?"Tk":widget_class_name(w));
            else snprintf(b,sizeof b,"%s %s %s all",w->path,widget_class_name(w),widget_toplevel_path(i->ui,w));
            return tg_set_result(i,b,TG_OK);
        }
        char buffer[MAX_WORD]; size_t used=0; buffer[0]=0;
        for(size_t k=2;k<n;k++) {
            size_t len=strlen(a[k]);
            if(used+len+2>=sizeof buffer) return error(i);
            if(used) buffer[used++]=' ';
            memcpy(buffer+used,a[k],len+1);
            used+=len;
        }
        bindtag_entry *found=NULL;
        for(bindtag_entry *e=i->bindtags;e;e=e->next) {
            if(e->id==id) { found=e; break; }
        }
        if(!found) {
            found=calloc(1,sizeof *found);
            if(!found) return error(i);
            found->id=id;
            found->next=i->bindtags;
            i->bindtags=found;
        } else {
            free(found->tags);
        }
        found->tags=tg_copy_string(buffer);
        return tg_set_result(i,"",TG_OK);
    }
    if(n>=2&&a[0][0]=='.'&&!strcmp(a[1],"bind")) {
        tg_id cid=tg_ui_find(i->ui,a[0]);
        const tg_widget *cw=tg_ui_get(i->ui,cid);
        if(!cw||cw->kind!=TG_CANVAS) return -1;
        if(n<4) return tg_set_result(i,"",TG_OK);
        if(strlen(a[2])>=256) return error(i);
        tg_event_kind k=sequence(a[3]); if(!k) return tg_set_result(i,"",TG_OK);
        canvas_binding **p=find_canvas_binding(i,cid,a[2],k);
        if(n==4) return tg_set_result(i,*p?(*p)->script:"",TG_OK);
        if(!*a[4]) {
            if(*p) { canvas_binding *dead=*p; *p=dead->next; free(dead->script); free(dead); }
            return tg_set_result(i,"",TG_OK);
        }
        int append=a[4][0]=='+';
        const char *s=a[4]+append;
        size_t old=append&&*p?strlen((*p)->script):0,len=strlen(s);
        if(len>=SCRIPT_LIMIT||old+len+1>=SCRIPT_LIMIT) return error(i);
        char *script=malloc(old+len+2); if(!script) return error(i);
        if(old) { memcpy(script,(*p)->script,old); script[old++]='\n'; }
        memcpy(script+old,s,len+1);
        if(!*p) {
            if(!(*p=calloc(1,sizeof **p))) { free(script); return error(i); }
            (*p)->canvas_id=cid;
            strcpy((*p)->tag_or_id,a[2]);
            (*p)->kind=k;
        }
        free((*p)->script); (*p)->script=script;
        return tg_set_result(i,"",TG_OK);
    }
    if(strcmp(a[0],"bind")) return -1;
    if(n<3||n>4) return error(i);
    tg_id id=tg_ui_find(i->ui,a[1]);
    const char *tag=NULL;
    if(!id&&strcmp(a[1],"all")) tag=a[1];
    tg_event_kind k=sequence(a[2]); if(!k) return error(i);
    binding **p=find_tag(i,id,tag,k);
    if(n==3) return tg_set_result(i,*p?(*p)->script:"",TG_OK);
    if(!*a[3]) { if(*p) remove_binding(p); return tg_set_result(i,"",TG_OK); }
    int append=a[3][0]=='+';
    const char *s=a[3]+append;
    size_t old=append&&*p?strlen((*p)->script):0,len=strlen(s);
    if(len>=SCRIPT_LIMIT||old+len+1>=SCRIPT_LIMIT) return error(i);
    char *script=malloc(old+len+2); if(!script) return error(i);
    if(old) { memcpy(script,(*p)->script,old); script[old++]='\n'; }
    memcpy(script+old,s,len+1);
    if(!*p) {
        size_t count=0; for(binding *b=i->bindings;b;b=b->next) count++;
        if(count>=BIND_LIMIT||!(*p=calloc(1,sizeof **p))) { free(script); return error(i); }
        (*p)->target=id;
        (*p)->tag=tag?tg_copy_string(tag):NULL;
        (*p)->kind=k;
    }
    free((*p)->script); (*p)->script=script;
    return tg_set_result(i,"",TG_OK);
}
static int append_text(char *out,size_t *n,const char *text) {
    size_t len=strlen(text);
    if(len>=MAX_WORD-*n) return 0;
    memcpy(out+*n,text,len+1); *n+=len; return 1;
}
/* Preview substitutions deliberately restrict replacements to safe numeric
   tokens or validated widget paths; unsupported placeholders fail closed. */
static int expand(tg_interp *i,const char *script,tg_event e,char *out) {
    size_t n=0; out[0]=0;
    const tg_widget *w=tg_ui_get(i->ui,e.target);
    for(const char *p=script;*p;p++) {
        char b[TG_WIDGET_PATH_CAP]; b[0]=*p; b[1]=0;
        if(*p=='%') {
            switch(*++p) {
            case '%': b[0]='%'; break;
            case 'x': case 'y':
                if(e.kind==TG_CONFIGURE||e.kind==TG_FOCUS_IN||e.kind==TG_FOCUS_OUT) strcpy(b,"??");
                else snprintf(b,sizeof b,"%d",*p=='x'?e.x-(w?w->rect.x:0):e.y-(w?w->rect.y:0));
                break;
            case 'w': case 'h':
                if(e.kind==TG_CONFIGURE) snprintf(b,sizeof b,"%d",*p=='w'?e.x:e.y);
                else strcpy(b,"??");
                break;
            case 'k': {
                if(e.kind==TG_KEY_DOWN||e.kind==TG_KEY_UP) {
                    uint32_t kv=e.key&TG_KEY_VALUE_MASK;
                    unsigned long vk=kv;
                    if(kv>='a'&&kv<='z') vk=(unsigned long)(kv-'a'+'A');
                    else if(kv>='A'&&kv<='Z') vk=(unsigned long)kv;
                    else if(kv>='0'&&kv<='9') vk=(unsigned long)kv;
                    else if(kv=='/') vk=191;
                    else if(kv=='='||kv=='+') vk=187;
                    else if(kv=='-') vk=189;
                    else if(kv==' ') vk=32;
                    else if(kv==TG_KEY_BACKSPACE) vk=8;
                    else if(kv==TG_KEY_TAB) vk=9;
                    else if(kv==TG_KEY_RETURN) vk=13;
                    else if(kv==TG_KEY_ESCAPE) vk=27;
                    else if(kv==TG_KEY_PRIOR) vk=33;
                    else if(kv==TG_KEY_NEXT) vk=34;
                    else if(kv==TG_KEY_END) vk=35;
                    else if(kv==TG_KEY_HOME) vk=36;
                    else if(kv==TG_KEY_LEFT) vk=37;
                    else if(kv==TG_KEY_ARROW_UP) vk=38;
                    else if(kv==TG_KEY_RIGHT) vk=39;
                    else if(kv==TG_KEY_ARROW_DOWN) vk=40;
                    else if(kv==TG_KEY_INSERT) vk=45;
                    else if(kv==TG_KEY_DELETE) vk=46;
                    else if(kv>=TG_KEY_F1&&kv<=TG_KEY_F12) vk=112+(kv-TG_KEY_F1);
                    snprintf(b,sizeof b,"%lu",vk);
                } else {
                    snprintf(b,sizeof b,"%lu",(unsigned long)e.key);
                }
                break;
            }
            case 'b':
                if(e.kind==TG_POINTER_DOWN||e.kind==TG_POINTER_UP) snprintf(b,sizeof b,"%u",e.key?e.key&255:1); else strcpy(b,"??");
                break;
            case 'f': if(e.kind==TG_FOCUS_IN||e.kind==TG_FOCUS_OUT) strcpy(b,e.kind==TG_FOCUS_IN?"1":"0"); else strcpy(b,"??"); break;
            case 'E': strcpy(b,"0"); break;
            case 'T': {
                int type=e.kind==TG_WHEEL?38:TG_IS_VIRTUAL(e.kind)?35:e.kind==TG_POINTER_DOWN?4:e.kind==TG_POINTER_UP?5:e.kind==TG_POINTER_MOVE?6:
                    e.kind==TG_KEY_DOWN?2:e.kind==TG_KEY_UP?3:e.kind==TG_CONFIGURE?22:e.kind==TG_MAP?19:e.kind==TG_UNMAP?18:
                    e.kind==TG_FOCUS_IN?9:e.kind==TG_FOCUS_OUT?10:e.kind==TG_ENTER?7:e.kind==TG_LEAVE?8:0;
                snprintf(b,sizeof b,"%d",type); break;
            }
            case 'A': strcpy(b,"{}"); break;
            case 'K': {
                uint32_t saved=e.key; e.key&=TG_KEY_VALUE_MASK;
                const char *name=e.key==TG_KEY_LEFT?"Left":e.key==TG_KEY_RIGHT?"Right":
                    e.key==TG_KEY_HOME?"Home":e.key==TG_KEY_END?"End":e.key==TG_KEY_BACKSPACE?"BackSpace":
                    e.key==TG_KEY_DELETE?"Delete":e.key==TG_KEY_RETURN?"Return":
                    e.key==TG_KEY_ESCAPE?"Escape":e.key==TG_KEY_TAB?"Tab":e.key==TG_KEY_INSERT?"Insert":
                    e.key==TG_KEY_PRIOR?"Prior":e.key==TG_KEY_NEXT?"Next":
                    e.key==TG_KEY_ARROW_UP?"Up":e.key==TG_KEY_ARROW_DOWN?"Down":
                    e.key==' '?"space":e.key=='/'?"slash":e.key=='='?"equal":
                    e.key=='+'?"plus":e.key=='-'?"minus":"??";
                if(!strcmp(name,"??")&&e.key>=TG_KEY_F1&&e.key<=TG_KEY_F12) {
                    snprintf(b,sizeof b,"F%u",(unsigned)(e.key-TG_KEY_F1+1));
                } else if(!strcmp(name,"??")&&e.key>=32&&e.key<127) { b[0]=(char)e.key; b[1]=0; }
                else strcpy(b,name);
                e.key=saved; break;
            }
            case 'X': case 'Y': {
                int ox=0,oy=0;
                if(i->tk_origin) (void)i->tk_origin(i->tk_host,e.target,&ox,&oy);
                snprintf(b,sizeof b,"%d",*p=='X'?e.x+ox:e.y+oy); break;
            }
            case '#': strcpy(b,"0"); break; /* Synthetic events have no native serial. */
            case 's': snprintf(b,sizeof b,"%u",((e.key&TG_MOD_SHIFT)?1u:0u)|((e.key&TG_MOD_CONTROL)?4u:0u)|((e.key&TG_MOD_ALT)?8u:0u)); break;
            case 'D': snprintf(b,sizeof b,"%d",e.kind==TG_WHEEL?(int)(int16_t)(e.key&0xffffu):0); break;
            case 't': {
                static uint64_t last_event_time=0;
                uint64_t now=0;
                if(i->tk_clock&&i->tk_clock(&now)) {
                    if(now<=last_event_time) now=last_event_time+1;
                    last_event_time=now;
                } else {
                    now=++last_event_time;
                }
                snprintf(b,sizeof b,"%lu",(unsigned long)(now&0xffffffffu));
                break;
            }
            case 'N': strcpy(b,"0"); break;
            case 'W':
                if(!w||strspn(w->path,"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-!")!=strlen(w->path)) return 0;
                snprintf(b,sizeof b,"%s",w->path); break;
            default: return 0;
            }
        }
        if(!append_text(out,&n,b)) return 0;
    }
    return 1;
}
static int point_near_segment(double px,double py,double x0,double y0,double x1,double y1,double tol2) {
    double dx=x1-x0,dy=y1-y0,len2=dx*dx+dy*dy;
    double t=len2>1e-9?((px-x0)*dx+(py-y0)*dy)/len2:0.0;
    if(t<0.0) t=0.0; else if(t>1.0) t=1.0;
    double qx=x0+t*dx-px,qy=y0+t*dy-py;
    return qx*qx+qy*qy<=tol2;
}
static int canvas_item_hit(const tg_ui *u,const tg_canvas_item *it,int cx,int cy) {
    tg_rect q;
    if(!tg_canvas_item_bounds(u,it,&q)) return 0;
    int pad=(it->kind==TG_ITEM_LINE||it->is_arc)?((int)(it->width/2)+4):0;
    if(cx<q.x-pad||cx>q.x+q.w+pad||cy<q.y-pad||cy>q.y+q.h+pad) return 0;
    if(it->kind==TG_ITEM_LINE&&it->point_count>=2) {
        double tol=(double)it->width*0.5+4.0,tol2=tol*tol;
        if(it->smooth&&it->point_count>=3) {
            unsigned n=it->point_count;
            double sx=it->points[0],sy=it->points[1];
            for(unsigned k=1;k+1<n;k++) {
                double bx=it->points[k*2],by=it->points[k*2+1];
                double ex=0.5*(bx+it->points[(k+1)*2]),ey=0.5*(by+it->points[(k+1)*2+1]);
                double px=sx,py=sy;
                for(int step=1;step<=8;step++) {
                    double t=step/8.0,mt=1.0-t;
                    double nx=mt*mt*sx+2.0*mt*t*bx+t*t*ex;
                    double ny=mt*mt*sy+2.0*mt*t*by+t*t*ey;
                    if(point_near_segment(cx,cy,px,py,nx,ny,tol2)) return 1;
                    px=nx; py=ny;
                }
                sx=ex; sy=ey;
            }
            return point_near_segment(cx,cy,sx,sy,it->points[(n-1)*2],it->points[(n-1)*2+1],tol2);
        }
        for(unsigned k=0;k+1<it->point_count;k++) {
            if(point_near_segment(cx,cy,it->points[k*2],it->points[k*2+1],
                                  it->points[(k+1)*2],it->points[(k+1)*2+1],tol2)) return 1;
        }
        return 0;
    }
    if((it->kind==TG_ITEM_RECT||it->kind==TG_ITEM_OVAL)&&!it->has_fill&&it->has_outline) {
        int bw=(int)(it->width/2)+3;
        if(cx>q.x+bw&&cx<q.x+q.w-bw&&cy>q.y+bw&&cy<q.y+q.h-bw) return 0;
    }
    return 1;
}
int tg_binding_dispatch(tg_interp *i,tg_event e) {
    /* Capture a button-bound widget for its drag and release callbacks. */
    tg_event_kind kind=e.kind;
    unsigned btn=e.key&255u;
    unsigned ev_mods=e.key&(TG_MOD_CONTROL|TG_MOD_SHIFT|TG_MOD_ALT);
    if(e.kind==TG_POINTER_DOWN) {
        if(btn==2) kind=BIND_MID_DOWN;
        else if(btn==3) kind=BIND_RIGHT_DOWN;
    } else if(e.kind==TG_POINTER_UP) {
        if(btn==2) kind=BIND_MID_UP;
        else if(btn==3) kind=BIND_RIGHT_UP;
    }
    /* Native wheel and pointer events include active modifier bits first,
       falling back to unmodified bindings if no modified binding exists. */
    if(e.kind==TG_KEY_DOWN||e.kind==TG_KEY_UP) kind=(tg_event_kind)((e.kind==TG_KEY_DOWN?BIND_KEY_DOWN_BASE:BIND_KEY_UP_BASE)|e.key);
    if(e.kind==TG_POINTER_DOWN) {
        i->binding_capture=e.target;
        i->binding_capture_button=btn?btn:1u;
        if(btn!=2&&btn!=3&&!ev_mods) {
            uint64_t now=0;
            if(i->tk_clock&&i->tk_clock(&now)) {
                int dx=e.x-i->last_click_x,dy=e.y-i->last_click_y;
                if(e.target&&e.target==i->last_click_target&&now>=i->last_click_ms&&
                   now-i->last_click_ms<=500&&dx>=-5&&dx<=5&&dy>=-5&&dy<=5&&
                   (*find(i,e.target,BIND_DOUBLE)||*find(i,0,BIND_DOUBLE))) {
                    kind=BIND_DOUBLE; i->last_click_target=0;
                } else {
                    i->last_click_target=e.target; i->last_click_ms=now;
                    i->last_click_x=e.x; i->last_click_y=e.y;
                }
            }
        }
    } else if(e.kind==TG_POINTER_MOVE&&i->binding_capture) {
        /* Implicit Tk grab: resolve all bindtags on the pressed widget,
           even when the pointer has left it or its geometry has moved. */
        e.target=i->binding_capture;
        kind=i->binding_capture_button==2?BIND_MID_DRAG:i->binding_capture_button==3?BIND_RIGHT_DRAG:BIND_DRAG;
    } else if(e.kind==TG_POINTER_UP) {
        tg_id captured=i->binding_capture;
        if(captured&&(!i->binding_capture_button||btn==i->binding_capture_button)) {
            i->binding_capture=0; i->binding_capture_button=0;
            e.target=captured;
        }
    } else if(e.kind==TG_FOCUS_OUT) {
        /* Internal widget focus changes are not loss of the native grab. */
        const tg_widget *focus=tg_ui_get(i->ui,e.target);
        if(e.target==1||(focus&&focus->kind==TG_TOPLEVEL)) { i->binding_capture=0; i->binding_capture_button=0; }
    }
    tg_event_kind base_kind=kind;
    if(ev_mods&&(e.kind==TG_POINTER_DOWN||e.kind==TG_POINTER_UP||e.kind==TG_POINTER_MOVE||e.kind==TG_WHEEL)) {
        kind=(tg_event_kind)((unsigned)kind|ev_mods);
    }
    /* Tk bindtags: iterate through the tags of e.target.
       Snapshot scripts before invoking callbacks that can destroy or rebind widgets. */
    char default_tags[2*TG_WIDGET_PATH_CAP+64];
    const char *tag_list=NULL;
    const tg_widget *w=tg_ui_get(i->ui,e.target);
    if(w) {
        for(bindtag_entry *e_bt=i->bindtags;e_bt;e_bt=e_bt->next) {
            if(e_bt->id==e.target) { tag_list=e_bt->tags; break; }
        }
        if(!tag_list) {
            if(w->id==1||w->kind==TG_TOPLEVEL)
                snprintf(default_tags,sizeof default_tags,"%s %s all",w->path,w->id==1?"Tk":widget_class_name(w));
            else snprintf(default_tags,sizeof default_tags,"%s %s %s all",
                          w->path,widget_class_name(w),widget_toplevel_path(i->ui,w));
            tag_list=default_tags;
        }
    } else {
        tag_list="all";
    }
    /* Update canvas 'current' item and dispatch canvas item/tag bindings (canvas.tag_bind)
       only on the topmost hit item (matching Tk semantics so wires underneath components
       or wire bounding boxes never steal clicks from components). */
    if(w&&w->kind==TG_CANVAS&&i->ui->drawing&&
       (e.kind==TG_POINTER_DOWN||e.kind==TG_POINTER_UP||e.kind==TG_POINTER_MOVE)) {
        int cx=(e.x-w->rect.x)+w->canvas_x;
        int cy=(e.y-w->rect.y)+w->canvas_y;
        const tg_canvas_item *top_item=NULL;
        for(size_t k=i->ui->drawing->count;k>0;k--) {
            const tg_canvas_item *it=&i->ui->drawing->item[k-1];
            if(it->owner!=w->id||it->hidden||it->kind==TG_ITEM_WINDOW) continue;
            if(canvas_item_hit(i->ui,it,cx,cy)) { top_item=it; break; }
        }
        i->ui->drawing->current_id=top_item?top_item->id:0;
        const tg_canvas_item *hit_item=NULL;
        if(top_item&&i->canvas_bindings) {
            for(canvas_binding *cb=i->canvas_bindings;cb;cb=cb->next) {
                if(cb->canvas_id==w->id&&(cb->kind==kind||cb->kind==e.kind)&&canvas_item_matches_tag(top_item,cb->tag_or_id)) {
                    hit_item=top_item; break;
                }
            }
        }
        if(hit_item) {
            char item_scripts[8][SCRIPT_LIMIT]={{0}};
            size_t item_sc=0;
            for(canvas_binding *cb=i->canvas_bindings;cb&&item_sc<8;cb=cb->next) {
                if(cb->canvas_id==w->id&&(cb->kind==kind||cb->kind==e.kind)&&canvas_item_matches_tag(hit_item,cb->tag_or_id)) {
                    strcpy(item_scripts[item_sc++],cb->script);
                }
            }
            for(size_t k=0;k<item_sc;k++) if(item_scripts[k][0]) {
                char expanded[MAX_WORD];
                if(!strcmp(item_scripts[k],"break")) return 1;
                if(!expand(i,item_scripts[k],e,expanded)) continue;
                const char *cursor=expanded;
                int rc=tg_eval_script(i,&cursor,0);
                if(rc==TG_BREAK) return 1;
                if(rc!=TG_OK) {
                    char *message=tg_copy_string(i->result);
                    if(message) { (void)tg_var_store(i,"errorInfo",message); free(message); }
                }
            }
        }
    }
    /* If the hovered widget itself is a scrollable Treeview/Listbox/Text without an explicit
       per-widget wheel binding, scroll it directly (like Tkinter's class binding) before
       any global 'all' binding steals the wheel event. */
    if(e.kind==TG_WHEEL&&(int16_t)(e.key&0xffffu)) {
        const tg_widget *self_w=tg_ui_get(i->ui,e.target);
        if(self_w&&(self_w->kind==TG_TREEVIEW||self_w->kind==TG_LISTBOX||self_w->kind==TG_TEXT)) {
            int has_self=(*find_tag(i,self_w->id,NULL,kind)!=NULL)||
                         (kind!=e.kind&&*find_tag(i,self_w->id,NULL,e.kind)!=NULL)||
                         (*find_tag(i,self_w->id,NULL,(int16_t)(e.key&0xffffu)>0?BIND_WHEEL_UP:BIND_WHEEL_DOWN)!=NULL);
            if(!has_self) {
                int delta=(int)(int16_t)(e.key&0xffffu),units=-(delta/120);
                if(!units) units=delta>0?-1:1;
                units*=3;
                char path[TG_WIDGET_PATH_CAP],amount[32],result[64];
                strcpy(path,self_w->path); snprintf(amount,sizeof amount,"%d",units);
                char *args[]={path,"yview","scroll",amount,"units"};
                if(tg_ui_command(i->ui,5,args,result,sizeof result)==TG_OK) {
                    (void)tg_interp_dispatch_scroll(i);
                    if(i->tk_repaint) i->tk_repaint(i->tk_host,self_w->id);
                    else if(i->tk_invalidate) i->tk_invalidate(i->tk_host);
                    return 1;
                }
            }
        }
    }
    char scripts[16][SCRIPT_LIMIT]={{0}};
    size_t script_count=0;
    const char *tp=tag_list;
    while(*tp&&script_count<16) {
        while(*tp==' ') tp++;
        if(!*tp) break;
        char tag_buf[TG_WIDGET_PATH_CAP]; size_t tlen=0;
        if(*tp=='{') {
            tp++;
            while(*tp&&*tp!='}'&&tlen+1<sizeof tag_buf) tag_buf[tlen++]=*tp++;
            if(*tp=='}') tp++;
        } else {
            while(*tp&&*tp!=' '&&tlen+1<sizeof tag_buf) tag_buf[tlen++]=*tp++;
        }
        tag_buf[tlen]=0;
        if(!tlen) continue;
        binding **p=NULL;
        tg_event_kind key_mod_kind=(tg_event_kind)((e.kind==TG_KEY_DOWN?BIND_KEY_DOWN_BASE:BIND_KEY_UP_BASE)|(e.key&~TG_KEY_VALUE_MASK));
        if(!strcmp(tag_buf,"all")) {
            p=find_tag(i,0,NULL,kind);
            if(!*p&&(e.kind==TG_KEY_DOWN||e.kind==TG_KEY_UP)) p=find_tag(i,0,NULL,key_mod_kind);
            if(!*p&&(e.kind==TG_KEY_DOWN||e.kind==TG_KEY_UP)) p=find_tag(i,0,NULL,e.kind);
            if(!*p&&kind!=base_kind) p=find_tag(i,0,NULL,base_kind);
            if(!*p&&e.kind==TG_WHEEL) p=find_tag(i,0,NULL,(int16_t)(e.key&0xffffu)>0?BIND_WHEEL_UP:BIND_WHEEL_DOWN);
        } else {
            tg_id tid=tg_ui_find(i->ui,tag_buf);
            if(tid>0) {
                p=find_tag(i,tid,NULL,kind);
                if(!*p&&(e.kind==TG_KEY_DOWN||e.kind==TG_KEY_UP)) p=find_tag(i,tid,NULL,key_mod_kind);
                if(!*p&&(e.kind==TG_KEY_DOWN||e.kind==TG_KEY_UP)) p=find_tag(i,tid,NULL,e.kind);
                if(!*p&&kind!=base_kind) p=find_tag(i,tid,NULL,base_kind);
                if(!*p&&e.kind==TG_WHEEL) p=find_tag(i,tid,NULL,(int16_t)(e.key&0xffffu)>0?BIND_WHEEL_UP:BIND_WHEEL_DOWN);
            }
            if(!p||!*p) {
                p=find_tag(i,0,tag_buf,kind);
                if(!*p&&(e.kind==TG_KEY_DOWN||e.kind==TG_KEY_UP)) p=find_tag(i,0,tag_buf,key_mod_kind);
                if(!*p&&(e.kind==TG_KEY_DOWN||e.kind==TG_KEY_UP)) p=find_tag(i,0,tag_buf,e.kind);
                if(!*p&&kind!=base_kind) p=find_tag(i,0,tag_buf,base_kind);
                if(!*p&&e.kind==TG_WHEEL) p=find_tag(i,0,tag_buf,(int16_t)(e.key&0xffffu)>0?BIND_WHEEL_UP:BIND_WHEEL_DOWN);
            }
        }
        if(p&&*p) {
            strcpy(scripts[script_count++],(*p)->script);
        }
    }
    /* If the hovered widget itself is a scrollable Treeview/Listbox/Text without an explicit binding,
       scroll it directly before falling back to outer page canvas bindings. */
    if(!script_count&&e.kind==TG_WHEEL&&(int16_t)(e.key&0xffffu)) {
        const tg_widget *self_w=tg_ui_get(i->ui,e.target);
        if(self_w&&(self_w->kind==TG_TREEVIEW||self_w->kind==TG_LISTBOX||self_w->kind==TG_TEXT)) {
            int delta=(int)(int16_t)(e.key&0xffffu),units=-(delta/120);
            if(!units) units=delta>0?-1:1;
            if(self_w->kind==TG_TREEVIEW) units*=3;
            char path[TG_WIDGET_PATH_CAP],amount[32],result[64];
            strcpy(path,self_w->path); snprintf(amount,sizeof amount,"%d",units);
            char *args[]={path,"yview","scroll",amount,"units"};
            if(tg_ui_command(i->ui,5,args,result,sizeof result)==TG_OK) {
                (void)tg_interp_dispatch_scroll(i);
                if(i->tk_invalidate) i->tk_invalidate(i->tk_host);
                return 1;
            }
        }
    }
    /* Only supply native scrolling when no explicit binding exists.
       Never scroll twice when application code already handles the wheel. */
    if(!script_count&&e.kind==TG_WHEEL&&(int16_t)(e.key&0xffffu)) {
        int delta=(int)(int16_t)(e.key&0xffffu);
        const tg_widget *anc=tg_ui_get(i->ui,e.target);
        if(anc) anc=tg_ui_get(i->ui,anc->parent);
        while(anc&&anc->kind!=TG_TOPLEVEL&&!script_count) {
            binding **bp=find_tag(i,anc->id,NULL,kind);
            if(!*bp&&kind!=e.kind) bp=find_tag(i,anc->id,NULL,e.kind);
            if(!*bp) bp=find_tag(i,anc->id,NULL,delta>0?BIND_WHEEL_UP:BIND_WHEEL_DOWN);
            if(*bp) {
                strcpy(scripts[script_count++],(*bp)->script);
                break;
            }
            /* Also check custom bindtags attached to ancestor containers (e.g. Budget/Settings scroll canvases). */
            for(bindtag_entry *e_bt=i->bindtags;e_bt;e_bt=e_bt->next) {
                if(e_bt->id!=anc->id) continue;
                const char *atp=e_bt->tags;
                while(*atp&&!script_count) {
                    while(*atp==' ') atp++;
                    if(!*atp) break;
                    char abuf[TG_WIDGET_PATH_CAP]; size_t alen=0;
                    if(*atp=='{') { atp++; while(*atp&&*atp!='}'&&alen+1<sizeof abuf) abuf[alen++]=*atp++; if(*atp=='}') atp++; }
                    else { while(*atp&&*atp!=' '&&alen+1<sizeof abuf) abuf[alen++]=*atp++; }
                    abuf[alen]=0;
                    if(!alen||!strcmp(abuf,"all")||abuf[0]=='.') continue;
                    binding **cbp=find_tag(i,0,abuf,kind);
                    if(!*cbp&&kind!=e.kind) cbp=find_tag(i,0,abuf,e.kind);
                    if(!*cbp) cbp=find_tag(i,0,abuf,delta>0?BIND_WHEEL_UP:BIND_WHEEL_DOWN);
                    if(*cbp) { strcpy(scripts[script_count++],(*cbp)->script); break; }
                }
                break;
            }
            anc=tg_ui_get(i->ui,anc->parent);
        }
    }
    if(!script_count&&e.kind==TG_WHEEL&&(int16_t)(e.key&0xffffu)) {
        const tg_widget *canvas=tg_ui_get(i->ui,e.target);
        while(canvas) {
            if(canvas->kind==TG_TEXT||canvas->kind==TG_LISTBOX||canvas->kind==TG_TREEVIEW||
               (canvas->kind==TG_CANVAS&&canvas->canvas_region_set&&canvas->canvas_region.h>canvas->rect.h)) {
                int delta=(int)(int16_t)(e.key&0xffffu),units=-(delta/120);
                if(!units) units=delta>0?-1:1;
                char path[TG_WIDGET_PATH_CAP],amount[32],result[64];
                strcpy(path,canvas->path); snprintf(amount,sizeof amount,"%d",units);
                char *args[]={path,"yview","scroll",amount,"units"};
                if(tg_ui_command(i->ui,5,args,result,sizeof result)==TG_OK) {
                    (void)tg_interp_dispatch_scroll(i);
                    if(i->tk_invalidate) i->tk_invalidate(i->tk_host);
                    return 1;
                }
                break;
            }
            if(canvas->kind==TG_TOPLEVEL) break;
            canvas=tg_ui_get(i->ui,canvas->parent);
        }
    }
    const tg_widget *wheel_target=NULL;
    double wheel_prev_first=-1.0,wheel_prev_last=-1.0;
    if(e.kind==TG_WHEEL&&(int16_t)(e.key&0xffffu)&&!(ev_mods&TG_MOD_CONTROL)) {
        const tg_widget *cw=tg_ui_get(i->ui,e.target);
        while(cw) {
            if(cw->kind==TG_TEXT||cw->kind==TG_LISTBOX||cw->kind==TG_TREEVIEW||
               (cw->kind==TG_CANVAS&&cw->canvas_region_set&&cw->canvas_region.h>cw->rect.h)) {
                wheel_target=cw;
                if(cw->kind==TG_TEXT) tg_document_view_fractions(cw,&wheel_prev_first,&wheel_prev_last);
                else if(cw->kind==TG_LISTBOX) tg_listbox_view_fractions(cw,&wheel_prev_first,&wheel_prev_last);
                else if(cw->kind==TG_TREEVIEW) tg_tree_view_fractions(i->ui,cw,1,&wheel_prev_first,&wheel_prev_last);
                else tg_canvas_view_fractions(cw,1,&wheel_prev_first,&wheel_prev_last);
                break;
            }
            if(cw->kind==TG_TOPLEVEL) break;
            cw=tg_ui_get(i->ui,cw->parent);
        }
    }
    for(size_t k=0;k<script_count;k++) if(scripts[k][0]) {
        char expanded[MAX_WORD];
        if(!strcmp(scripts[k],"break")) return 1;
        if(!expand(i,scripts[k],e,expanded)) {
            (void)tg_var_store(i,"errorInfo","unsupported binding substitution"); return 1;
        }
        const char *cursor=expanded;
        int rc=tg_eval_script(i,&cursor,0);
        if(rc==TG_BREAK) return 1;
        if(rc!=TG_OK) {
            char *message=tg_copy_string(i->result);
            if(message) { (void)tg_var_store(i,"errorInfo",message); free(message); }
            return 1;
        }
        if(e.target&&!tg_ui_get(i->ui,e.target)) return 1;
    }
    /* Native fallback wheel scroll when no binding broke out or already scrolled this target. */
    if(wheel_target&&tg_ui_get(i->ui,wheel_target->id)==wheel_target) {
        double cur_first=-1.0,cur_last=-1.0;
        if(wheel_target->kind==TG_TEXT) tg_document_view_fractions(wheel_target,&cur_first,&cur_last);
        else if(wheel_target->kind==TG_LISTBOX) tg_listbox_view_fractions(wheel_target,&cur_first,&cur_last);
        else if(wheel_target->kind==TG_TREEVIEW) tg_tree_view_fractions(i->ui,wheel_target,1,&cur_first,&cur_last);
        else tg_canvas_view_fractions(wheel_target,1,&cur_first,&cur_last);
        if(cur_first==wheel_prev_first&&cur_last==wheel_prev_last) {
            int delta=(int)(int16_t)(e.key&0xffffu),units=-(delta/120);
            if(!units) units=delta>0?-1:1;
            if(wheel_target->kind==TG_TREEVIEW||wheel_target->kind==TG_TEXT) units*=3;
            char path[TG_WIDGET_PATH_CAP],amount[32],result[64];
            strcpy(path,wheel_target->path); snprintf(amount,sizeof amount,"%d",units);
            char *args[]={path,"yview","scroll",amount,"units"};
            if(tg_ui_command(i->ui,5,args,result,sizeof result)==TG_OK) {
                (void)tg_interp_dispatch_scroll(i);
                if(i->tk_invalidate) i->tk_invalidate(i->tk_host);
                return 1;
            }
        }
    }
    if(e.kind==TG_KEY_DOWN&&i->tk_hotkey&&i->tk_hotkey(i,e)) return 1;
    return 0;
}
