#include "TKGPUTree.H"
#include "../TKGPUStyle.H"
#include "../../../core/TKGPUList.H"
#include "TKGPURaster.H"
#include "TKGPUText.H"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
enum { LIMIT=1024, COLS=32, TAG_LIMIT=64 };
typedef struct { char id[64],text[256],values[1024],tags[256]; unsigned parent,child,next,prev; int open,selected; } node;
typedef struct { char name[64]; int has_bg,has_fg; uint32_t bg,fg; char font[128]; } tree_tag;
struct tg_tree {
    node *rows;
    unsigned capacity,count,serial,focus;
    char columns[COLS][64],heading[COLS+1][128];
    unsigned nc;
    int width[COLS+1],minwidth[COLS+1];
    int drag_column,drag_x,drag_width;
    unsigned char col_anchor[COLS+1],head_anchor[COLS+1],col_stretch[COLS+1];
    char column_list[1024];
    int show_tree,show_head;
    int scroll_y,scroll_x;
    tree_tag tags[TAG_LIMIT];
    unsigned tag_count;
};
tg_tree *tg_tree_create(void) {
    tg_tree *t=calloc(1,sizeof *t); if(!t) return NULL;
    t->rows=calloc(16,sizeof *t->rows); if(!t->rows) { free(t); return NULL; }
    t->capacity=16; t->serial=1; t->show_tree=t->show_head=1;
    for(unsigned k=0;k<=COLS;k++) { t->width[k]=100; t->minwidth[k]=20; t->col_stretch[k]=1; }
    return t;
}
void tg_tree_destroy(tg_tree *t) { if(t) { free(t->rows); free(t); } }
tg_tree *tg_tree_clone(const tg_tree *t) {
    if(!t) return NULL;
    tg_tree *c=malloc(sizeof *c); if(!c) return NULL;
    *c=*t; c->rows=malloc(t->capacity*sizeof *c->rows);
    if(!c->rows) { free(c); return NULL; }
    memcpy(c->rows,t->rows,t->capacity*sizeof *c->rows); return c;
}
static int result(char *out,size_t cap,const char *s) { if(strlen(s)>=cap) return TG_ERROR; strcpy(out,s); return TG_OK; }
static int safe(const char *s,size_t cap) { return *s&&strlen(s)<cap&&!strpbrk(s," \t\r\n\v\f\\{}[]$;\""); }
static int find(const tg_tree *t,const char *s) {
    if(!*s) return 0;
    for(unsigned k=1;k<t->capacity;k++) if(!strcmp(t->rows[k].id,s)) return (int)k;
    return -1;
}
static int integer(const char *s,int *v) { char *end; errno=0; long x=strtol(s,&end,10); if(errno||end==s||*end||x<0||x>16384) return 0; *v=(int)x; return 1; }
static int boolean(const char *s,int *v) {
    if(!strcmp(s,"1")||!strcmp(s,"true")||!strcmp(s,"yes")||!strcmp(s,"on")) *v=1;
    else if(!strcmp(s,"0")||!strcmp(s,"false")||!strcmp(s,"no")||!strcmp(s,"off")) *v=0;
    else return 0;
    return 1;
}
static int list_ok(const char *s) { char buf[1024]; int rc; if(strlen(s)>=1024) return 0; while((rc=tg_list_next(&s,buf,sizeof buf))>0) {} return rc==0; }
static unsigned char parse_anchor(const char *s) {
    if(!s||!*s) return 0;
    if(!strcmp(s,"center")||!strcmp(s,"c")||!strcmp(s,"n")||!strcmp(s,"s")) return 1;
    if(!strcmp(s,"e")||!strcmp(s,"ne")||!strcmp(s,"se")) return 2;
    return 0;
}
static int hex_digit(char c) {
    if(c>='0'&&c<='9') return c-'0';
    if(c>='a'&&c<='f') return c-'a'+10;
    if(c>='A'&&c<='F') return c-'A'+10;
    return -1;
}
static int parse_tree_color(const char *s,uint32_t *out) {
    if(!s||!*s) return 0;
    if(s[0]=='#') {
        size_t len=strlen(s);
        if(len==7||len==9) {
            uint32_t v=0;
            for(int k=1;k<7;k++) { int d=hex_digit(s[k]); if(d<0) return 0; v=(v<<4)|(unsigned)d; }
            *out=v; return 1;
        }
        if(len==4) {
            int r=hex_digit(s[1]),g=hex_digit(s[2]),b=hex_digit(s[3]);
            if(r<0||g<0||b<0) return 0;
            *out=((uint32_t)(r*17)<<16)|((uint32_t)(g*17)<<8)|(uint32_t)(b*17);
            return 1;
        }
        return 0;
    }
    char lo[32]; size_t n=strlen(s); if(n>=sizeof lo) return 0;
    for(size_t k=0;k<=n;k++) lo[k]=(char)tolower((unsigned char)s[k]);
    if(!strcmp(lo,"white")) { *out=0xffffff; return 1; }
    if(!strcmp(lo,"black")) { *out=0x000000; return 1; }
    if(!strcmp(lo,"red")) { *out=0xff0000; return 1; }
    if(!strcmp(lo,"green")) { *out=0x008000; return 1; }
    if(!strcmp(lo,"blue")) { *out=0x0000ff; return 1; }
    if(!strcmp(lo,"gray")||!strcmp(lo,"grey")) { *out=0x808080; return 1; }
    if(!strcmp(lo,"yellow")) { *out=0xffff00; return 1; }
    if(!strcmp(lo,"orange")) { *out=0xffa500; return 1; }
    if(!strcmp(lo,"cyan")) { *out=0x00ffff; return 1; }
    if(!strcmp(lo,"magenta")) { *out=0xff00ff; return 1; }
    return 0;
}
static int tree_row_height(const tg_ui *u,const tg_widget *w) {
    int rh=tg_text_row_height(24);
    if(u&&w) {
        const char *style=w->style[0]?w->style:"Treeview";
        char val[64];
        if(tg_style_lookup_value(u,style,"-rowheight",0,0,val,sizeof val)>0) {
            int parsed=0;
            if(integer(val,&parsed)&&parsed>=14&&parsed<=120) rh=parsed;
        }
    }
    return rh;
}
static int tree_head_height(int row_h) {
    return row_h>=24?row_h:26;
}
static uint32_t shift_color(uint32_t c,int delta) {
    int r=(int)((c>>16)&255u)+delta; if(r<0) r=0; if(r>255) r=255;
    int g=(int)((c>>8)&255u)+delta; if(g<0) g=0; if(g>255) g=255;
    int b=(int)(c&255u)+delta; if(b<0) b=0; if(b>255) b=255;
    return ((uint32_t)r<<16)|((uint32_t)g<<8)|(uint32_t)b;
}
static uint32_t blend_color(uint32_t a,uint32_t b,unsigned weight_b) {
    unsigned wa=255u-weight_b;
    unsigned r=(((a>>16)&255u)*wa+((b>>16)&255u)*weight_b+127u)/255u;
    unsigned g=(((a>>8)&255u)*wa+((b>>8)&255u)*weight_b+127u)/255u;
    unsigned bl=((a&255u)*wa+(b&255u)*weight_b+127u)/255u;
    return (r<<16)|(g<<8)|bl;
}
static int is_dark_color(uint32_t c) {
    unsigned lum=((c>>16)&255u)*299u+((c>>8)&255u)*587u+(c&255u)*114u;
    return lum<128000u;
}
static tree_tag *find_tag(tg_tree *t,const char *name,int create) {
    for(unsigned k=0;k<t->tag_count;k++) if(!strcmp(t->tags[k].name,name)) return &t->tags[k];
    if(!create||t->tag_count>=TAG_LIMIT) return NULL;
    tree_tag *tag=&t->tags[t->tag_count++];
    memset(tag,0,sizeof *tag);
    snprintf(tag->name,sizeof tag->name,"%s",name);
    return tag;
}
static const tree_tag *find_tag_const(const tg_tree *t,const char *name) {
    for(unsigned k=0;k<t->tag_count;k++) if(!strcmp(t->tags[k].name,name)) return &t->tags[k];
    return NULL;
}
int tg_tree_option(tg_tree *t,const char *key,const char *s) {
    if(!strcmp(key,"-columns")) {
        char cols[COLS][64]={{0}}; unsigned n=0; char item[64]; const char *p=s; int rc;
        if(strlen(s)>=sizeof t->column_list) return TG_ERROR;
        while((rc=tg_list_next(&p,item,sizeof item))>0) {
            if(n==COLS||!safe(item,sizeof item)||*item=='#') return TG_ERROR;
            for(unsigned k=0;k<n;k++) if(!strcmp(cols[k],item)) return TG_ERROR;
            strcpy(cols[n++],item);
        }
        if(rc<0) return TG_ERROR;
        memcpy(t->columns,cols,sizeof cols); t->nc=n; strcpy(t->column_list,s); return TG_OK;
    }
    if(!strcmp(key,"-show")) {
        int tree=0,head=0,rc; char item[32];
        while((rc=tg_list_next(&s,item,sizeof item))>0) { if(!strcmp(item,"tree")) tree=1; else if(!strcmp(item,"headings")) head=1; else return TG_ERROR; }
        if(rc<0) return TG_ERROR;
        t->show_tree=tree; t->show_head=head; return TG_OK;
    }
    if(!strcmp(key,"-selectmode")||!strcmp(key,"-displaycolumns")||!strcmp(key,"-padding")||
       !strcmp(key,"-takefocus")||!strcmp(key,"-height")) {
        return TG_OK;
    }
    return -1;
}
static unsigned alloc_row(tg_tree *t) {
    for(unsigned k=1;k<t->capacity;k++) if(!t->rows[k].id[0]) return k;
    if(t->capacity==LIMIT+1) return 0;
    unsigned old=t->capacity,n=old*2; if(n>LIMIT+1) n=LIMIT+1;
    node *p=realloc(t->rows,n*sizeof *p); if(!p) return 0;
    memset(p+old,0,(n-old)*sizeof *p); t->rows=p; t->capacity=n; return old;
}
static int options(node *r,size_t n,char **a) {
    if(n%2) return 0;
    for(size_t k=0;k<n;k+=2) {
        if(!strcmp(a[k],"-text")) snprintf(r->text,sizeof r->text,"%s",a[k+1]);
        else if(!strcmp(a[k],"-values")&&list_ok(a[k+1])) strcpy(r->values,a[k+1]);
        else if(!strcmp(a[k],"-open")&&boolean(a[k+1],&r->open)) {}
        else if(!strcmp(a[k],"-tags")) snprintf(r->tags,sizeof r->tags,"%s",a[k+1]);
        else if(!strcmp(a[k],"-image")) {}
        else return 0;
    }
    return 1;
}
static void link_row(tg_tree *t,unsigned k,unsigned parent,int index) {
    unsigned at=t->rows[parent].child,prev=0;
    while(at&&index!=0) { prev=at; at=t->rows[at].next; if(index>0) index--; }
    node *r=&t->rows[k]; r->parent=parent; r->prev=prev; r->next=at;
    if(prev) t->rows[prev].next=k; else t->rows[parent].child=k;
    if(at) t->rows[at].prev=k;
}
static void erase(tg_tree *t,unsigned k) {
    node *r=&t->rows[k]; while(r->child) erase(t,r->child);
    if(r->prev) t->rows[r->prev].next=r->next; else t->rows[r->parent].child=r->next;
    if(r->next) t->rows[r->next].prev=r->prev;
    if(t->focus==k) t->focus=0;
    memset(r,0,sizeof *r); t->count--;
}
static int append_id(char *out,size_t cap,const char *s) {
    size_t n=strlen(out),z=strlen(s); if(n+z+(n?1:0)>=cap) return 0;
    if(n) out[n++]=' ';
    memcpy(out+n,s,z+1); return 1;
}
static int column(const tg_tree *t,const char *s) {
    int i; if(*s=='#'&&integer(s+1,&i)&&i<=(int)t->nc) return i;
    for(unsigned k=0;k<t->nc;k++) if(!strcmp(t->columns[k],s)) return (int)k+1;
    return -1;
}
static unsigned advance(const tg_tree *t,unsigned k,unsigned *depth);
static int count_visible_rows(const tg_tree *t) {
    int total=0; unsigned depth=0;
    for(unsigned k=t->rows[0].child;k;k=advance(t,k,&depth)) total++;
    return total;
}
static int total_tree_width(const tg_tree *t) {
    int total=0;
    for(unsigned k=t->show_tree?0:1;k<=t->nc;k++) total+=t->width[k];
    return total;
}
static void compute_col_widths(const tg_tree *t,int avail_w,int out_w[COLS+1]) {
    unsigned start=t->show_tree?0:1;
    int total=0,stretch_count=0;
    for(unsigned k=0;k<=COLS;k++) out_w[k]=t->width[k];
    for(unsigned k=start;k<=t->nc;k++) {
        total+=out_w[k];
        if(t->col_stretch[k]) stretch_count++;
    }
    if(avail_w>total&&stretch_count>0) {
        int extra=avail_w-total;
        int share=extra/stretch_count;
        int rem=extra%stretch_count;
        for(unsigned k=start;k<=t->nc;k++) {
            if(t->col_stretch[k]) {
                out_w[k]+=share+(rem>0?1:0);
                if(rem>0) rem--;
            }
        }
    }
}
void tg_tree_view_fractions(const tg_ui *u,const tg_widget *w,int y,double *first,double *last) {
    *first=0.0; *last=1.0;
    if(!w||w->kind!=TG_TREEVIEW||!w->tree) return;
    const tg_tree *t=w->tree;
    if(y) {
        int row_h=tree_row_height(u,w);
        int head_h=t->show_head?tree_head_height(row_h):0;
        int total=count_visible_rows(t);
        int body_h=w->rect.h-head_h;
        int vis=body_h>0?(body_h/row_h):10; if(vis<1) vis=1;
        if(total>0) {
            *first=(double)t->scroll_y/total;
            *last=(double)(t->scroll_y+vis)/total;
            if(*first<0.0) *first=0.0; if(*first>1.0) *first=1.0;
            if(*last<*first) *last=*first; if(*last>1.0) *last=1.0;
        }
    } else {
        int total=total_tree_width(t);
        int vis=w->rect.w>0?w->rect.w:300;
        if(total>0) {
            *first=(double)t->scroll_x/total;
            *last=(double)(t->scroll_x+vis)/total;
            if(*first<0.0) *first=0.0; if(*first>1.0) *first=1.0;
            if(*last<*first) *last=*first; if(*last>1.0) *last=1.0;
        }
    }
}
/* Match the viewport traversal used by tg_tree_paint. Coordinates here
   are widget-local, not absolute screen coordinates. */
static int signed_coord(const char *s,int *out) {
    char *end; errno=0; long v=strtol(s,&end,10);
    if(errno||end==s||*end||v< -1000000||v>1000000) return 0;
    *out=(int)v; return 1;
}
static int visible_index(const tg_tree *t,unsigned target) {
    int row=0; unsigned depth=0;
    for(unsigned k=t->rows[0].child;k;k=advance(t,k,&depth),row++)
        if(k==target) return row;
    return -1;
}
static unsigned row_at(const tg_tree *t,int target,unsigned *depth) {
    *depth=0;
    unsigned k=t->rows[0].child;
    for(int j=0;k&&j<target;j++) k=advance(t,k,depth);
    return k;
}
static int column_at(const tg_tree *t,const int widths[COLS+1],int x) {
    int pos=-t->scroll_x;
    for(unsigned j=t->show_tree?0:1;j<=t->nc;j++) {
        if(x>=pos&&x<pos+widths[j]) return (int)j;
        pos+=widths[j];
    }
    return -1;
}
/* Cell values stay literal Tcl-compatible list elements. A failed edit
   leaves the original node untouched. */
static int cell_command(tg_tree *t,node *r,size_t n,char **a,char *out,size_t cap) {
    if(n<3||n>5) return TG_ERROR;
    int target=n>=4?column(t,a[3]):-1;
    if(n>=4&&target<=0) return TG_ERROR;
    const char *cursor=r->values;
    char value[1024],replacement[sizeof r->values]="";
    if(n==3) {
        for(unsigned j=1;j<=t->nc;j++) {
            int rc=tg_list_next(&cursor,value,sizeof value);
            if(rc<0) return TG_ERROR;
            if(!rc) value[0]=0;
            if(!tg_list_append(out,cap,t->columns[j-1])||!tg_list_append(out,cap,value)) return TG_ERROR;
        }
        return TG_OK;
    }
    for(unsigned j=1;;j++) {
        int rc=tg_list_next(&cursor,value,sizeof value);
        if(rc<0) return TG_ERROR;
        if(!rc&&j>(unsigned)target) break;
        if(!rc) value[0]=0;
        if(n==4&&j==(unsigned)target) return result(out,cap,value);
        if(n==5&&!tg_list_append(replacement,sizeof replacement,j==(unsigned)target?a[4]:value))
            return TG_ERROR;
    }
    if(n!=5) return TG_ERROR;
    strcpy(r->values,replacement);
    return TG_OK;
}
static int move_row(tg_tree *t,unsigned k,const char *parent_name,const char *index_name) {
    int parent=find(t,parent_name),index=-1;
    if(parent<0||(strcmp(index_name,"end")&&!integer(index_name,&index))) return TG_ERROR;
    /* Never allow a node to become its own ancestor. */
    for(unsigned p=(unsigned)parent;p;p=t->rows[p].parent) if(p==k) return TG_ERROR;
    node *r=&t->rows[k];
    if(r->prev) t->rows[r->prev].next=r->next;
    else t->rows[r->parent].child=r->next;
    if(r->next) t->rows[r->next].prev=r->prev;
    link_row(t,k,(unsigned)parent,index);
    return TG_OK;
}
static int viewport_command(const tg_ui *u,tg_widget *w,size_t n,char **a,char *out,size_t cap) {
    tg_tree *t=w->tree;
    int rh=tree_row_height(u,w),hh=t->show_head?tree_head_height(rh):0;
    int body=w->rect.h-hh;
    if(!strcmp(a[1],"see")) {
        if(n!=3) return TG_ERROR;
        int k=find(t,a[2]); if(k<=0) return TG_ERROR;
        for(unsigned p=t->rows[k].parent;p;p=t->rows[p].parent) t->rows[p].open=1;
        int at=visible_index(t,(unsigned)k);
        if(at<0) return TG_ERROR;
        int vis=body/rh; if(vis<1) vis=1;
        if(at<t->scroll_y) t->scroll_y=at;
        else if(at>=t->scroll_y+vis) t->scroll_y=at-vis+1;
        int max=count_visible_rows(t)-vis;
        if(max<0) max=0;
        if(t->scroll_y>max) t->scroll_y=max;
        return TG_OK;
    }
    int widths[COLS+1]; compute_col_widths(t,w->rect.w,widths);
    if(!strcmp(a[1],"bbox")) {
        if(n!=3&&n!=4) return TG_ERROR;
        int k=find(t,a[2]); if(k<=0) return TG_ERROR;
        int row=visible_index(t,(unsigned)k);
        if(row<0||body<=0||row<t->scroll_y) return TG_OK;
        int y=hh+(row-t->scroll_y)*rh;
        if(y>=w->rect.h) return TG_OK;
        int x=-t->scroll_x,width=0;
        unsigned start=t->show_tree?0:1;
        if(n==4&&a[3][0]) {
            int col=column(t,a[3]);
            if(col<0) return TG_ERROR;
            if(col<(int)start) return TG_OK;
            for(unsigned j=start;j<(unsigned)col;j++) x+=widths[j];
            width=widths[col];
        } else {
            for(unsigned j=start;j<=t->nc;j++) width+=widths[j];
        }
        if(width<=0||x+width<=0||x>=w->rect.w) return TG_OK;
        int z=snprintf(out,cap,"%d %d %d %d",x,y,width,rh);
        return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR;
    }
    if(!strcmp(a[1],"identify")) {
        if(n!=5) return TG_ERROR;
        int x,y;
        if(!signed_coord(a[3],&x)||!signed_coord(a[4],&y)) return TG_ERROR;
        const char *what=a[2];
        if(strcmp(what,"row")&&strcmp(what,"column")&&strcmp(what,"region")&&strcmp(what,"element"))
            return TG_ERROR;
        if(x<0||x>=w->rect.w||y<0||y>=w->rect.h) return TG_OK;
        int col=column_at(t,widths,x);
        if(!strcmp(what,"column")) {
            if(col<0) return TG_OK;
            int z=snprintf(out,cap,"#%d",col); return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR;
        }
        unsigned depth=0,k=y>=hh?row_at(t,t->scroll_y+(y-hh)/rh,&depth):0;
        if(!strcmp(what,"row")) return k?result(out,cap,t->rows[k].id):TG_OK;
        if(!strcmp(what,"region")) {
            if(col<0) return result(out,cap,"nothing");
            if(y<hh) {
                int edge=-t->scroll_x;
                for(unsigned j=t->show_tree?0:1;j<=(unsigned)col;j++) edge+=widths[j];
                return result(out,cap,edge-x<=3?"separator":"heading");
            }
            return result(out,cap,k?(col==0?"tree":"cell"):"nothing");
        }
        if(y<hh&&col>=0) return result(out,cap,"Treeheading.text");
        if(!k||col<0) return TG_OK;
        if(col==0&&t->rows[k].child) {
            int indent=-t->scroll_x+(int)depth*12;
            if(x>=indent&&x<indent+14) return result(out,cap,"Treeitem.indicator");
        }
        return result(out,cap,col==0?"Treeitem.text":"Treeitem.text");
    }
    return -1;
}
int tg_tree_command(const tg_ui *u,tg_widget *w,size_t n,char **a,char *out,size_t cap) {
    if(w->kind!=TG_TREEVIEW||n<2) return -1;
    tg_tree *t=w->tree; if(!t||!cap) return TG_ERROR;
    const char *op=a[1]; out[0]=0;
    if(!strcmp(op,"cget")&&n==3) {
        if(!strcmp(a[2],"-columns")) return result(out,cap,t->column_list);
        if(!strcmp(a[2],"-show")) return result(out,cap,t->show_tree?(t->show_head?"tree headings":"tree"):(t->show_head?"headings":""));
        return -1;
    }
    if(!strcmp(op,"insert")&&n>=4&&n%2==0) {
        int parent=find(t,a[2]),index=-1; if(parent<0||(strcmp(a[3],"end")&&!integer(a[3],&index))) return TG_ERROR;
        node r={0}; size_t pos=4;
        if(n>=6&&!strcmp(a[4],"-id")) { if(!safe(a[5],sizeof r.id)||find(t,a[5])>=0) return TG_ERROR; strcpy(r.id,a[5]); pos=6; }
        else { do { if(t->serial==UINT32_MAX) return TG_ERROR; snprintf(r.id,sizeof r.id,"I%03X",t->serial++); } while(find(t,r.id)>=0); }
        if(!options(&r,n-pos,a+pos)||strlen(r.id)>=cap) return TG_ERROR;
        unsigned k=alloc_row(t); if(!k) return TG_ERROR;
        t->rows[k]=r; link_row(t,k,(unsigned)parent,index); t->count++; return result(out,cap,r.id);
    }
    if(!strcmp(op,"selection")) {
        if(n==2) { for(unsigned k=1;k<t->capacity;k++) if(t->rows[k].selected&&!append_id(out,cap,t->rows[k].id)) return TG_ERROR; return TG_OK; }
        if(n==4&&(!strcmp(a[2],"set")||!strcmp(a[2],"add")||!strcmp(a[2],"remove")||!strcmp(a[2],"toggle"))) {
            int is_set=!strcmp(a[2],"set"),is_add=!strcmp(a[2],"add"),is_rem=!strcmp(a[2],"remove");
            unsigned char selected[LIMIT+1]={0}; const char *p=a[3]; char id[64]; int rc;
            while((rc=tg_list_next(&p,id,sizeof id))>0) { int k=find(t,id); if(k<=0) return TG_ERROR; selected[k]=1; }
            if(rc<0) return TG_ERROR;
            for(unsigned k=1;k<t->capacity;k++) {
                if(is_set) t->rows[k].selected=selected[k];
                else if(is_add&&selected[k]) t->rows[k].selected=1;
                else if(is_rem&&selected[k]) t->rows[k].selected=0;
                else if(!is_set&&!is_add&&!is_rem&&selected[k]) t->rows[k].selected=!t->rows[k].selected;
            }
            return TG_OK;
        }
        return TG_ERROR;
    }
    if(!strcmp(op,"delete")) {
        for(size_t j=2;j<n;j++) {
            const char *p=a[j]; char id[64]; int rc;
            while((rc=tg_list_next(&p,id,sizeof id))>0) {
                int k=find(t,id);
                if(k>0) erase(t,(unsigned)k);
            }
            if(rc<0) return TG_ERROR;
        }
        return TG_OK;
    }
    if(!strcmp(op,"children")&&n==2) {
        for(unsigned j=t->rows[0].child;j;j=t->rows[j].next) if(!append_id(out,cap,t->rows[j].id)) return TG_ERROR;
        return TG_OK;
    }
    if(!strcmp(op,"yview")) {
        int row_h=tree_row_height(u,w);
        int head_h=t->show_head?tree_head_height(row_h):0;
        int total=count_visible_rows(t);
        int body_h=w->rect.h-head_h;
        int vis=body_h>0?(body_h/row_h):10; if(vis<1) vis=1;
        int max_scroll=total>vis?total-vis:0;
        if(n==2) {
            double first=total>0?(double)t->scroll_y/total:0.0;
            double last=total>0?(double)(t->scroll_y+vis)/total:1.0;
            if(last>1.0) last=1.0;
            int z=snprintf(out,cap,"%.9g %.9g",first,last);
            return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR;
        }
        if(n==4&&!strcmp(a[2],"moveto")) {
            double f=strtod(a[3],NULL);
            if(f<0.0) f=0.0; if(f>1.0) f=1.0;
            t->scroll_y=(int)(f*total+0.5);
        } else if(n==5&&!strcmp(a[2],"scroll")) {
            int amt=(int)strtol(a[3],NULL,10);
            int step=(!strcmp(a[4],"pages")||!strcmp(a[4],"page"))?vis:1;
            t->scroll_y+=amt*step;
        }
        if(t->scroll_y<0) t->scroll_y=0;
        if(t->scroll_y>max_scroll) t->scroll_y=max_scroll;
        return TG_OK;
    }
    if(!strcmp(op,"xview")) {
        int total=total_tree_width(t);
        int vis=w->rect.w>0?w->rect.w:300;
        int max_scroll=total>vis?total-vis:0;
        if(n==2) {
            double first=total>0?(double)t->scroll_x/total:0.0;
            double last=total>0?(double)(t->scroll_x+vis)/total:1.0;
            if(last>1.0) last=1.0;
            int z=snprintf(out,cap,"%.9g %.9g",first,last);
            return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR;
        }
        if(n==4&&!strcmp(a[2],"moveto")) {
            double f=strtod(a[3],NULL);
            if(f<0.0) f=0.0; if(f>1.0) f=1.0;
            t->scroll_x=(int)(f*total+0.5);
        } else if(n==5&&!strcmp(a[2],"scroll")) {
            int amt=(int)strtol(a[3],NULL,10);
            int step=(!strcmp(a[4],"pages")||!strcmp(a[4],"page"))?vis:24;
            t->scroll_x+=amt*step;
        }
        if(t->scroll_x<0) t->scroll_x=0;
        if(t->scroll_x>max_scroll) t->scroll_x=max_scroll;
        return TG_OK;
    }
    if(!strcmp(op,"tag")) {
        if(n>=4&&!strcmp(a[2],"configure")) {
            tree_tag *tag=find_tag(t,a[3],n>4);
            if(n==4) return TG_OK;
            if(n==5) {
                if(!tag) return TG_OK;
                if(!strcmp(a[4],"-background")&&tag->has_bg) {
                    int z=snprintf(out,cap,"#%06x",tag->bg);
                    return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR;
                }
                if(!strcmp(a[4],"-foreground")&&tag->has_fg) {
                    int z=snprintf(out,cap,"#%06x",tag->fg);
                    return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR;
                }
                if(!strcmp(a[4],"-font")) return result(out,cap,tag->font);
                return TG_OK;
            }
            if(!tag) return TG_OK;
            for(size_t j=4;j+1<n;j+=2) {
                if(!strcmp(a[j],"-background")) {
                    if(!a[j+1][0]) tag->has_bg=0;
                    else if(parse_tree_color(a[j+1],&tag->bg)) tag->has_bg=1;
                } else if(!strcmp(a[j],"-foreground")) {
                    if(!a[j+1][0]) tag->has_fg=0;
                    else if(parse_tree_color(a[j+1],&tag->fg)) tag->has_fg=1;
                } else if(!strcmp(a[j],"-font")) {
                    const char *spec=(u&&a[j+1][0])?tg_ui_font_spec(u,a[j+1]):a[j+1];
                    snprintf(tag->font,sizeof tag->font,"%s",spec?spec:"");
                }
            }
            return TG_OK;
        }
        if(n==3&&!strcmp(a[2],"names")) {
            for(unsigned k=0;k<t->tag_count;k++) if(!append_id(out,cap,t->tags[k].name)) return TG_ERROR;
            return TG_OK;
        }
        return TG_OK;
    }
    if(!strcmp(op,"see")||!strcmp(op,"identify")||!strcmp(op,"bbox"))
        return viewport_command(u,w,n,a,out,cap);
    if(!strcmp(op,"focus")&&n==2) return result(out,cap,t->rows[t->focus].id);
    if((!strcmp(op,"heading")||!strcmp(op,"column"))&&n>=3) {
        int k=column(t,a[2]); if(k<0) return TG_ERROR;
        if(n==3) return TG_OK;
        if(n==4) {
            if(!strcmp(op,"heading")&&!strcmp(a[3],"-text")) return result(out,cap,t->heading[k]);
            if(!strcmp(op,"column")&&!strcmp(a[3],"-minwidth")) { int z=snprintf(out,cap,"%d",t->minwidth[k]); return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR; }
            if(!strcmp(op,"column")&&!strcmp(a[3],"-width")) { int z=snprintf(out,cap,"%d",t->width[k]); return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR; }
            return TG_OK;
        }
        if(n%2==0) return TG_ERROR;
        for(size_t j=3;j<n;j+=2) {
            if(!strcmp(op,"heading")) {
                if(!strcmp(a[j],"-text")) snprintf(t->heading[k],sizeof t->heading[k],"%s",a[j+1]);
                else if(!strcmp(a[j],"-anchor")) t->head_anchor[k]=parse_anchor(a[j+1]);
                else if(!strcmp(a[j],"-command")||!strcmp(a[j],"-image")) {}
                else return TG_ERROR;
            } else {
                int width,bval;
                if(!strcmp(a[j],"-width")&&integer(a[j+1],&width)) t->width[k]=width;
                else if(!strcmp(a[j],"-anchor")) t->col_anchor[k]=parse_anchor(a[j+1]);
                else if(!strcmp(a[j],"-stretch")&&boolean(a[j+1],&bval)) t->col_stretch[k]=(unsigned char)bval;
                else if(!strcmp(a[j],"-minwidth")&&integer(a[j+1],&width)) t->minwidth[k]=width;
                else if(!strcmp(a[j],"-id")) {}
                else return TG_ERROR;
            }
        }
        return TG_OK;
    }
    if(n<3) return -1;
    int k=find(t,a[2]);
    if(!strcmp(op,"exists")&&n==3) return result(out,cap,k>=0?"1":"0");
    if(k<0) return -1;
    node *r=&t->rows[k];
    if(!strcmp(op,"children")&&n==3) { for(unsigned j=r->child;j;j=t->rows[j].next) if(!append_id(out,cap,t->rows[j].id)) return TG_ERROR; return TG_OK; }
    if(!strcmp(op,"focus")&&n==3) { t->focus=(unsigned)k; return TG_OK; }
    if(!k) return -1;
    if(!strcmp(op,"set")) return cell_command(t,r,n,a,out,cap);
    if(!strcmp(op,"move")&&n==5) return move_row(t,(unsigned)k,a[3],a[4]);
    if(!strcmp(op,"delete")&&n==3) { erase(t,(unsigned)k); return TG_OK; }
    if(!strcmp(op,"parent")&&n==3) return result(out,cap,t->rows[r->parent].id);
    if(!strcmp(op,"next")&&n==3) return result(out,cap,t->rows[r->next].id);
    if(!strcmp(op,"prev")&&n==3) return result(out,cap,t->rows[r->prev].id);
    if(!strcmp(op,"index")&&n==3) {
        unsigned pos=0; for(unsigned j=r->prev;j;j=t->rows[j].prev) pos++;
        int z=snprintf(out,cap,"%u",pos); return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR;
    }
    if(!strcmp(op,"item")&&n>=3) {
        if(n==3) {
            const char *fields[]={"-text",r->text,"-values",r->values,"-open",r->open?"1":"0","-tags",r->tags,"-image",""};
            for(size_t j=0;j<sizeof fields/sizeof fields[0];j++) if(!tg_list_append(out,cap,fields[j])) return TG_ERROR;
            return TG_OK;
        }
        if(n==4) {
            if(!strcmp(a[3],"-text")) return result(out,cap,r->text);
            if(!strcmp(a[3],"-values")) return result(out,cap,r->values);
            if(!strcmp(a[3],"-open")) return result(out,cap,r->open?"1":"0");
            if(!strcmp(a[3],"-tags")) return result(out,cap,r->tags);
            return TG_ERROR;
        }
        node opt=*r; if(!options(&opt,n-3,a+3)) return TG_ERROR; *r=opt; return TG_OK;
    }
    return -1;
}
static unsigned advance(const tg_tree *t,unsigned k,unsigned *depth) {
    if(t->rows[k].open&&t->rows[k].child) { (*depth)++; return t->rows[k].child; }
    while(k&&!t->rows[k].next) { k=t->rows[k].parent; if(*depth) (*depth)--; }
    return k?t->rows[k].next:0;
}
/* Drag state belongs to the tree; the UI captures its widget until release. */
int tg_tree_resize(const tg_ui *u,tg_widget *w,int phase,int x,int y) {
    tg_tree *t=w->tree; if(!t||w->state==TG_STATE_DISABLED) return 0;
    if(phase==0) {
        int hh=t->show_head?tree_head_height(tree_row_height(u,w)):0;
        if(y<w->rect.y||y>=w->rect.y+hh) return 0;
        int widths[COLS+1]; compute_col_widths(t,w->rect.w,widths);
        int edge=w->rect.x-t->scroll_x;
        for(unsigned k=t->show_tree?0:1;k<=t->nc;k++) {
            edge+=widths[k];
            if(abs(x-edge)<=5) {
                /* Freeze displayed widths so stretched neighbours do not jump. */
                for(unsigned j=t->show_tree?0:1;j<=t->nc;j++) t->width[j]=widths[j];
                t->drag_column=(int)k; t->drag_x=x; t->drag_width=widths[k]; return 1;
            }
        }
        return 0;
    }
    int width=t->drag_width+x-t->drag_x;
    if(width<t->minwidth[t->drag_column]) width=t->minwidth[t->drag_column];
    if(width>16384) width=16384;
    t->width[t->drag_column]=width; return 1;
}
int tg_tree_click(const tg_ui *u,tg_widget *w,int x,int y) {
    tg_tree *t=w->tree; if(!t||w->state==TG_STATE_DISABLED) return 0;
    int row_h=tree_row_height(u,w);
    int head_h=t->show_head?tree_head_height(row_h):0;
    int line=y-w->rect.y-head_h; if(line<0||y>=w->rect.y+w->rect.h) return 0;
    int target_row=t->scroll_y+line/row_h;
    unsigned depth=0,k=t->rows[0].child; for(int j=0;k&&j<target_row;j++) k=advance(t,k,&depth);
    if(!k) return 0;
    if(t->show_tree&&t->rows[k].child&&x>=w->rect.x-t->scroll_x+(int)depth*12&&x<w->rect.x-t->scroll_x+(int)depth*12+12) t->rows[k].open=!t->rows[k].open;
    else { for(unsigned j=1;j<t->capacity;j++) t->rows[j].selected=j==k; t->focus=k; }
    return 1;
}
static void cell(tg_canvas *c,tg_rect area,int x,int y,int width,int height,const char *s,const char *font,unsigned char anchor,uint32_t fg) {
    if(!s||!*s||width<=0||height<=0) return;
    tg_rect clip;
    if(!tg_rect_intersect(area,(tg_rect){x,y,width,height},&clip)) return;
    if(tg_text_native_enabled()) {
        const char *f=(font&&*font)?font:"";
        int tw=0,th=0;
        if(tg_text_font_measure(s,f,&tw,&th)==TG_OK) {
            int tx=x+8;
            if(anchor==1) { tx=x+(width-tw)/2; if(tx<x+4) tx=x+4; }
            else if(anchor==2) { tx=x+width-tw-8; if(tx<x+4) tx=x+4; }
            int ty=y+(height-th)/2;
            tg_text_font_draw(c,clip,tx,ty,s,f,fg);
            return;
        }
    }
    tg_text_draw_row(c,clip,x+8,s,fg);
}
void tg_tree_paint(const tg_ui *u,const tg_widget *w,tg_canvas *c,uint32_t fg) {
    const tg_tree *t=w->tree; if(!t) return;
    tg_rect r=w->rect; int y=r.y;
    const char *style=w->style[0]?w->style:"Treeview";
    char head_style[96];
    snprintf(head_style,sizeof head_style,"%s.Heading",style);
    int row_h=tree_row_height(u,w);
    int head_h=tree_head_height(row_h);

    uint32_t body_bg=w->bg, body_fg=fg;
    if(u) {
        if(!tg_style_lookup_color(u,style,"-fieldbackground",0,0,&body_bg))
            (void)tg_style_lookup_color(u,style,"-background",0,0,&body_bg);
        (void)tg_style_lookup_color(u,style,"-foreground",0,0,&body_fg);
    }
    int dark=is_dark_color(body_bg);
    uint32_t sel_bg=dark?0x2b6cb0:0x397bc4, sel_fg=0xffffff;
    if(u) {
        (void)tg_style_lookup_color(u,style,"-background",1,0,&sel_bg);
        (void)tg_style_lookup_color(u,style,"-foreground",1,0,&sel_fg);
    }
    uint32_t head_bg=shift_color(body_bg,dark?12:-14);
    uint32_t head_fg=body_fg;
    if(u) {
        (void)tg_style_lookup_color(u,head_style,"-background",0,0,&head_bg);
        (void)tg_style_lookup_color(u,head_style,"-foreground",0,0,&head_fg);
    }
    uint32_t border_col=shift_color(body_bg,dark?24:-24);
    if(u) {
        if(!tg_style_lookup_color(u,style,"-bordercolor",0,0,&border_col))
            (void)tg_style_lookup_color(u,head_style,"-bordercolor",0,0,&border_col);
    }

    char body_font[256]={0}, head_font[256]={0};
    if(u) {
        char raw[256];
        if(tg_style_lookup_value(u,style,"-font",0,0,raw,sizeof raw)>0&&raw[0])
            snprintf(body_font,sizeof body_font,"%s",tg_ui_font_spec(u,raw));
        if(tg_style_lookup_value(u,head_style,"-font",0,0,raw,sizeof raw)>0&&raw[0])
            snprintf(head_font,sizeof head_font,"%s",tg_ui_font_spec(u,raw));
    }
    if(!body_font[0]&&w->font_spec[0]) snprintf(body_font,sizeof body_font,"%s",w->font_spec);
    if(!head_font[0]) snprintf(head_font,sizeof head_font,"%s",body_font);

    int col_w[COLS+1];
    compute_col_widths(t,r.w,col_w);

    tg_raster_fill(c,r,r,body_bg);

    if(t->show_head) {
        tg_raster_fill(c,r,(tg_rect){r.x,y,r.w,head_h},head_bg);
        int x=r.x-t->scroll_x;
        unsigned start=t->show_tree?0:1;
        uint32_t hsep=blend_color(head_bg,border_col,120);
        for(unsigned k=start;k<=t->nc;k++) {
            cell(c,r,x,y,col_w[k],head_h,t->heading[k],head_font,t->head_anchor[k],head_fg);
            if(k<t->nc) tg_raster_fill(c,r,(tg_rect){x+col_w[k]-1,y+4,1,head_h>8?head_h-8:head_h},hsep);
            x+=col_w[k];
        }
        tg_raster_fill(c,r,(tg_rect){r.x,y+head_h-1,r.w,1},border_col);
        y+=head_h;
    }

    unsigned depth=0,k=t->rows[0].child;
    for(int skip=0;k&&skip<t->scroll_y;skip++) k=advance(t,k,&depth);
    for(;k&&y<r.y+r.h;k=advance(t,k,&depth),y+=row_h) {
        const node *p=&t->rows[k];
        int x=r.x-t->scroll_x;
        uint32_t row_bg=body_bg;
        uint32_t row_fg=body_fg;
        const char *row_font=body_font;

        if(p->tags[0]&&t->tag_count>0) {
            const char *tp=p->tags; char tname[64];
            while(tg_list_next(&tp,tname,sizeof tname)>0) {
                const tree_tag *tag=find_tag_const(t,tname);
                if(!tag) continue;
                if(tag->has_bg) row_bg=tag->bg;
                if(tag->has_fg) row_fg=tag->fg;
                if(tag->font[0]) row_font=tag->font;
            }
        }
        if(p->selected) {
            row_bg=sel_bg;
            row_fg=sel_fg;
        }
        if(row_bg!=body_bg) tg_raster_fill(c,r,(tg_rect){r.x,y,r.w,row_h},row_bg);
        if(!p->selected) {
            uint32_t rsep=blend_color(row_bg,border_col,70);
            tg_raster_fill(c,r,(tg_rect){r.x,y+row_h-1,r.w,1},rsep);
        }
        if(t->show_tree) {
            int indent=(int)depth*12;
            if(indent<col_w[0]) {
                if(p->child) cell(c,r,x+indent,y,14,row_h,p->open?"-":"+",row_font,1,row_fg);
                if(indent+14<col_w[0]) cell(c,r,x+indent+14,y,col_w[0]-indent-14,row_h,p->text,row_font,t->col_anchor[0],row_fg);
            }
            x+=col_w[0];
        }
        const char *cursor=p->values; char item[512];
        for(unsigned j=1;j<=t->nc;j++) {
            if(tg_list_next(&cursor,item,sizeof item)<=0) item[0]=0;
            cell(c,r,x,y,col_w[j],row_h,item,row_font,t->col_anchor[j],row_fg);
            x+=col_w[j];
        }
    }
}
