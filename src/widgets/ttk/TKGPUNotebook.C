#include "TKGPUNotebook.H"
#include "TKGPUStyle.H"
#include "../image/TKGPUImageRegistry.H"
#include "../TKGPUWidgetInternal.H"
#include "../../core/TKGPUList.H"
#include "TKGPUText.H"
#include "TKGPUUtf8.H"
#include "TKGPURaster.H"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
static int result(char *out,size_t cap,const char *s) {
    if(strlen(s)>=cap) return TG_ERROR;
    strcpy(out,s); return TG_OK;
}
static int tab_at(const tg_ui *,const tg_widget *,int,int);
static int tab_box(const tg_ui *,const tg_widget *,int,tg_rect *);
static int index_of(const tg_ui *u,const tg_widget *w,const char *s) {
    if(*s=='.') {
        tg_id id=tg_ui_find(u,s);
        for(unsigned k=0;k<w->tab_count;k++) if(w->tabs[k]==id) return (int)k;
        return -1;
    }
    if(*s=='@') {
        int x,y; char tail;
        if(sscanf(s+1,"%d,%d%c",&x,&y,&tail)!=2) return -1;
        return tab_at(u,w,x,y);
    }
    if(!strcmp(s,"current")) {
        for(unsigned k=0;k<w->tab_count;k++) if(w->tabs[k]==w->selected) return (int)k;
        return -1;
    }
    char *end; errno=0; long k=strtol(s,&end,10);
    return !errno&&end!=s&&!*end&&k>=0&&k<w->tab_count?(int)k:-1;
}
static void changed(tg_ui *u,tg_widget *w,tg_id previous) {
    if(previous!=w->selected)
        (void)tg_ui_post(u,(tg_event){tg_virtual_event("NotebookTabChanged"),w->id,0,0,0});
}
static void tab_defaults(tg_widget *p) {
    p->tab_state=1; p->tab_text[0]=p->tab_image[0]=p->tab_compound[0]=0;
    strcpy(p->tab_padding,"0"); memset(p->tab_pad,0,sizeof p->tab_pad);
    p->tab_sticky=15; p->tab_underline=-1;
}
static void repair(tg_ui *u,tg_widget *w) {
    const tg_widget *p=tg_ui_get(u,w->selected);
    if(p&&p->tab_state==1) return;
    w->selected=0;
    for(unsigned k=0;k<w->tab_count;k++) {
        p=tg_ui_get(u,w->tabs[k]);
        if(p&&p->tab_state==1) { w->selected=p->id; break; }
    }
}
static void forget(tg_ui *u,tg_widget *w,unsigned k) {
    tg_widget *p=tg_lookup(u,w->tabs[k]);
    tg_id previous=w->selected;
    if(p) { tab_defaults(p); p->tab_state=0; }
    memmove(w->tabs+k,w->tabs+k+1,(w->tab_count-k-1)*sizeof w->tabs[0]);
    w->tab_count--; repair(u,w); changed(u,w,previous);
}
void tg_notebook_detach(tg_ui *u,tg_widget *p) {
    tg_widget *w=tg_lookup(u,p->parent);
    if(!w||w->kind!=TG_NOTEBOOK) return;
    for(unsigned k=0;k<w->tab_count;k++) if(w->tabs[k]==p->id) { forget(u,w,k); return; }
}
static const char *state_name(int state) { return state==2?"hidden":state==3?"disabled":"normal"; }
static int padding(tg_widget *p,const char *value) {
    const char *cur=value; char word[24],normalized[48]=""; int values[4],n=0,rc;
    while((rc=tg_list_next(&cur,word,sizeof word))>0) {
        char *end; errno=0; long x=strtol(word,&end,10);
        if(n==4||errno||end==word||*end||x<0||x>4096) return 0;
        values[n++]=(int)x;
        char number[16]; snprintf(number,sizeof number,"%ld",x);
        if(!tg_list_append(normalized,sizeof normalized,number)) return 0;
    }
    if(rc<0||!n) return 0;
    p->tab_pad[0]=values[0]; p->tab_pad[1]=n>1?values[1]:values[0];
    p->tab_pad[2]=n>2?values[2]:values[0]; p->tab_pad[3]=n>3?values[3]:p->tab_pad[1];
    strcpy(p->tab_padding,normalized); return 1;
}
static int options(const tg_ui *u,tg_widget *p,size_t n,char **a) {
    if(n%2) return 0;
    for(size_t k=0;k<n;k+=2) {
        if(!strcmp(a[k],"-text")&&strlen(a[k+1])<sizeof p->tab_text) strcpy(p->tab_text,a[k+1]);
        else if(!strcmp(a[k],"-padding")) { if(!padding(p,a[k+1])) return 0; }
        else if(!strcmp(a[k],"-sticky")) {
            unsigned bits=0;
            for(const char *v=a[k+1];*v;v++) {
                if(*v=='n') bits|=1; else if(*v=='s') bits|=2;
                else if(*v=='e') bits|=4; else if(*v=='w') bits|=8;
                else if(*v!=' '&&*v!=',') return 0;
            }
            p->tab_sticky=bits;
        }
        else if(!strcmp(a[k],"-underline")) {
            char *end; errno=0; long x=strtol(a[k+1],&end,10);
            if(errno||end==a[k+1]||*end||x< -1||x>1024) return 0;
            p->tab_underline=(int)x;
        }
        else if(!strcmp(a[k],"-image")) {
            if(strlen(a[k+1])>=sizeof p->tab_image||(*a[k+1]&&!tg_image_find(u,a[k+1]))) return 0;
            strcpy(p->tab_image,a[k+1]);
        }
        else if(!strcmp(a[k],"-compound")) {
            const char *v=a[k+1];
            if(*v&&strcmp(v,"none")&&strcmp(v,"text")&&strcmp(v,"image")&&strcmp(v,"left")&&
               strcmp(v,"right")&&strcmp(v,"top")&&strcmp(v,"bottom")&&strcmp(v,"center")) return 0;
            strcpy(p->tab_compound,v);
        }
        else if(!strcmp(a[k],"-state")) {
            if(!strcmp(a[k+1],"normal")) p->tab_state=1;
            else if(!strcmp(a[k+1],"hidden")) p->tab_state=2;
            else if(!strcmp(a[k+1],"disabled")) p->tab_state=3;
            else return 0;
        } else return 0;
    }
    return 1;
}
static void tab_apply(tg_widget *p,const tg_widget *opt) {
    p->tab_state=opt->tab_state; strcpy(p->tab_text,opt->tab_text); strcpy(p->tab_image,opt->tab_image);
    strcpy(p->tab_padding,opt->tab_padding); strcpy(p->tab_compound,opt->tab_compound);
    memcpy(p->tab_pad,opt->tab_pad,sizeof p->tab_pad); p->tab_sticky=opt->tab_sticky; p->tab_underline=opt->tab_underline;
}
static const char *tab_value(const tg_widget *p,const char *key,char buffer[32]) {
    if(!strcmp(key,"-text")) return p->tab_text;
    if(!strcmp(key,"-state")) return state_name(p->tab_state);
    if(!strcmp(key,"-image")) return p->tab_image;
    if(!strcmp(key,"-compound")) return p->tab_compound;
    if(!strcmp(key,"-padding")) return p->tab_padding;
    if(!strcmp(key,"-underline")) { snprintf(buffer,32,"%d",p->tab_underline); return buffer; }
    if(!strcmp(key,"-sticky")) {
        size_t n=0; const char *names="nsew";
        for(unsigned k=0;k<4;k++) if(p->tab_sticky&(1u<<k)) buffer[n++]=names[k];
        buffer[n]=0; return buffer;
    }
    return NULL;
}
tg_rect tg_notebook_page_rect(const tg_widget *p,tg_rect r) {
    int left=p->tab_pad[0]<r.w?p->tab_pad[0]:r.w;
    int top=p->tab_pad[1]<r.h?p->tab_pad[1]:r.h;
    r.x+=left; r.y+=top; r.w-=left; r.h-=top;
    r.w-=p->tab_pad[2]<r.w?p->tab_pad[2]:r.w;
    r.h-=p->tab_pad[3]<r.h?p->tab_pad[3]:r.h;
    unsigned bits=p->tab_sticky;
    if((bits&12)!=12&&p->request.w<r.w) {
        int width=p->request.w>0?p->request.w:0,extra=r.w-width;
        r.x+=(bits&8)?0:(bits&4)?extra:extra/2; r.w=width;
    }
    if((bits&3)!=3&&p->request.h<r.h) {
        int height=p->request.h>0?p->request.h:0,extra=r.h-height;
        r.y+=(bits&1)?0:(bits&2)?extra:extra/2; r.h=height;
    }
    return r;
}
/* A move changes only the tab order. Existing page state, widget ID and
   selection survive unless the requested options make selection invalid. */
static int insert_tab(tg_ui *u,tg_widget *w,size_t n,char **a,char *out,size_t cap) {
    if(n<4) return TG_ERROR;
    int pos=!strcmp(a[2],"end")?(int)w->tab_count:index_of(u,w,a[2]);
    if(pos<0) return TG_ERROR;
    tg_widget *p=tg_lookup(u,tg_ui_find(u,a[3]));
    if(!p||p->parent!=w->id||strpbrk(p->path," \t\r\n\\{}[]$;\"")) return TG_ERROR;
    int old=index_of(u,w,a[3]);
    if(old<0&&w->tab_count==32) return TG_ERROR;
    tg_widget opt=*p;
    if(old<0) tab_defaults(&opt); else if(opt.tab_state==2) opt.tab_state=1;
    if(!options(u,&opt,n-4,a+4)) return TG_ERROR;
    if(old>=0) {
        memmove(w->tabs+old,w->tabs+old+1,(w->tab_count-(unsigned)old-1)*sizeof w->tabs[0]);
        w->tab_count--;
    }
    if((unsigned)pos>w->tab_count) pos=(int)w->tab_count;
    memmove(w->tabs+pos+1,w->tabs+pos,(w->tab_count-(unsigned)pos)*sizeof w->tabs[0]);
    w->tabs[pos]=p->id; w->tab_count++;
    tab_apply(p,&opt);
    tg_id previous=w->selected;
    repair(u,w); changed(u,w,previous);
    return result(out,cap,"");
}
int tg_notebook_command(tg_ui *u,tg_widget *w,size_t n,char **a,char *out,size_t cap) {
    if(w->kind!=TG_NOTEBOOK||n<2) return -1;
    if(!cap) return TG_ERROR;
    if(!strcmp(a[1],"tabs")&&n==2) {
        size_t len=0; out[0]=0;
        for(unsigned k=0;k<w->tab_count;k++) {
            const tg_widget *p=tg_ui_get(u,w->tabs[k]); if(!p) return TG_ERROR;
            size_t z=strlen(p->path); if(len+z+(k?1u:0u)>=cap) return TG_ERROR;
            if(k) out[len++]=' ';
            memcpy(out+len,p->path,z+1); len+=z;
        }
        return TG_OK;
    }
    if(!strcmp(a[1],"select")&&n==2) {
        const tg_widget *p=tg_ui_get(u,w->selected); return result(out,cap,p?p->path:"");
    }
    if(!strcmp(a[1],"insert")) return insert_tab(u,w,n,a,out,cap);
    if(!strcmp(a[1],"add")&&n>=3) {
        tg_widget *p=tg_lookup(u,tg_ui_find(u,a[2]));
        if(!p||p->parent!=w->id||strpbrk(p->path," \t\r\n\\{}[]$;\"")) return TG_ERROR;
        int k=index_of(u,w,a[2]); tg_widget opt=*p;
        if(k<0) { if(w->tab_count==32) return TG_ERROR; tab_defaults(&opt); }
        else if(opt.tab_state==2) opt.tab_state=1;
        if(!options(u,&opt,n-3,a+3)) return TG_ERROR;
        tg_id previous=w->selected;
        if(k<0) w->tabs[w->tab_count++]=p->id;
        tab_apply(p,&opt); repair(u,w); changed(u,w,previous);
        return result(out,cap,"");
    }
    if(!strcmp(a[1],"identify")&&(n==4||n==5)) {
        int x,y; char tail;
        if(sscanf(a[n-2],"%d%c",&x,&tail)!=1||sscanf(a[n-1],"%d%c",&y,&tail)!=1) return TG_ERROR;
        int hit=tab_at(u,w,x,y);
        if(n==5&&!strcmp(a[2],"tab")) {
            if(hit<0) return result(out,cap,"");
            int z=snprintf(out,cap,"%d",hit); return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR;
        }
        if(n==5&&strcmp(a[2],"element")) return TG_ERROR;
        return result(out,cap,hit>=0?"label":"");
    }
    if(n<3) return -1;
    if(!strcmp(a[1],"index")&&n==3&&!strcmp(a[2],"end")) {
        int z=snprintf(out,cap,"%u",w->tab_count); return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR;
    }
    int k=index_of(u,w,a[2]); if(k<0) return -1;
    tg_widget *p=tg_lookup(u,w->tabs[k]); if(!p) return TG_ERROR;
    if(!strcmp(a[1],"bbox")&&n==3) {
        tg_rect box;
        if(!tab_box(u,w,k,&box)) return result(out,cap,"");
        int z=snprintf(out,cap,"%d %d %d %d",box.x-w->rect.x,box.y-w->rect.y,box.w,box.h);
        return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR;
    }
    if(!strcmp(a[1],"index")&&n==3) { int z=snprintf(out,cap,"%d",k); return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR; }
    if(!strcmp(a[1],"select")&&n==3) {
        if(p->tab_state==2) p->tab_state=1;
        if(p->tab_state==1&&w->selected!=p->id) {
            w->selected=p->id;
            (void)tg_ui_post(u,(tg_event){tg_virtual_event("NotebookTabChanged"),w->id,0,0,0});
        }
        return result(out,cap,"");
    }
    if(!strcmp(a[1],"hide")&&n==3) { tg_id previous=w->selected; p->tab_state=2; repair(u,w); changed(u,w,previous); return result(out,cap,""); }
    if(!strcmp(a[1],"forget")&&n==3) { forget(u,w,(unsigned)k); return result(out,cap,""); }
    if(!strcmp(a[1],"tab")&&n>=3) {
        if(n==3) {
            out[0]=0;
            static const char *keys[]={"-text","-state","-padding","-sticky","-image","-compound","-underline"};
            for(unsigned j=0;j<sizeof keys/sizeof keys[0];j++) {
                char buffer[32]; const char *v=tab_value(p,keys[j],buffer);
                if(!tg_list_append(out,cap,keys[j])||!tg_list_append(out,cap,v)) return TG_ERROR;
            }
            return TG_OK;
        }
        if(n==4) {
            char buffer[32]; const char *v=tab_value(p,a[3],buffer);
            return v?result(out,cap,v):TG_ERROR;
        }
        tg_widget opt=*p; if(!options(u,&opt,n-3,a+3)) return TG_ERROR;
        tg_id previous=w->selected; tab_apply(p,&opt); repair(u,w); changed(u,w,previous);
        return result(out,cap,"");
    }
    return -1;
}
static void tab_style(const tg_ui *u,const tg_widget *w,const tg_widget *p,char name[80],char font[384],int pad[4]) {
    snprintf(name,80,"%s.Tab",w->style[0]?w->style:"TNotebook");
    int selected=p->id==w->selected,disabled=p->tab_state==3;
    font[0]=0; pad[0]=pad[2]=8; pad[1]=pad[3]=3;
    char value[384];
    if(tg_style_lookup_value(u,name,"-font",selected,disabled,value,sizeof value)>0)
        snprintf(font,384,"%s",tg_ui_font_spec(u,value));
    if(tg_style_lookup_value(u,name,"-padding",selected,disabled,value,sizeof value)>0) {
        int v[4]={0},n=sscanf(value,"%d %d %d %d",&v[0],&v[1],&v[2],&v[3]);
        if(n>0) {
            for(int k=0;k<n;k++) { if(v[k]<0) v[k]=0; if(v[k]>256) v[k]=256; }
            pad[0]=v[0]; pad[1]=n>1?v[1]:v[0]; pad[2]=n>2?v[2]:v[0]; pad[3]=n>3?v[3]:pad[1];
        }
    }
}
static const tg_photo *tab_image(const tg_ui *u,const tg_widget *p) {
    const tg_image_slot *slot=p->tab_image[0]?tg_image_find(u,p->tab_image):NULL;
    return slot&&strcmp(p->tab_compound,"text")?slot->photo:NULL;
}
static int tab_has_text(const tg_widget *p,const tg_photo *image) {
    return *p->tab_text&&strcmp(p->tab_compound,"image")&&(!image||(*p->tab_compound&&strcmp(p->tab_compound,"none")));
}
static void tab_content(const tg_ui *u,const tg_widget *p,const char *font,int *tw,int *th,int *iw,int *ih,int *width,int *height) {
    const tg_photo *image=tab_image(u,p);
    *tw=*th=*iw=*ih=0;
    if(image) { *iw=tg_photo_width(image); *ih=tg_photo_height(image); }
    if(tab_has_text(p,image)&&(!tg_text_native_enabled()||tg_text_font_measure(p->tab_text,font,tw,th)!=TG_OK))
        (void)tg_text_measure(p->tab_text,1,tw,th);
    *width=*tw>*iw?*tw:*iw; *height=*th>*ih?*th:*ih;
    if(*tw&&*iw&&(!strcmp(p->tab_compound,"left")||!strcmp(p->tab_compound,"right"))) *width=*tw+*iw+3;
    if(*th&&*ih&&(!strcmp(p->tab_compound,"top")||!strcmp(p->tab_compound,"bottom"))) *height=*th+*ih+3;
}
static void tab_measure(const tg_ui *u,const tg_widget *w,const tg_widget *p,int *width,int *height) {
    char name[80],font[384]; int pad[4]; tab_style(u,w,p,name,font,pad);
    int tw,th,iw,ih; tab_content(u,p,font,&tw,&th,&iw,&ih,width,height);
    *width+=pad[0]+pad[2]; *height+=pad[1]+pad[3];
}
int tg_notebook_tab_height(const tg_ui *u,const tg_widget *w) {
    int height=20;
    for(unsigned k=0;k<w->tab_count;k++) {
        const tg_widget *p=tg_ui_get(u,w->tabs[k]); int tw=0,th=0;
        if(!p||p->tab_state==2) continue;
        tab_measure(u,w,p,&tw,&th); if(th>height) height=th;
    }
    return height;
}
static tg_rect tab_rect(const tg_ui *u,const tg_widget *w,const tg_widget *p,int x,int strip_height) {
    int width=0,height=0; tab_measure(u,w,p,&width,&height);
    if(width<40) width=40;
    int remaining=w->rect.x+w->rect.w-x;
    if(width>remaining) width=remaining;
    height=strip_height; if(height>w->rect.h) height=w->rect.h;
    return (tg_rect){x,w->rect.y,width,height};
}
static int tab_box(const tg_ui *u,const tg_widget *w,int index,tg_rect *out) {
    int left=w->rect.x,height=tg_notebook_tab_height(u,w);
    for(unsigned k=0;k<w->tab_count;k++) {
        const tg_widget *p=tg_ui_get(u,w->tabs[k]);
        if(!p||p->tab_state==2) continue;
        tg_rect r=tab_rect(u,w,p,left,height);
        if(r.w<=0||r.h<=0) break;
        if((int)k==index) { *out=r; return 1; }
        left+=r.w;
    }
    return 0;
}
static int tab_at(const tg_ui *u,const tg_widget *w,int x,int y) {
    if(x<0||y<0||x>=w->rect.w||y>=w->rect.h) return -1;
    int left=w->rect.x,height=tg_notebook_tab_height(u,w);
    if(y>=height) return -1;
    for(unsigned k=0;k<w->tab_count;k++) {
        const tg_widget *p=tg_ui_get(u,w->tabs[k]);
        if(!p||p->tab_state==2) continue;
        tg_rect r=tab_rect(u,w,p,left,height);
        if(r.w<=0) break;
        if(x>=r.x-w->rect.x&&x<r.x-w->rect.x+r.w) return (int)k;
        left+=r.w;
    }
    return -1;
}
int tg_notebook_step(tg_ui *u,tg_widget *w,int direction) {
    if(!w||w->kind!=TG_NOTEBOOK||w->state==TG_STATE_DISABLED||!w->tab_count) return 0;
    int old=-1,n=(int)w->tab_count;
    for(int k=0;k<n;k++) if(w->tabs[k]==w->selected) { old=k; break; }
    int at=old<0?(direction>0?n-1:0):old;
    for(int k=0;k<n;k++) {
        at=(at+n+(direction>0?1:-1))%n;
        const tg_widget *page=tg_ui_get(u,w->tabs[at]);
        if(page&&page->tab_state==1) {
            tg_id previous=w->selected; w->selected=page->id;
            changed(u,w,previous); return 1;
        }
    }
    return 0;
}
static tg_id top_owner(const tg_ui *u,const tg_widget *w) {
    while(w&&w->id!=1&&w->kind!=TG_TOPLEVEL) w=tg_ui_get(u,w->parent);
    return w?w->id:0;
}
static uint32_t fold_mnemonic(uint32_t key) {
    /* Locale-independent ASCII case folding; non-ASCII exact matching. */
    return key>='A'&&key<='Z'?key+('a'-'A'):key;
}
static int mnemonic(const tg_widget *p,uint32_t key) {
    if(p->tab_underline<0) return 0;
    size_t len=strlen(p->tab_text),at=0,used; uint32_t scalar;
    for(int k=0;k<p->tab_underline;k++) {
        if(at>=len||tg_utf8_decode((const unsigned char *)p->tab_text+at,len-at,&scalar,&used)!=TG_OK) return 0;
        at+=used;
    }
    return at<len&&tg_utf8_decode((const unsigned char *)p->tab_text+at,len-at,&scalar,&used)==TG_OK&&
        fold_mnemonic(scalar)==fold_mnemonic(key);
}
int tg_notebook_traverse(tg_ui *u,uint32_t key,unsigned modifiers) {
    const tg_widget *focus=tg_ui_get(u,u->focus);
    if(key==TG_KEY_TAB&&(modifiers&TG_MOD_CONTROL)) {
        for(const tg_widget *p=focus;p;p=tg_ui_get(u,p->parent)) {
            if(p->kind==TG_NOTEBOOK&&p->notebook_traversal&&p->state!=TG_STATE_DISABLED) {
                tg_widget *notebook=tg_lookup(u,p->id);
                if(tg_notebook_step(u,notebook,modifiers&TG_MOD_SHIFT?-1:1)) {
                    u->focus=notebook->id; return 1;
                }
            }
            if(p->id==1||p->kind==TG_TOPLEVEL) break;
        }
    }
    if((modifiers&TG_MOD_ALT)&&!(modifiers&TG_MOD_CONTROL)&&key<=0x10ffff) {
        tg_id root=focus?top_owner(u,focus):u->view_root;
        if(!root) root=1;
        for(size_t k=0;k<u->count;k++) {
            tg_widget *w=&u->items[k];
            if(w->kind!=TG_NOTEBOOK||!w->notebook_traversal||w->state==TG_STATE_DISABLED||
               w->rect.w<=0||w->rect.h<=0||top_owner(u,w)!=root) continue;
            for(unsigned j=0;j<w->tab_count;j++) {
                const tg_widget *page=tg_ui_get(u,w->tabs[j]);
                if(page&&page->tab_state==1&&mnemonic(page,key)) {
                    tg_id previous=w->selected; w->selected=page->id;
                    u->focus=w->id; changed(u,w,previous); return 1;
                }
            }
        }
    }
    return 0;
}
int tg_notebook_click(tg_ui *u,tg_widget *w,int x,int y) {
    if(w->kind!=TG_NOTEBOOK||w->state==TG_STATE_DISABLED||y<w->rect.y||y>=w->rect.y+tg_notebook_tab_height(u,w)||y>=w->rect.y+w->rect.h) return 0;
    int left=w->rect.x,strip_height=tg_notebook_tab_height(u,w);
    for(unsigned k=0;k<w->tab_count;k++) {
        const tg_widget *p=tg_ui_get(u,w->tabs[k]); if(!p||p->tab_state==2) continue;
        tg_rect r=tab_rect(u,w,p,left,strip_height); left+=r.w;
        if(x>=r.x&&x<left&&p->tab_state==1) {
            if(w->takefocus!=0) u->focus=w->id;
            if(w->selected!=p->id) {
                w->selected=p->id;
                (void)tg_ui_post(u,(tg_event){tg_virtual_event("NotebookTabChanged"),w->id,0,0,0});
            }
            return 1;
        }
    }
    return 0;
}
void tg_notebook_paint(const tg_ui *u,const tg_widget *w,tg_canvas *c,uint32_t fg) {
    int left=w->rect.x,strip_height=tg_notebook_tab_height(u,w);
    for(unsigned k=0;k<w->tab_count;k++) {
        const tg_widget *p=tg_ui_get(u,w->tabs[k]); if(!p||p->tab_state==2) continue;
        tg_rect r=tab_rect(u,w,p,left,strip_height); if(r.w<=0||r.h<=0) break;
        int selected=p->id==w->selected,disabled=p->tab_state==3;
        char name[80],font[384]; int pad[4]; tab_style(u,w,p,name,font,pad);
        uint32_t bg=selected?0x397bc4:0xe0e0e0,ink=fg,border=0x61768c;
        (void)tg_style_lookup_color(u,name,"-background",selected,disabled,&bg);
        (void)tg_style_lookup_color(u,name,"-foreground",selected,disabled,&ink);
        (void)tg_style_lookup_color(u,name,"-bordercolor",selected,disabled,&border);
        tg_raster_fill(c,w->rect,r,bg);
        tg_raster_outline(c,w->rect,r,border);
        int tw,th,iw,ih,cw,ch;
        tab_content(u,p,font,&tw,&th,&iw,&ih,&cw,&ch);
        int tx=r.x+pad[0],ty=r.y+(r.h-th)/2,ix=tx+(cw-iw)/2,iy=r.y+(r.h-ih)/2;
        if(iw&&tw) {
            if(!strcmp(p->tab_compound,"left")) { ix=tx; tx+=iw+3; }
            else if(!strcmp(p->tab_compound,"right")) ix=tx+tw+3;
            else if(!strcmp(p->tab_compound,"top")) { iy=r.y+(r.h-ch)/2; ty=iy+ih+3; tx+=(cw-tw)/2; }
            else if(!strcmp(p->tab_compound,"bottom")) { ty=r.y+(r.h-ch)/2; iy=ty+th+3; tx+=(cw-tw)/2; }
        }
        const tg_photo *image=tab_image(u,p);
        if(image) tg_photo_paint(image,c,r,ix,iy);
        int shaped=tw&&tg_text_native_enabled()&&tg_text_font_draw(c,r,tx,ty,p->tab_text,font,ink)==TG_OK;
        if(tw&&!shaped) tg_text_draw_row(c,r,tx,p->tab_text,ink);
        if(tw&&p->tab_underline>=0) {
            size_t len=strlen(p->tab_text),at=0,used=0; uint32_t scalar;
            for(int k=0;k<p->tab_underline&&at<len;k++) {
                if(tg_utf8_decode((const unsigned char *)p->tab_text+at,len-at,&scalar,&used)!=TG_OK) { at=len; break; }
                at+=used;
            }
            if(at<len&&tg_utf8_decode((const unsigned char *)p->tab_text+at,len-at,&scalar,&used)==TG_OK) {
                int a=p->tab_underline*6,b=a+5;
                if(!shaped||tg_text_font_caret(p->tab_text,font,at,&a)!=TG_OK||
                   tg_text_font_caret(p->tab_text,font,at+used,&b)!=TG_OK) { a=p->tab_underline*6; b=a+5; }
                int lo=a<b?a:b,hi=a<b?b:a;
                if(hi>lo) tg_raster_fill(c,r,(tg_rect){tx+lo,ty+th-1,hi-lo,1},ink);
            }
        }
        left+=r.w;
    }
}
