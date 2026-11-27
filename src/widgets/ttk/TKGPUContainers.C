#include "TKGPUContainers.H"
#include "../TKGPUWidgetInternal.H"
#include "../../core/TKGPUList.H"
#include "TKGPURaster.H"
#include "TKGPUText.H"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
enum { MAX_PANES=32,GAP=5,MENU_WIDTH=160 };
#define MENU_ROW tg_text_row_height(20)
typedef struct { char label[128],command[512],submenu[TG_WIDGET_PATH_CAP]; char variable[TG_WIDGET_PATH_CAP],on[128],off[128]; int separator,disabled,cascade,toggle,checked,explicit_variable; } entry;
struct tg_menu { unsigned count; int active; entry items[32]; };
tg_menu *tg_menu_create(void) { tg_menu *m=calloc(1,sizeof *m); if(m) m->active=-1; return m; }
void tg_menu_destroy(tg_menu *m) { free(m); }
tg_menu *tg_menu_clone(const tg_menu *m) { tg_menu *c=malloc(sizeof *c); if(c) *c=*m; return c; }
static int result(char *out,size_t cap,const char *s) { if(strlen(s)>=cap) return TG_ERROR; strcpy(out,s); return TG_OK; }
static int integer(const char *s,int *v) { char *end; errno=0; long n=strtol(s,&end,10); if(errno||end==s||*end||n<0||n>16384) return 0; *v=(int)n; return 1; }
static int clamp(int x,int lo,int hi) { return x<lo?lo:x>hi?hi:x; }
static void reset(tg_widget *w) { for(unsigned k=0;k<MAX_PANES;k++) w->sashes[k]=-1; }
void tg_container_defaults(tg_widget *w) { if(w->kind==TG_PANEDWINDOW) { w->vertical=1; reset(w); } }
int tg_container_option(tg_widget *w,const char *key,const char *s) {
    if(w->kind==TG_PANEDWINDOW) {
        if(!strcmp(key,"-orient")) {
            if(!strcmp(s,"horizontal")) w->vertical=0;
            else if(!strcmp(s,"vertical")) w->vertical=1;
            else return TG_ERROR;
            reset(w); return TG_OK;
        }
        if(!strcmp(key,"-sashwidth")||!strcmp(key,"-sashpad")||!strcmp(key,"-handlesize")||
           !strcmp(key,"-proxyborderwidth")||!strcmp(key,"-showhandle")||!strcmp(key,"-opaqueresize")||
           !strcmp(key,"-sashrelief")||!strcmp(key,"-proxyrelief")||!strcmp(key,"-proxybackground")||
           !strcmp(key,"-sashcursor")) {
            (void)s;
            return TG_OK;
        }
    }
    if(w->kind==TG_MENUBUTTON&&!strcmp(key,"-menu")) return result(w->menu_path,sizeof w->menu_path,s);
    return -1;
}
int tg_container_query(const tg_widget *w,const char *key,char *out,size_t cap) {
    if(w->kind==TG_PANEDWINDOW&&!strcmp(key,"-orient")) return result(out,cap,w->vertical?"vertical":"horizontal");
    if(w->kind==TG_MENUBUTTON&&!strcmp(key,"-menu")) return result(out,cap,w->menu_path);
    return -1;
}
static int pane_index(const tg_ui *u,const tg_widget *w,const char *s) {
    if(*s=='.') { tg_id id=tg_ui_find(u,s); for(unsigned k=0;k<w->tab_count;k++) if(w->tabs[k]==id) return (int)k; return -1; }
    int k; return integer(s,&k)&&k<(int)w->tab_count?k:-1;
}
/* Accept ttk -weight plus classic tk.PanedWindow pane options. */
static int pane_options(size_t n,char **a,int *weight) {
    if(n%2) return 0;
    for(size_t k=0;k<n;k+=2) {
        if(!strcmp(a[k],"-weight")) { if(!integer(a[k+1],weight)) return 0; }
        else if(!strcmp(a[k],"-stretch")) {
            if(!strcmp(a[k+1],"always")||!strcmp(a[k+1],"first")||!strcmp(a[k+1],"last")||
               !strcmp(a[k+1],"middle")||!strcmp(a[k+1],"never")) {
                if(!strcmp(a[k+1],"always")&&*weight==0) *weight=1;
            } else return 0;
        }
        else if(!strcmp(a[k],"-minsize")||!strcmp(a[k],"-height")||!strcmp(a[k],"-width")||
                !strcmp(a[k],"-padx")||!strcmp(a[k],"-pady")||!strcmp(a[k],"-sticky")||
                !strcmp(a[k],"-hide")) {
            continue;
        }
        else return 0;
    }
    return 1;
}
static int pane_insert(tg_ui *u,tg_widget *w,size_t n,char **a,char *out,size_t cap) {
    int inserting=!strcmp(a[1],"insert");
    size_t at=inserting?3:2;
    if(n<=at||!cap) return TG_ERROR;
    int pos=(int)w->tab_count;
    if(inserting&&strcmp(a[2],"end")) {
        pos=pane_index(u,w,a[2]);
        if(pos<0) return TG_ERROR;
    }
    tg_widget *p=tg_lookup(u,tg_ui_find(u,a[at]));
    if(!p||p->parent!=w->id||strpbrk(p->path," \t\r\n\\{}[]$;\"")) return TG_ERROR;
    int old=pane_index(u,w,a[at]);
    if((old>=0&&!inserting)||(old<0&&w->tab_count==MAX_PANES)) return TG_ERROR;
    int weight=old>=0?p->pane_weight:0;
    if(!pane_options(n-at-1,a+at+1,&weight)) return TG_ERROR;
    if(old>=0) {
        memmove(w->tabs+old,w->tabs+old+1,(w->tab_count-(unsigned)old-1)*sizeof w->tabs[0]);
        w->tab_count--;
    }
    if((unsigned)pos>w->tab_count) pos=(int)w->tab_count;
    memmove(w->tabs+pos+1,w->tabs+pos,(w->tab_count-(unsigned)pos)*sizeof w->tabs[0]);
    w->tabs[pos]=p->id; w->tab_count++; p->pane_weight=weight;
    reset(w); return result(out,cap,"");
}
static int extent(const tg_widget *w) { return w->vertical?w->rect.h:w->rect.w; }
static int gap(const tg_widget *w) { unsigned n=w->tab_count; return n>1?clamp(extent(w)/(int)(n-1),0,GAP):0; }
static void bounds(const tg_ui *u,const tg_widget *w,int *b) {
    unsigned n=w->tab_count; int space=extent(w)-(n?(int)(n-1)*gap(w):0),total=0,acc=0;
    b[0]=0; if(!n) return;
    for(unsigned k=0;k<n;k++) { const tg_widget *p=tg_ui_get(u,w->tabs[k]); total+=p&&p->pane_weight?p->pane_weight:1; }
    for(unsigned k=1;k<n;k++) {
        const tg_widget *p=tg_ui_get(u,w->tabs[k-1]); acc+=p&&p->pane_weight?p->pane_weight:1;
        int x=(int)((int64_t)space*acc/total);
        b[k]=clamp(w->sashes[k-1]>=0?w->sashes[k-1]:x,b[k-1],space);
    }
    b[n]=space;
}
tg_rect tg_pane_rect(const tg_ui *u,const tg_widget *w,tg_id id) {
    int b[MAX_PANES+1]; bounds(u,w,b); int g=gap(w); tg_rect r=w->rect;
    for(unsigned k=0;k<w->tab_count;k++) if(w->tabs[k]==id) {
        int start=b[k]+(int)k*g,len=b[k+1]-b[k];
        return w->vertical?(tg_rect){r.x,r.y+start,r.w,len}:(tg_rect){r.x+start,r.y,len,r.h};
    }
    return (tg_rect){r.x,r.y,0,0};
}
static int sash(tg_ui *u,tg_widget *w,int k,int pos,int set) {
    int b[MAX_PANES+1]; bounds(u,w,b); int g=gap(w);
    if(set) { w->sashes[k]=clamp(pos-k*g,b[k],b[k+2]); bounds(u,w,b); }
    return b[k+1]+k*g;
}
static void forget(tg_widget *w,unsigned k) { memmove(w->tabs+k,w->tabs+k+1,(w->tab_count-k-1)*sizeof w->tabs[0]); w->tab_count--; reset(w); }
/* Root remains in menu_posted for compatibility with existing render paths.
   Descendants are stable widget IDs, never pointers into the dense tree. */
static int menu_level(const tg_ui *u,tg_id id) {
    if(!id||!u->menu_posted) return -1;
    if(id==u->menu_posted) return 0;
    for(unsigned k=0;k<u->menu_depth;k++) if(u->menu_chain[k]==id) return (int)k+1;
    return -1;
}
int tg_menu_mapped(const tg_ui *u,tg_id id) { return menu_level(u,id)>=0; }
static tg_id level_id(const tg_ui *u,unsigned level) {
    return level?u->menu_chain[level-1]:u->menu_posted;
}
static void menu_trim(tg_ui *u,unsigned level) {
    while(u->menu_depth>level) {
        tg_widget *w=tg_lookup(u,u->menu_chain[u->menu_depth-1]);
        if(w&&w->menu) w->menu->active=-1;
        u->menu_depth--;
    }
}
static void menu_close(tg_ui *u) {
    menu_trim(u,0);
    tg_widget *w=tg_lookup(u,u->menu_posted);
    if(w&&w->menu) w->menu->active=-1;
    u->menu_posted=0; u->menu_host=0;
}
static tg_id menu_owner(const tg_ui *u,const tg_widget *w) {
    if(!u->native_toplevels) return 1;
    while(w&&w->kind!=TG_TOPLEVEL) w=tg_ui_get(u,w->parent);
    return w?w->id:1;
}
static void menu_post(tg_ui *u,tg_widget *w,tg_id host,int x,int y) {
    menu_close(u); u->choice_posted=0;
    u->menu_posted=w->id; u->menu_host=host; u->menu_x=x; u->menu_y=y;
    w->menu->active=-1;
    u->pressed=u->entry_drag=u->drag_scroll=u->drag_pane=0;
}
static int menu_cascade(tg_ui *u,tg_widget *w,int index) {
    int level=menu_level(u,w->id);
    if(level<0) return TG_OK;
    if(index<0) { menu_trim(u,(unsigned)level); return TG_OK; }
    if((unsigned)index>=w->menu->count) return TG_ERROR;
    entry *e=&w->menu->items[index];
    if(!e->cascade||e->disabled) { menu_trim(u,(unsigned)level); return TG_OK; }
    tg_id child=tg_ui_find(u,e->submenu);
    const tg_widget *sub=tg_ui_get(u,child);
    if(!sub||sub->kind!=TG_MENU||!sub->menu||level>=TG_MENU_DEPTH) {
        menu_trim(u,(unsigned)level); return TG_ERROR;
    }
    for(int k=0;k<=level;k++) if(level_id(u,(unsigned)k)==child) {
        menu_trim(u,(unsigned)level); return TG_ERROR;
    }
    if(u->menu_depth>(unsigned)level&&u->menu_chain[level]==child&&u->menu_rows[level]==(unsigned)index)
        return TG_OK;
    menu_trim(u,(unsigned)level);
    u->menu_chain[level]=child; u->menu_rows[level]=(unsigned)index; u->menu_depth=(unsigned)level+1;
    return TG_OK;
}
static void menu_changed(tg_ui *u,tg_widget *w) {
    int level=menu_level(u,w->id);
    if(level>=0) menu_trim(u,(unsigned)level);
    w->menu->active=-1;
}
void tg_container_detach(tg_ui *u,tg_widget *p) {
    if(p->menu) for(unsigned k=0;k<p->menu->count;k++) if(p->menu->items[k].toggle) u->menu_toggle_count--;
    if(u->menu_activation==p->id) u->menu_activation=0;
    int level=menu_level(u,p->id);
    if(level==0||u->menu_host==p->id) menu_close(u);
    else if(level>0) menu_trim(u,(unsigned)level-1);
    if(u->drag_pane==p->id) u->drag_pane=0;
    tg_widget *w=tg_lookup(u,p->parent);
    if(w&&w->kind==TG_PANEDWINDOW) for(unsigned k=0;k<w->tab_count;k++) if(w->tabs[k]==p->id) { forget(w,k); break; }
}
static int menu_index(const tg_menu *m,const char *s) {
    int k;
    if(!strcmp(s,"end")||!strcmp(s,"last")) return m->count?(int)m->count-1:-1;
    if(!strcmp(s,"active")) return m->active;
    if(!strcmp(s,"none")) return -1;
    if(*s=='@') return integer(s+1,&k)&&k/MENU_ROW<(int)m->count?k/MENU_ROW:-1;
    if(integer(s,&k)) return k<(int)m->count?k:-1;
    for(unsigned j=0;j<m->count;j++) if(!strcmp(m->items[j].label,s)) return (int)j;
    return -1;
}
int tg_menu_script(const tg_widget *w,const char *s,char *out,size_t cap) {
    if(!w||w->kind!=TG_MENU||!w->menu) return -1;
    int k=menu_index(w->menu,s); if(k<0) return TG_ERROR;
    const entry *e=&w->menu->items[k]; return result(out,cap,e->separator||e->disabled?"":e->command);
}
static const char *global_name(const char *s) { return s[0]==':'&&s[1]==':'?s+2:s; }
int tg_menu_sync_widget(tg_widget *w,const char *key,const char *value) {
    if(!w||!w->menu) return 0;
    int changed=0;
    for(unsigned k=0;k<w->menu->count;k++) {
        entry *e=&w->menu->items[k];
        if(!e->toggle||!e->variable[0]||strcmp(global_name(e->variable),global_name(key))) continue;
        int checked=value&&!strcmp(value,e->on);
        changed|=checked!=e->checked; e->checked=checked;
    }
    return changed;
}
int tg_menu_binding(const tg_widget *w,unsigned row,tg_menu_action *out) {
    if(!w||!w->menu||row>=w->menu->count) return -1;
    const entry *e=&w->menu->items[row];
    memset(out,0,sizeof *out);
    if(!e->toggle||!e->variable[0]) return 0;
    strcpy(out->variable,e->variable); strcpy(out->value,e->toggle==1?e->off:"");
    return 1;
}
int tg_menu_prepare_row(const tg_widget *w,unsigned row,tg_menu_action *out) {
    if(!w||!w->menu||row>=w->menu->count) return TG_ERROR;
    const entry *e=&w->menu->items[row];
    memset(out,0,sizeof *out);
    if(e->disabled||e->separator) return TG_OK;
    strcpy(out->command,e->command);
    if(e->toggle) { strcpy(out->variable,e->variable); strcpy(out->value,e->toggle==1&&e->checked?e->off:e->on); }
    return TG_OK;
}
int tg_menu_prepare_action(const tg_widget *w,const char *index,tg_menu_action *out) {
    if(!w||w->kind!=TG_MENU||!w->menu) return -1;
    int row=menu_index(w->menu,index);
    return row<0?TG_ERROR:tg_menu_prepare_row(w,(unsigned)row,out);
}
/* Parse options into a temporary entry; invalid edits never mutate the menu. */
static int entry_options(entry *e,size_t n,char **a) {
    if(n%2) return 0;
    for(size_t k=0;k<n;k+=2) {
        if(!strcmp(a[k],"-label")) {
            if(result(e->label,sizeof e->label,a[k+1])!=TG_OK) return 0;
        } else if(!strcmp(a[k],"-command")) {
            if(result(e->command,sizeof e->command,a[k+1])!=TG_OK) return 0;
        } else if(!strcmp(a[k],"-menu")&&e->cascade) {
            if(result(e->submenu,sizeof e->submenu,a[k+1])!=TG_OK) return 0;
        } else if(!strcmp(a[k],"-variable")&&e->toggle) {
            if(result(e->variable,sizeof e->variable,a[k+1])!=TG_OK) return 0;
            e->explicit_variable=1;
        } else if((!strcmp(a[k],"-onvalue")&&e->toggle==1)||(!strcmp(a[k],"-value")&&e->toggle==2)) {
            if(result(e->on,sizeof e->on,a[k+1])!=TG_OK) return 0;
        } else if(!strcmp(a[k],"-offvalue")&&e->toggle==1) {
            if(result(e->off,sizeof e->off,a[k+1])!=TG_OK) return 0;
        } else if(!strcmp(a[k],"-state")) {
            if(!strcmp(a[k+1],"disabled")) e->disabled=1;
            else if(!strcmp(a[k+1],"normal")) e->disabled=0;
            else return 0;
        } else return 0;
    }
    if(e->toggle&&!e->explicit_variable&&!e->variable[0]) strcpy(e->variable,e->label);
    return 1;
}
static const char *entry_value(const entry *e,const char *key) {
    if(!strcmp(key,"-label")) return e->label;
    if(!strcmp(key,"-command")) return e->command;
    if(!strcmp(key,"-menu")&&e->cascade) return e->submenu;
    if(!strcmp(key,"-state")) return e->disabled?"disabled":"normal";
    if(!strcmp(key,"-variable")&&e->toggle) return e->variable;
    if((!strcmp(key,"-onvalue")&&e->toggle==1)||(!strcmp(key,"-value")&&e->toggle==2)) return e->on;
    if(!strcmp(key,"-offvalue")&&e->toggle==1) return e->off;
    return NULL;
}
static int entry_spec(const entry *item,const char *option,char *out,size_t cap) {
    const char *v=entry_value(item,option);
    if(!v||!cap) return TG_ERROR;
    out[0]=0;
    return tg_list_append(out,cap,option)&&tg_list_append(out,cap,"")&&tg_list_append(out,cap,"")&&
           tg_list_append(out,cap,!strcmp(option,"-state")?"normal":"")&&tg_list_append(out,cap,v)?TG_OK:TG_ERROR;
}
static int menu_command(tg_ui *u,tg_widget *w,size_t n,char **a,char *out,size_t cap) {
    tg_menu *m=w->menu; if(!m) return TG_ERROR;
    if(!strcmp(a[1],"add")||!strcmp(a[1],"insert")) {
        int ins=!strcmp(a[1],"insert");
        size_t type=ins?3:2;
        if(n<=type||m->count==32) return TG_ERROR;
        int pos=(int)m->count;
        if(ins&&strcmp(a[2],"end")) {
            if(!integer(a[2],&pos)||pos>(int)m->count) return TG_ERROR;
        } else if(ins&&m->count) pos=(int)m->count-1;
        entry item={0};
        if(!strcmp(a[type],"separator")) item.separator=1;
        else if(!strcmp(a[type],"cascade")) item.cascade=1;
        else if(!strcmp(a[type],"checkbutton")) { item.toggle=1; strcpy(item.on,"1"); strcpy(item.off,"0"); }
        else if(!strcmp(a[type],"radiobutton")) { item.toggle=2; strcpy(item.variable,"selectedButton"); }
        else if(strcmp(a[type],"command")) return TG_ERROR;
        if(!entry_options(&item,n-type-1,a+type+1)) return TG_ERROR;
        memmove(m->items+pos+1,m->items+pos,(m->count-(unsigned)pos)*sizeof item);
        m->items[pos]=item; m->count++; if(item.toggle) u->menu_toggle_count++; menu_changed(u,w);
        return result(out,cap,"");
    }
    if(n==4&&!strcmp(a[1],"post")) {
        int x,y; if(!integer(a[2],&x)||!integer(a[3],&y)) return TG_ERROR;
        menu_post(u,w,menu_owner(u,w),x,y); return result(out,cap,"");
    }
    if(n==2&&!strcmp(a[1],"unpost")) {
        int level=menu_level(u,w->id);
        if(level==0) menu_close(u); else if(level>0) menu_trim(u,(unsigned)level-1);
        return result(out,cap,"");
    }
    if(n<3) return -1;
    int k=menu_index(m,a[2]);
    if(!strcmp(a[1],"index")&&n==3) { if(k<0) return result(out,cap,"none"); int z=snprintf(out,cap,"%d",k); return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR; }
    if(n==3&&!strcmp(a[1],"activate")) {
        m->active=k>=0&&!m->items[k].disabled&&!m->items[k].separator?k:-1;
        return result(out,cap,"");
    }
    if(n==3&&!strcmp(a[1],"postcascade")) {
        if(k<0&&strcmp(a[2],"none")&&strcmp(a[2],"active")) return TG_ERROR;
        return menu_cascade(u,w,k)==TG_OK?result(out,cap,""):TG_ERROR;
    }
    if(k<0) return TG_ERROR;
    if(n==3&&!strcmp(a[1],"yposition")) {
        int z=snprintf(out,cap,"%d",k*MENU_ROW); return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR;
    }
    if(!strcmp(a[1],"delete")&&(n==3||n==4)) {
        int last=n==3?k:menu_index(m,a[3]); if(last<0) return TG_ERROR;
        if(last>=k) {
            unsigned count=(unsigned)(last-k+1);
            for(int j=k;j<=last;j++) if(m->items[j].toggle) u->menu_toggle_count--;
            memmove(m->items+k,m->items+last+1,(m->count-(unsigned)last-1)*sizeof m->items[0]);
            m->count-=count; menu_changed(u,w);
        }
        return result(out,cap,"");
    }
    if(!strcmp(a[1],"type")&&n==3) return result(out,cap,m->items[k].separator?"separator":m->items[k].cascade?"cascade":m->items[k].toggle==1?"checkbutton":m->items[k].toggle==2?"radiobutton":"command");
    if(!strcmp(a[1],"entrycget")&&n==4) {
        const char *v=entry_value(&m->items[k],a[3]); return v?result(out,cap,v):TG_ERROR;
    }
    if(!strcmp(a[1],"entryconfigure")||!strcmp(a[1],"entryconfig")) {
        if(n==4) return entry_spec(&m->items[k],a[3],out,cap);
        if(n==3) {
            static const char *keys[]={"-label","-command","-state","-menu","-variable","-onvalue","-offvalue","-value"};
            char spec[2048]; out[0]=0;
            for(unsigned j=0;j<sizeof keys/sizeof keys[0];j++) {
                if(!entry_value(&m->items[k],keys[j])) continue;
                if(entry_spec(&m->items[k],keys[j],spec,sizeof spec)!=TG_OK||
                   !tg_list_append(out,cap,spec)) return TG_ERROR;
            }
            return TG_OK;
        }
        entry copy=m->items[k];
        if(!entry_options(&copy,n-3,a+3)) return TG_ERROR;
        m->items[k]=copy; menu_changed(u,w); return result(out,cap,"");
    }
    return -1;
}
int tg_container_command(tg_ui *u,tg_widget *w,size_t n,char **a,char *out,size_t cap) {
    if(n>=2&&(!strcmp(a[1],"cget")||!strcmp(a[1],"configure")||!strcmp(a[1],"config"))) return -1;
    if(n<2||!cap) return -1;
    if(w->kind==TG_MENU) return menu_command(u,w,n,a,out,cap);
    if(w->kind!=TG_PANEDWINDOW) return -1;
    if(!strcmp(a[1],"panes")&&n==2) {
        size_t len=0; out[0]=0;
        for(unsigned k=0;k<w->tab_count;k++) { const tg_widget *p=tg_ui_get(u,w->tabs[k]); if(!p) return TG_ERROR; size_t z=strlen(p->path); if(len+z+(k?1u:0u)>=cap) return TG_ERROR; if(k) out[len++]=' '; memcpy(out+len,p->path,z+1); len+=z; }
        return TG_OK;
    }
    if(!strcmp(a[1],"add")||!strcmp(a[1],"insert"))
        return pane_insert(u,w,n,a,out,cap);
    if(!strcmp(a[1],"identify")&&n==4) {
        int x,y; char extra;
        if(sscanf(a[2],"%d%c",&x,&extra)!=1||sscanf(a[3],"%d%c",&y,&extra)!=1) return TG_ERROR;
        if(x<0||y<0||x>=w->rect.w||y>=w->rect.h) return result(out,cap,"");
        int p=w->vertical?y:x;
        for(unsigned j=0;j+1<w->tab_count;j++) {
            int start=sash(u,w,(int)j,0,0);
            if(p>=start&&p<start+gap(w)) {
                int z=snprintf(out,cap,"%u",j); return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR;
            }
        }
        return result(out,cap,"");
    }
    if(n<3) return -1;
    int k=pane_index(u,w,a[2]); if(k<0) return -1;
    if(!strcmp(a[1],"forget")&&n==3) { forget(w,(unsigned)k); return result(out,cap,""); }
    if(!strcmp(a[1],"pane")) {
        tg_widget *p=tg_lookup(u,w->tabs[k]); if(!p) return TG_ERROR;
        if(n==3||n==4) {
            if(n==4&&strcmp(a[3],"-weight")) return TG_ERROR;
            int z=snprintf(out,cap,n==3?"-weight %d":"%d",p->pane_weight);
            return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR;
        }
        int weight=p->pane_weight;
        if(!pane_options(n-3,a+3,&weight)) return TG_ERROR;
        if(weight!=p->pane_weight) { p->pane_weight=weight; reset(w); }
        return result(out,cap,"");
    }
    if(!strcmp(a[1],"sashpos")&&(n==3||n==4)&&k+1<(int)w->tab_count) {
        int pos=0; if(n==4&&!integer(a[3],&pos)) return TG_ERROR;
        int z=snprintf(out,cap,"%d",sash(u,w,k,pos,n==4)); return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR;
    }
    return -1;
}
static int menu_visible(const tg_ui *u) {
    return u->menu_posted&&(!u->native_toplevels||!u->view_root||u->view_root==u->menu_host);
}
static tg_rect menu_rect_at(const tg_ui *u,unsigned level) {
    const tg_widget *host=tg_ui_get(u,u->menu_host?u->menu_host:1);
    if(!host) return (tg_rect){0,0,0,0};
    tg_rect root=host->rect,r={0,0,0,0};
    for(unsigned k=0;k<=level;k++) {
        const tg_widget *w=tg_ui_get(u,level_id(u,k));
        if(!w||!w->menu) return (tg_rect){0,0,0,0};
        int width=clamp(MENU_WIDTH,0,root.w),height=clamp((int)w->menu->count*MENU_ROW,0,root.h);
        int x=k?r.x+r.w:u->menu_x;
        int y=k?r.y+(int)u->menu_rows[k-1]*MENU_ROW:u->menu_y;
        if(k&&x+width>root.w) x=r.x-width;
        r=(tg_rect){clamp(x,0,root.w-width),clamp(y,0,root.h-height),width,height};
    }
    return r;
}
tg_id tg_menu_hit(const tg_ui *u,int x,int y) {
    if(!menu_visible(u)) return 0;
    for(int k=(int)u->menu_depth;k>=0;k--) {
        tg_rect r=menu_rect_at(u,(unsigned)k);
        if(x>=r.x&&y>=r.y&&x<r.x+r.w&&y<r.y+r.h) return level_id(u,(unsigned)k);
    }
    return 0;
}
static int menu_select(tg_ui *u,tg_widget *w,int row,int click,char *script,size_t cap) {
    if(row<0||(unsigned)row>=w->menu->count) return 1;
    entry *item=&w->menu->items[row];
    int previous=w->menu->active;
    unsigned depth=u->menu_depth;
    w->menu->active=item->disabled||item->separator?-1:row;
    if(item->cascade&&!item->disabled) {
        (void)menu_cascade(u,w,row);
        return click||previous!=w->menu->active||depth!=u->menu_depth;
    }
    (void)menu_cascade(u,w,-1);
    if(click&&!item->disabled&&!item->separator) {
        if(item->toggle) { u->menu_activation=w->id; u->menu_activation_row=(unsigned)row; }
        else (void)result(script,cap,item->command);
        menu_close(u);
        return 1;
    }
    return click||previous!=w->menu->active||depth!=u->menu_depth;
}
static int menu_key(tg_ui *u,uint32_t key,char *script,size_t cap) {
    unsigned level=u->menu_depth;
    tg_widget *w=tg_lookup(u,level_id(u,level));
    if(!w||!w->menu) { menu_close(u); return 1; }
    tg_menu *m=w->menu;
    if(key==TG_KEY_ESCAPE) { menu_close(u); return 1; }
    if(key==TG_KEY_LEFT) { if(level) menu_trim(u,level-1); else menu_close(u); return 1; }
    if(key==TG_KEY_RETURN||key==TG_KEY_RIGHT) {
        if(m->active>=0&&(key==TG_KEY_RETURN||m->items[m->active].cascade)) {
            unsigned before=u->menu_depth;
            int rc=menu_select(u,w,m->active,key==TG_KEY_RETURN,script,cap);
            if(u->menu_depth>before) {
                tg_widget *sub=tg_lookup(u,level_id(u,u->menu_depth));
                if(sub&&sub->menu) for(unsigned k=0;k<sub->menu->count;k++)
                    if(!sub->menu->items[k].disabled&&!sub->menu->items[k].separator) { sub->menu->active=(int)k; break; }
            }
            return rc;
        }
        return 1;
    }
    if(key==TG_KEY_ARROW_UP||key==TG_KEY_ARROW_DOWN) {
        int step=key==TG_KEY_ARROW_UP?-1:1,n=(int)m->count;
        int row=m->active;
        if(row<0) row=step>0?-1:0;
        for(int k=0;k<n;k++) {
            row=(row+step+n)%n;
            if(!m->items[row].disabled&&!m->items[row].separator) { m->active=row; break; }
        }
        return 1;
    }
    return 1;
}
/* Consume overlay input before underlying widget bindings/validation.
   A zero return consumes without requesting an unnecessary repaint. */
int tg_menu_input(tg_ui *u,tg_event e,char *script,size_t cap) {
    u->menu_activation=0;
    if(!u->menu_posted) return -1;
    if(!menu_visible(u)) {
        if(e.kind==TG_POINTER_DOWN) menu_close(u);
        return -1;
    }
    if(e.kind==TG_KEY_DOWN) return menu_key(u,e.key&TG_KEY_VALUE_MASK,script,cap);
    if(e.kind==TG_POINTER_MOVE||e.kind==TG_POINTER_DOWN) {
        tg_id hit=tg_menu_hit(u,e.x,e.y);
        int level=menu_level(u,hit);
        tg_widget *w=tg_lookup(u,hit);
        if(level>=0&&w&&w->menu) {
            tg_rect r=menu_rect_at(u,(unsigned)level);
            return menu_select(u,w,(e.y-r.y)/MENU_ROW,e.kind==TG_POINTER_DOWN,script,cap);
        }
        if(e.kind==TG_POINTER_DOWN) { menu_close(u); return 1; }
        return 0;
    }
    if(e.kind==TG_POINTER_UP||e.kind==TG_TEXT_INPUT||e.kind==TG_KEY_UP) return 0;
    return -1;
}
int tg_container_input(tg_ui *u,tg_event e,char *script,size_t cap) {
    if(u->drag_pane&&(e.kind==TG_POINTER_MOVE||e.kind==TG_POINTER_UP)) {
        tg_widget *w=tg_lookup(u,u->drag_pane);
        if(w&&w->kind==TG_PANEDWINDOW&&u->drag_sash+1<(int)w->tab_count) (void)sash(u,w,u->drag_sash,w->vertical?e.y-w->rect.y:e.x-w->rect.x,1);
        if(e.kind==TG_POINTER_UP) u->drag_pane=0;
        return 1;
    }
    if(e.kind!=TG_POINTER_DOWN) return -1;
    tg_widget *w=tg_lookup(u,e.target); if(!w||w->state==TG_STATE_DISABLED) return -1;
    if(w->kind==TG_MENUBUTTON) { tg_widget *m=tg_lookup(u,tg_ui_find(u,w->menu_path)); if(m&&m->kind==TG_MENU) { menu_post(u,m,menu_owner(u,w),w->rect.x,w->rect.y+w->rect.h); } return 1; }
    if(w->kind==TG_PANEDWINDOW) { int p=w->vertical?e.y-w->rect.y:e.x-w->rect.x; for(unsigned k=0;k+1<w->tab_count;k++) { int s=sash(u,w,(int)k,0,0); if(p>=s&&p<s+gap(w)) { u->drag_pane=w->id; u->drag_sash=(int)k; return 1; } } }
    return -1;
}
void tg_container_paint(const tg_widget *w,tg_canvas *c,uint32_t fg) {
    tg_rect r=w->rect;
    if(w->kind==TG_MENUBUTTON) { tg_text_draw_row(c,r,r.x+4,w->text,fg); if(r.w>=12&&r.h>=8) { int x=r.x+r.w-11,y=r.y+r.h/2; tg_raster_line(c,r,x,y-2,x+4,y+2,fg); tg_raster_line(c,r,x+4,y+2,x+8,y-2,fg); } }
    if(w->kind==TG_SIZEGRIP) for(int k=4;k<=12;k+=4) tg_raster_line(c,r,r.x+r.w-k,r.y+r.h-2,r.x+r.w-2,r.y+r.h-k,fg);
}
void tg_menu_paint(const tg_ui *u,tg_canvas *c) {
    if(!menu_visible(u)) return;
    for(unsigned level=0;level<=u->menu_depth;level++) {
        const tg_widget *w=tg_ui_get(u,level_id(u,level)); if(!w||!w->menu) break;
        tg_rect r=menu_rect_at(u,level); tg_raster_fill(c,r,r,w->bg); tg_raster_outline(c,r,r,0x61768c);
        for(unsigned k=0;k<w->menu->count;k++) {
            const entry *e=&w->menu->items[k]; int y=r.y+(int)k*MENU_ROW;
            if(y>=r.y+r.h) break;
            if(e->separator) { tg_raster_line(c,r,r.x+3,y+MENU_ROW/2,r.x+r.w-4,y+MENU_ROW/2,0x61768c); continue; }
            tg_rect line;
            if(!tg_rect_intersect(r,(tg_rect){r.x,y,r.w,MENU_ROW},&line)) continue;
            uint32_t fg=e->disabled?0x808080:w->explicit_fg?w->fg:0xf0f5ff;
            if(w->menu->active==(int)k) tg_raster_fill(c,line,line,0x344c65);
            if(e->cascade&&line.w>=16) {
                int x=r.x+r.w-10,mid=y+MENU_ROW/2;
                tg_raster_line(c,line,x-3,mid-4,x+1,mid,fg);
                tg_raster_line(c,line,x+1,mid,x-3,mid+4,fg);
                line.w-=14;
            }
            int label_x=r.x+5;
            if(e->toggle&&line.w>=20) {
                int mid=y+MENU_ROW/2;
                tg_rect mark={r.x+4,mid-5,10,10};
                if(e->toggle==2) {
                    tg_raster_ellipse(c,line,mark,fg,0);
                    if(e->checked) tg_raster_ellipse(c,line,(tg_rect){mark.x+2,mark.y+2,6,6},fg,1);
                } else {
                    tg_raster_outline(c,line,mark,fg);
                    if(e->checked) {
                        tg_raster_line(c,line,mark.x+1,mid,mark.x+4,mid+3,fg);
                        tg_raster_line(c,line,mark.x+4,mid+3,mark.x+8,mid-3,fg);
                    }
                }
                label_x+=14;
            }
            tg_text_draw_row(c,line,label_x,e->label,fg);
        }
    }
}
