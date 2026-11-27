#include "TKGPUCanvasItems.H"
#include "../image/TKGPUImageRegistry.H"
#include "../TKGPUWidgetInternal.H"
#include "../../core/TKGPUList.H"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
static int result(char *b,size_t cap,const char *s,int rc) {
    if(!cap) return TG_ERROR;
    snprintf(b,cap,"%s",s); return rc;
}
static int option_error(char *b,size_t cap,const char *path,const char *operation,const char *key) {
    snprintf(b,cap,"unsupported canvas operation %.80s on %.160s: invalid or unsupported option %.100s",operation,path,key);
    return TG_ERROR;
}
static int coord(const char *s,int *v) {
    char *end; errno=0; double n=strtod(s,&end);
    if(errno||end==s||*end||!(n>=-10000000&&n<=10000000)) return 0;
    *v=(int)(n<0?n-0.5:n+0.5); return 1;
}
static int color(const char *s,uint32_t *v,int *present) {
    if(!*s) { *present=0; return 1; }
    if(!strcmp(s,"black")) *v=0;
    else if(!strcmp(s,"white")) *v=0xffffff;
    else if(!strcmp(s,"red")) *v=0xff0000;
    else if(!strcmp(s,"blue")) *v=0x0000ff;
    else {
        if(strlen(s)!=7||*s!='#') return 0;
        uint32_t n=0;
        for(int k=1;k<7;k++) {
            char c=s[k]; unsigned d;
            if(c>='0'&&c<='9') d=(unsigned)(c-'0');
            else if(c>='a'&&c<='f') d=(unsigned)(c-'a'+10);
            else if(c>='A'&&c<='F') d=(unsigned)(c-'A'+10);
            else return 0;
            n=(n<<4)|d;
        }
        *v=n;
    }
    *present=1; return 1;
}
static int opacity(tg_canvas_item *p,const char *s) {
    char *end; errno=0; double v=strtod(s,&end);
    if(errno||end==s||*end||!(v>=0&&v<=1)) return 0;
    p->opacity=(unsigned char)(v*255.0+0.5); p->opacity_set=1; return 1;
}
static int tag_has(const char *list,const char *tag) {
    char word[256]; int rc;
    while((rc=tg_list_next(&list,word,sizeof word))>0) if(!strcmp(word,tag)) return 1;
    return 0;
}
static int matches(const tg_canvas_item *p,tg_id owner,const char *s) {
    if(p->owner!=owner) return 0;
    if(!strcmp(s,"all")) return 1;
    char *end; errno=0; unsigned long id=strtoul(s,&end,10);
    if(*s>='0'&&*s<='9'&&!*end&&!errno) return id==p->id;
    return tag_has(p->tags,s);
}
static tg_canvas_item *find_item(tg_ui *u,tg_id owner,const char *s) {
    if(u->drawing) for(size_t k=0;k<u->drawing->count;k++)
        if(matches(&u->drawing->item[k],owner,s)) return &u->drawing->item[k];
    return NULL;
}
/* Stacking order is the dense item order; other canvases are independent. */
static uint32_t adjacent_item(const tg_ui *u,tg_id owner,const char *selector,int above) {
    if(!u->drawing) return 0;
    const tg_canvas_items *d=u->drawing;
    size_t at=d->count;
    for(size_t k=0;k<d->count;k++) if(matches(&d->item[k],owner,selector)) {
        at=k;
        if(!above) break;
    }
    if(at==d->count) return 0;
    if(above) {
        for(size_t k=at+1;k<d->count;k++) if(d->item[k].owner==owner) return d->item[k].id;
    } else {
        while(at) { at--; if(d->item[at].owner==owner) return d->item[at].id; }
    }
    return 0;
}
static int valid_tags(const char *s) {
    if(strlen(s)>=256) return 0;
    char word[256]; int rc;
    while((rc=tg_list_next(&s,word,sizeof word))>0) { if(!*word) return 0; }
    return rc==0;
}
static int append_id_fast(char *b,size_t cap,size_t *used,uint32_t id) {
    char word[24];
    int len=snprintf(word,sizeof word,"%lu",(unsigned long)id);
    if(len<=0) return 0;
    size_t need=(*used?1u:0u)+(size_t)len;
    if(*used+need>=cap) return 0;
    if(*used) b[(*used)++]=' ';
    memcpy(b+*used,word,(size_t)len+1);
    *used+=(size_t)len;
    return 1;
}
static int shift(tg_canvas_item *p,int dx,int dy) {
    if(p->kind==TG_ITEM_LINE||p->kind==TG_ITEM_POLYGON) {
        for(unsigned k=0;k<p->point_count;k++) {
            int x=p->points[k*2]+dx,y=p->points[k*2+1]+dy;
            if(x< -10000000||x>10000000||y< -10000000||y>10000000) return 0;
            p->points[k*2]=x; p->points[k*2+1]=y;
        }
        p->x0=p->points[0]; p->y0=p->points[1];
        p->x1=p->points[2*p->point_count-2]; p->y1=p->points[2*p->point_count-1];
        return 1;
    }
    int *xy[]={&p->x0,&p->y0,&p->x1,&p->y1};
    unsigned n=(p->kind==TG_ITEM_IMAGE||p->kind==TG_ITEM_TEXT||p->kind==TG_ITEM_WINDOW)?2:4;
    for(unsigned k=0;k<n;k++) {
        int v=*xy[k]+(k%2?dy:dx);
        if(v< -10000000||v>10000000) return 0;
        *xy[k]=v;
    }
    return 1;
}
static int line_points(tg_canvas_item *p,size_t n,char **a) {
    if(n<4||n%2||n>2*TG_LINE_POINTS) return 0;
    for(size_t k=0;k<n;k++) if(!coord(a[k],&p->points[k])) return 0;
    p->point_count=(unsigned)(n/2);
    p->x0=p->points[0]; p->y0=p->points[1]; p->x1=p->points[n-2]; p->y1=p->points[n-1];
    return 1;
}
static int text_anchor(const char *s) {
    if(!strcmp(s,"c")) return 5;
    static const char *names[]={"nw","n","ne","w","center","e","sw","s","se"};
    for(int k=0;k<9;k++) if(!strcmp(s,names[k])) return k+1;
    return 0;
}
static int font_option(tg_canvas_item *p,const char *s) {
    snprintf(p->font,sizeof p->font,"%s",s);
    const char *cursor=s; char word[128]; int scale=1,rc,seen=0;
    while((rc=tg_list_next(&cursor,word,sizeof word))>0) {
        int size;
        if(seen&&coord(word,&size)&&size) {
            if(size<0) size=-size;
            scale=(size+4)/8;
            if(scale<1) scale=1; if(scale>16) scale=16;
            break;
        }
        seen=1;
    }
    p->text_scale=(unsigned)scale; return 1;
}
const tg_canvas_item *tg_canvas_window_item(const tg_ui *u,tg_id owner,tg_id child) {
    if(u->drawing) for(size_t k=0;k<u->drawing->count;k++) {
        const tg_canvas_item *p=&u->drawing->item[k];
        if(p->owner==owner&&p->kind==TG_ITEM_WINDOW&&p->window_id==child) return p;
    }
    return NULL;
}
static int option(tg_canvas_item *p,const char *key,const char *value) {
    if(!strcmp(key,"-state")) {
        if(!strcmp(value,"hidden")) p->hidden=1;
        else if(!strcmp(value,"normal")||!strcmp(value,"")) p->hidden=0;
        return 1;
    }
    if(!strcmp(key,"-tags")) {
        if(!valid_tags(value)) return 0;
        strcpy(p->tags,value); return 1;
    }
    if(p->kind==TG_ITEM_WINDOW) {
        if(!strcmp(key,"-width")||!strcmp(key,"-height")) {
            int size; if(!coord(value,&size)||size<0) return 0;
            if(key[1]=='w') p->window_width=size; else p->window_height=size;
            return 1;
        }
        if(!strcmp(key,"-anchor")) {
            int a=text_anchor(value); if(!a) return 0;
            p->text_anchor=(unsigned)a; return 1;
        }
    }
    if(p->kind==TG_ITEM_TEXT) {
        if(!strcmp(key,"-text")) {
            snprintf(p->text,sizeof p->text,"%s",value);
            return 1;
        }
        if(!strcmp(key,"-font")) return font_option(p,value);
        if(!strcmp(key,"-anchor")) {
            int a=text_anchor(value); if(!a) return 0;
            p->text_anchor=(unsigned)a; return 1;
        }
        if(!strcmp(key,"-justify")) return 1;
        if(!strcmp(key,"-angle")) return 1;
    }
    if(!strcmp(key,"-anchor")) {
        int a=text_anchor(value);
        if(!a) return 0;
        p->text_anchor=(unsigned)a;
        if(!strcmp(value,"nw")) p->anchor_nw=1;
        else if(!strcmp(value,"center")||!strcmp(value,"c")) p->anchor_nw=0;
        return 1;
    }
    if(!strcmp(key,"-opacity")) return opacity(p,value);
    if(!strcmp(key,"-fill")) return color(value,&p->fill,&p->has_fill);
    if(!strcmp(key,"-outline")) return color(value,&p->outline,&p->has_outline);
    if(!strcmp(key,"-width")&&p->kind!=TG_ITEM_IMAGE) {
        int w; if(!coord(value,&w)||w<0) return 0;
        p->width=(unsigned)w; return 1;
    }
    if(p->kind==TG_ITEM_LINE&&!strcmp(key,"-capstyle")) {
        if(strcmp(value,"butt")&&strcmp(value,"round")&&strcmp(value,"projecting")) return 0;
        p->round_cap=!strcmp(value,"round"); return 1;
    }
    if(p->kind==TG_ITEM_LINE&&!strcmp(key,"-joinstyle")) {
        if(strcmp(value,"round")&&strcmp(value,"bevel")&&strcmp(value,"miter")) return 0;
        p->round_join=!strcmp(value,"round"); return 1;
    }
    if(!strcmp(key,"-stipple")) {
        if(!p->opacity_set) {
            if(!strcmp(value,"gray12")) { p->opacity=32; p->opacity_set=1; }
            else if(!strcmp(value,"gray25")) { p->opacity=64; p->opacity_set=1; }
            else if(!strcmp(value,"gray50")) { p->opacity=128; p->opacity_set=1; }
            else if(!strcmp(value,"gray75")) { p->opacity=192; p->opacity_set=1; }
        }
        return 1;
    }
    if(!strcmp(key,"-activestipple")||!strcmp(key,"-disabledstipple")) return 1;
    if(!strcmp(key,"-activefill")||!strcmp(key,"-disabledfill")) return 1;
    if(!strcmp(key,"-activeoutline")||!strcmp(key,"-disabledoutline")) return 1;
    if(!strcmp(key,"-activewidth")||!strcmp(key,"-disabledwidth")) return 1;
    if(!strcmp(key,"-dash")) {
        p->dash_on=p->dash_off=0;
        if(value&&*value) {
            int d0=0,d1=0;
            int cnt=sscanf(value,"%d %d",&d0,&d1);
            if(cnt>=1&&d0>0) {
                p->dash_on=(unsigned short)(d0>255?255:d0);
                p->dash_off=(unsigned short)((cnt>=2&&d1>0)?(d1>255?255:d1):p->dash_on);
            } else if(strchr(value,'-')) {
                p->dash_on=6; p->dash_off=4;
            } else if(strchr(value,'.')) {
                p->dash_on=2; p->dash_off=2;
            }
        }
        return 1;
    }
    if(!strcmp(key,"-activedash")||!strcmp(key,"-disableddash")) return 1;
    if(!strcmp(key,"-arrow")||!strcmp(key,"-arrowshape")) return 1;
    if(!strcmp(key,"-smooth")) {
        p->smooth=(!strcmp(value,"1")||!strcmp(value,"true")||!strcmp(value,"yes")||!strcmp(value,"bezier")||!strcmp(value,"raw"))?1:0;
        return 1;
    }
    if(!strcmp(key,"-splinesteps")) {
        int st=0; if(coord(value,&st)&&st>=2) p->splinesteps=(unsigned char)(st>12?12:st);
        return 1;
    }
    if(!strcmp(key,"-offset")) return 1;
    if(!strcmp(key,"-start")) {
        char *end; errno=0; double v=strtod(value,&end);
        if(errno||end==value||*end) return 0;
        p->arc_start=v; return 1;
    }
    if(!strcmp(key,"-extent")) {
        char *end; errno=0; double v=strtod(value,&end);
        if(errno||end==value||*end) return 0;
        p->arc_extent=v; return 1;
    }
    if(!strcmp(key,"-style")) {
        if(!strcmp(value,"arc")) p->arc_style=2;
        else if(!strcmp(value,"chord")) p->arc_style=1;
        else p->arc_style=0;
        return 1;
    }
    if(!strcmp(key,"-outlinestipple")||!strcmp(key,"-activeoutlinestipple")||!strcmp(key,"-disabledoutlinestipple")) return 1;
    return 0;
}
int tg_canvas_option(tg_widget *w,const char *key,const char *value) {
    if((w->kind==TG_TEXT||w->kind==TG_LISTBOX||w->kind==TG_TREEVIEW)&&(!strcmp(key,"-xscrollcommand")||!strcmp(key,"-yscrollcommand"))) {
        if(strlen(value)>=sizeof w->canvas_xscroll) return TG_ERROR;
        strcpy(key[1]=='x'?w->canvas_xscroll:w->canvas_yscroll,value);
        return TG_OK;
    }
    if(w->kind!=TG_CANVAS) return -1;
    if(!strcmp(key,"-scrollregion")) {
        if(!*value) { w->canvas_region_set=0; w->canvas_x=w->canvas_y=0; return TG_OK; }
        char word[64]; int xy[4]; const char *cursor=value;
        for(int k=0;k<4;k++) if(tg_list_next(&cursor,word,sizeof word)!=1||!coord(word,&xy[k])) return TG_ERROR;
        if(tg_list_next(&cursor,word,sizeof word)!=0||xy[2]<xy[0]||xy[3]<xy[1]) return TG_ERROR;
        w->canvas_region=(tg_rect){xy[0],xy[1],xy[2]-xy[0],xy[3]-xy[1]};
        if(!w->canvas_region_set) { w->canvas_x=xy[0]; w->canvas_y=xy[1]; }
        w->canvas_region_set=1; return TG_OK;
    }
    if(!strcmp(key,"-xscrollincrement")||!strcmp(key,"-yscrollincrement")) {
        int v; if(!coord(value,&v)||v<0) return TG_ERROR;
        if(key[1]=='x') w->canvas_xincrement=v; else w->canvas_yincrement=v;
        return TG_OK;
    }
    if(!strcmp(key,"-xscrollcommand")||!strcmp(key,"-yscrollcommand")) {
        if(strlen(value)>=sizeof w->canvas_xscroll) return TG_ERROR;
        strcpy(key[1]=='x'?w->canvas_xscroll:w->canvas_yscroll,value);
        return TG_OK;
    }
    return -1;
}
int tg_canvas_query(const tg_widget *w,const char *key,char *b,size_t cap) {
    if((w->kind==TG_TEXT||w->kind==TG_LISTBOX||w->kind==TG_TREEVIEW)&&(!strcmp(key,"-xscrollcommand")||!strcmp(key,"-yscrollcommand"))) {
        return result(b,cap,key[1]=='x'?w->canvas_xscroll:w->canvas_yscroll,TG_OK);
    }
    if(w->kind!=TG_CANVAS) return -1;
    int z;
    if(!strcmp(key,"-scrollregion")) {
        if(!w->canvas_region_set) return result(b,cap,"",TG_OK);
        tg_rect r=w->canvas_region;
        z=snprintf(b,cap,"%d %d %d %d",r.x,r.y,r.x+r.w,r.y+r.h);
    } else if(!strcmp(key,"-xscrollincrement")||!strcmp(key,"-yscrollincrement"))
        z=snprintf(b,cap,"%d",key[1]=='x'?w->canvas_xincrement:w->canvas_yincrement);
    else if(!strcmp(key,"-xscrollcommand")||!strcmp(key,"-yscrollcommand"))
        return result(b,cap,key[1]=='x'?w->canvas_xscroll:w->canvas_yscroll,TG_OK);
    else return -1;
    return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR;
}
static int clamp(int v,int lo,int hi) { return v<lo?lo:v>hi?hi:v; }
static int viewport(const tg_widget *w,int y) {
    int extent=y?w->rect.h:w->rect.w;
    return extent>0?extent:0;
}
void tg_canvas_view_clamp(tg_widget *w) {
    if(w->kind!=TG_CANVAS) return;
    if(!w->canvas_region_set) { w->canvas_x=w->canvas_y=0; return; }
    tg_rect r=w->canvas_region;
    int dx=r.w-viewport(w,0),dy=r.h-viewport(w,1);
    w->canvas_x=clamp(w->canvas_x,r.x,r.x+(dx>0?dx:0));
    w->canvas_y=clamp(w->canvas_y,r.y,r.y+(dy>0?dy:0));
}
void tg_canvas_view_fractions(const tg_widget *w,int y,double *first,double *last) {
    int origin=y?w->canvas_y:w->canvas_x;
    int extent=viewport(w,y),total=y?w->canvas_region.h:w->canvas_region.w;
    int start=y?w->canvas_region.y:w->canvas_region.x;
    *first=0; *last=1;
    if(w->canvas_region_set&&total>0) {
        int offset=clamp(origin-start,0,total);
        *first=(double)offset/total; *last=(double)clamp(offset+extent,0,total)/total;
    }
}
static int view_command(tg_widget *w,size_t n,char **a,char *b,size_t cap) {
    int y=a[1][0]=='y',*origin=y?&w->canvas_y:&w->canvas_x;
    int extent=viewport(w,y),total=y?w->canvas_region.h:w->canvas_region.w;
    int start=y?w->canvas_region.y:w->canvas_region.x;
    if(n==2) {
        double first,last;
        tg_canvas_view_fractions(w,y,&first,&last);
        int z=snprintf(b,cap,"%.9g %.9g",first,last);
        return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR;
    }
    if(n==4&&!strcmp(a[2],"moveto")) {
        char *end; errno=0; double v=strtod(a[3],&end);
        if(errno||end==a[3]||*end||!(v>=-1000000&&v<=1000000)) return TG_ERROR;
        if(v<0) v=0; if(v>1) v=1;
        *origin=start+(int)(v*total+0.5);
    } else if(n==5&&!strcmp(a[2],"scroll")) {
        int amount; if(!coord(a[3],&amount)) return TG_ERROR;
        int step=y?w->canvas_yincrement:w->canvas_xincrement;
        if(!strcmp(a[4],"pages")||!strcmp(a[4],"page")) step=extent*9/10;
        else if(strcmp(a[4],"units")&&strcmp(a[4],"unit")) return TG_ERROR;
        else if(!step) step=extent/10;
        if(step<1) step=1;
        int64_t next=(int64_t)*origin+(int64_t)amount*step;
        *origin=(int)(next< -65536?-65536:next>65536?65536:next);
    } else return TG_ERROR;
    tg_canvas_view_clamp(w); return result(b,cap,"",TG_OK);
}
int tg_canvas_item_command(tg_ui *u,tg_id owner,size_t n,char **a,char *b,size_t cap) {
    if(n<2) return -1;
    tg_widget *w=tg_lookup(u,owner);
    if(!strcmp(a[1],"xview")||!strcmp(a[1],"yview")) return view_command(w,n,a,b,cap);
    if(!strcmp(a[1],"canvasx")||!strcmp(a[1],"canvasy")) {
        int v,grid=0;
        if((n!=3&&n!=4)||!coord(a[2],&v)||(n==4&&(!coord(a[3],&grid)||grid<=0))) goto bad;
        v+=!strcmp(a[1],"canvasx")?w->canvas_x:w->canvas_y;
        if(grid) v=(v>=0?(v+grid/2)/grid:(v-grid/2)/grid)*grid;
        int z=snprintf(b,cap,"%d",v); return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR;
    }
    if(!strcmp(a[1],"delete")) {
        if(n<2) goto bad;
        if(n>2&&u->drawing) {
            size_t dst=0;
            for(size_t k=0;k<u->drawing->count;k++) {
                tg_canvas_item *p=&u->drawing->item[k]; int drop=0;
                for(size_t j=2;j<n&&!drop;j++) drop=matches(p,owner,a[j]);
                if(!drop) { if(dst!=k) u->drawing->item[dst]=*p; dst++; }
            }
            u->drawing->count=dst;
        }
        return result(b,cap,"",TG_OK);
    }
    if(!strcmp(a[1],"find")) {
        if(n<3||!cap) goto bad;
        b[0]=0;
        size_t used=0;
        if(n==4&&(!strcmp(a[2],"above")||!strcmp(a[2],"below"))) {
            uint32_t id=adjacent_item(u,owner,a[3],!strcmp(a[2],"above"));
            if(id&&!append_id_fast(b,cap,&used,id)) goto bad;
            return TG_OK;
        }
        int all=n==3&&!strcmp(a[2],"all"),tag=n==4&&!strcmp(a[2],"withtag");
        if(all||tag) {
            if(tag&&!strcmp(a[3],"current")) {
                if(u->drawing&&u->drawing->current_id) for(size_t k=0;k<u->drawing->count;k++) {
                    const tg_canvas_item *p=&u->drawing->item[k];
                    if(p->owner==owner&&p->id==u->drawing->current_id) {
                        if(!append_id_fast(b,cap,&used,p->id)) goto bad;
                        break;
                    }
                }
                return TG_OK;
            }
            if(u->drawing) for(size_t k=0;k<u->drawing->count;k++) {
                const tg_canvas_item *p=&u->drawing->item[k];
                if(matches(p,owner,all?"all":a[3])&&!append_id_fast(b,cap,&used,p->id)) goto bad;
            }
            return TG_OK;
        }
        if((!strcmp(a[2],"overlapping")||!strcmp(a[2],"enclosed"))&&n==7) {
            int x1,y1,x2,y2;
            if(!coord(a[3],&x1)||!coord(a[4],&y1)||!coord(a[5],&x2)||!coord(a[6],&y2)) goto bad;
            if(x1>x2) { int t=x1; x1=x2; x2=t; }
            if(y1>y2) { int t=y1; y1=y2; y2=t; }
            int enc=!strcmp(a[2],"enclosed");
            if(u->drawing) for(size_t k=0;k<u->drawing->count;k++) {
                const tg_canvas_item *p=&u->drawing->item[k];
                tg_rect q;
                if(p->owner!=owner||p->hidden||!tg_canvas_item_bounds(u,p,&q)) continue;
                int hit=enc?(q.x>=x1&&q.y>=y1&&q.x+q.w<=x2&&q.y+q.h<=y2):(q.x<=x2&&q.x+q.w>=x1&&q.y<=y2&&q.y+q.h>=y1);
                if(hit&&!append_id_fast(b,cap,&used,p->id)) goto bad;
            }
            return TG_OK;
        }
        if(!strcmp(a[2],"closest")&&n>=5) {
            int cx,cy; if(!coord(a[3],&cx)||!coord(a[4],&cy)) goto bad;
            uint32_t best_id=0; long long best_d=-1;
            if(u->drawing) for(size_t k=0;k<u->drawing->count;k++) {
                const tg_canvas_item *p=&u->drawing->item[k];
                tg_rect q;
                if(p->owner!=owner||p->hidden||!tg_canvas_item_bounds(u,p,&q)) continue;
                int dx=cx<q.x?q.x-cx:(cx>q.x+q.w?cx-(q.x+q.w):0);
                int dy=cy<q.y?q.y-cy:(cy>q.y+q.h?cy-(q.y+q.h):0);
                long long d=(long long)dx*dx+(long long)dy*dy;
                if(best_d<0||d<=best_d) { best_d=d; best_id=p->id; }
            }
            if(best_id&&!append_id_fast(b,cap,&used,best_id)) goto bad;
            return TG_OK;
        }
        goto bad;
    }
    if(!strcmp(a[1],"gettags")||!strcmp(a[1],"type")) {
        if(n!=3) goto bad;
        tg_canvas_item *p=find_item(u,owner,a[2]);
        static const char *names[]={"","line","rectangle","oval","image","text","window","polygon"};
        return result(b,cap,p?(!strcmp(a[1],"gettags")?p->tags:(p->is_arc?"arc":names[p->kind])):"",TG_OK);
    }
    if(!strcmp(a[1],"bbox")) {
        if(n<3) goto bad;
        tg_rect bounds={0,0,0,0}; int found=0;
        if(u->drawing) for(size_t k=0;k<u->drawing->count;k++) {
            const tg_canvas_item *p=&u->drawing->item[k]; int match=0;
            for(size_t j=2;j<n&&!match;j++) match=matches(p,owner,a[j]);
            tg_rect q; if(!match||!tg_canvas_item_bounds(u,p,&q)) continue;
            if(!found) { bounds=q; found=1; }
            else {
                int l=bounds.x<q.x?bounds.x:q.x,t=bounds.y<q.y?bounds.y:q.y;
                int r=bounds.x+bounds.w>q.x+q.w?bounds.x+bounds.w:q.x+q.w;
                int bot=bounds.y+bounds.h>q.y+q.h?bounds.y+bounds.h:q.y+q.h;
                bounds=(tg_rect){l,t,r-l,bot-t};
            }
        }
        if(!found) return result(b,cap,"",TG_OK);
        int z=snprintf(b,cap,"%d %d %d %d",bounds.x,bounds.y,bounds.x+bounds.w,bounds.y+bounds.h);
        return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR;
    }
    if(!strcmp(a[1],"move")) {
        int dx,dy;
        if(n!=5||!coord(a[3],&dx)||!coord(a[4],&dy)) goto bad;
        if(u->drawing) for(int pass=0;pass<2;pass++) for(size_t k=0;k<u->drawing->count;k++) {
            tg_canvas_item *p=&u->drawing->item[k];
            if(!matches(p,owner,a[2])) continue;
            tg_canvas_item copy=*p; if(!shift(&copy,dx,dy)) goto bad;
            if(pass) *p=copy;
        }
        return result(b,cap,"",TG_OK);
    }
    if(!strcmp(a[1],"raise")||!strcmp(a[1],"lower")) {
        if(n!=3&&n!=4) goto bad;
        if(!u->drawing) return result(b,cap,"",TG_OK);
        tg_canvas_items *d=u->drawing;
        int raise=!strcmp(a[1],"raise");
        size_t boundary=raise?d->count:0, count=0, insert=0;
        if(n==4) {
            int found=0;
            for(size_t k=0;k<d->count;k++) if(matches(&d->item[k],owner,a[3])) {
                if(raise) boundary=k+1;
                else if(!found) boundary=k;
                found=1;
            }
            if(!found) goto bad;
        }
        for(size_t k=0;k<d->count;k++) {
            if(matches(&d->item[k],owner,a[2])) count++;
            else if(k<boundary) insert++;
        }
        if(!count) return result(b,cap,"",TG_OK);
        tg_canvas_item *moved=malloc(count*sizeof *moved);
        if(!moved) goto bad;
        size_t kept=0,taken=0;
        for(size_t k=0;k<d->count;k++) {
            if(matches(&d->item[k],owner,a[2])) moved[taken++]=d->item[k];
            else { if(kept!=k) d->item[kept]=d->item[k]; kept++; }
        }
        memmove(d->item+insert+count,d->item+insert,(kept-insert)*sizeof *moved);
        memcpy(d->item+insert,moved,count*sizeof *moved);
        free(moved);
        return result(b,cap,"",TG_OK);
    }
    if(!strcmp(a[1],"scale")) {
        if(n!=7) goto bad;
        double v[4];
        for(int j=0;j<4;j++) {
            char *end; errno=0; v[j]=strtod(a[3+j],&end);
            if(errno||end==a[3+j]||*end||!(v[j]>=-10000000&&v[j]<=10000000)) goto bad;
        }
        if(u->drawing) for(int pass=0;pass<2;pass++) for(size_t k=0;k<u->drawing->count;k++) {
            tg_canvas_item *p=&u->drawing->item[k];
            if(!matches(p,owner,a[2])) continue;
            tg_canvas_item copy=*p;
            int *xy[]={&copy.x0,&copy.y0,&copy.x1,&copy.y1};
            unsigned count=(copy.kind==TG_ITEM_LINE||copy.kind==TG_ITEM_POLYGON)?copy.point_count*2:
                (copy.kind==TG_ITEM_IMAGE||copy.kind==TG_ITEM_TEXT||copy.kind==TG_ITEM_WINDOW)?2:4;
            for(unsigned j=0;j<count;j++) {
                int *field=(copy.kind==TG_ITEM_LINE||copy.kind==TG_ITEM_POLYGON)?&copy.points[j]:xy[j];
                double value=v[j%2]+(*field-v[j%2])*v[2+j%2];
                if(!(value>=-10000000&&value<=10000000)) goto bad;
                *field=(int)(value<0?value-0.5:value+0.5);
            }
            if(copy.kind==TG_ITEM_LINE||copy.kind==TG_ITEM_POLYGON) {
                copy.x0=copy.points[0]; copy.y0=copy.points[1];
                copy.x1=copy.points[count-2]; copy.y1=copy.points[count-1];
            }
            if(pass) *p=copy;
        }
        return result(b,cap,"",TG_OK);
    }
    if(!strcmp(a[1],"scan")) {
        int x,y,gain=10;
        if(n<5||n>6||!coord(a[3],&x)||!coord(a[4],&y)) goto bad;
        if(!strcmp(a[2],"mark")&&n==5) {
            w->scan_x=x; w->scan_y=y; w->scan_origin_x=w->canvas_x;
            w->scan_origin_y=w->canvas_y; w->scan_marked=1;
        } else if(!strcmp(a[2],"dragto")) {
            if(n==6&&(!coord(a[5],&gain)||gain<0)) goto bad;
            if(!w->scan_marked) return result(b,cap,"",TG_OK);
            int64_t nx=(int64_t)w->scan_origin_x+(int64_t)(w->scan_x-x)*gain;
            int64_t ny=(int64_t)w->scan_origin_y+(int64_t)(w->scan_y-y)*gain;
            w->canvas_x=(int)(nx< -65536?-65536:nx>65536?65536:nx);
            w->canvas_y=(int)(ny< -65536?-65536:ny>65536?65536:ny);
            tg_canvas_view_clamp(w);
        } else goto bad;
        return result(b,cap,"",TG_OK);
    }
    if(!strcmp(a[1],"bind")||!strcmp(a[1],"postscript")) {
        return result(b,cap,"",TG_OK);
    }
    if(!strcmp(a[1],"addtag")||!strcmp(a[1],"dtag")) {
        int add=!strcmp(a[1],"addtag");
        if(!add&&(n!=3&&n!=4)) goto bad;
        char adjacent[24];
        const char *selector=NULL,*tag=add?(n>=3?a[2]:""):n==4?a[3]:a[2];
        if(add) {
            if(n==4&&!strcmp(a[3],"all")) selector="all";
            else if(n==5&&!strcmp(a[3],"withtag")) selector=a[4];
            else if(n==5&&(!strcmp(a[3],"above")||!strcmp(a[3],"below"))) {
                uint32_t id=adjacent_item(u,owner,a[4],!strcmp(a[3],"above"));
                if(!id) return result(b,cap,"",TG_OK);
                snprintf(adjacent,sizeof adjacent,"%lu",(unsigned long)id);
                selector=adjacent;
            } else goto bad;
        } else selector=a[2];
        if(!*tag||strlen(tag)>=256) goto bad;
        if(u->drawing) for(int pass=0;pass<2;pass++) for(size_t k=0;k<u->drawing->count;k++) {
            tg_canvas_item *p=&u->drawing->item[k];
            if(!matches(p,owner,selector)) continue;
            char buf[256]="";
            if(add) {
                strcpy(buf,p->tags);
                if(!tag_has(buf,tag)&&!tg_list_append(buf,sizeof buf,tag)) goto bad;
            } else {
                const char *cursor=p->tags; char word[256]; int rc;
                while((rc=tg_list_next(&cursor,word,sizeof word))>0)
                    if(strcmp(word,tag)&&!tg_list_append(buf,sizeof buf,word)) goto bad;
                if(rc<0) goto bad;
            }
            if(pass) strcpy(p->tags,buf);
        }
        return result(b,cap,"",TG_OK);
    }
    if(!strcmp(a[1],"coords")) {
        if(n<3) goto bad;
        tg_canvas_item *p=find_item(u,owner,a[2]);
        if(!p) return result(b,cap,"",TG_OK);
        tg_canvas_item copy=*p;
        if(n==3) {
            size_t used=0; unsigned count=(p->kind==TG_ITEM_LINE||p->kind==TG_ITEM_POLYGON)?p->point_count*2:(p->kind==TG_ITEM_IMAGE||p->kind==TG_ITEM_TEXT||p->kind==TG_ITEM_WINDOW)?2:4;
            int xy[]={p->x0,p->y0,p->x1,p->y1};
            for(unsigned k=0;k<count;k++) {
                int z=snprintf(b+used,cap-used,"%s%d",k?" ":"",(p->kind==TG_ITEM_LINE||p->kind==TG_ITEM_POLYGON)?p->points[k]:xy[k]);
                if(z<0||(size_t)z>=cap-used) goto bad;
                used+=(size_t)z;
            }
            return TG_OK;
        }
        if(p->kind==TG_ITEM_LINE||p->kind==TG_ITEM_POLYGON) { if(!line_points(&copy,n-3,a+3)) goto bad; }
        else {
            if(n!=((p->kind==TG_ITEM_IMAGE||p->kind==TG_ITEM_TEXT||p->kind==TG_ITEM_WINDOW)?5:7)||!coord(a[3],&copy.x0)||!coord(a[4],&copy.y0)) goto bad;
            if(n==7&&(!coord(a[5],&copy.x1)||!coord(a[6],&copy.y1))) goto bad;
        }
        *p=copy; return result(b,cap,"",TG_OK);
    }
    if(!strcmp(a[1],"itemconfigure")||!strcmp(a[1],"itemconfig")||!strcmp(a[1],"itemcget")) {
        if(n<3) goto bad;
        tg_canvas_item *p=find_item(u,owner,a[2]);
        if(!p) return result(b,cap,"",TG_OK);
        if(n==3&&strcmp(a[1],"itemcget")) {
            char fill_s[16]="",out_s[16]="";
            if(p->has_fill) snprintf(fill_s,sizeof fill_s,"#%06lx",(unsigned long)p->fill);
            if(p->has_outline) snprintf(out_s,sizeof out_s,"#%06lx",(unsigned long)p->outline);
            if(p->kind==TG_ITEM_TEXT)
                snprintf(b,cap,"{-fill {} {} {} {%s}} {-text {} {} {} {%s}} {-font {} {} {} {%s}} {-tags {} {} {} {%s}}",fill_s,p->text,p->font,p->tags);
            else if(p->kind==TG_ITEM_LINE)
                snprintf(b,cap,"{-fill {} {} {} {%s}} {-width {} {} {} %u} {-tags {} {} {} {%s}}",fill_s,p->width,p->tags);
            else
                snprintf(b,cap,"{-fill {} {} {} {%s}} {-outline {} {} {} {%s}} {-width {} {} {} %u} {-tags {} {} {} {%s}}",fill_s,out_s,p->width,p->tags);
            return TG_OK;
        }
        if(n<4) goto bad;
        if(!strcmp(a[1],"itemcget")&&n==4) {
            int z;
            if(!strcmp(a[3],"-state")) z=snprintf(b,cap,"%s",p->hidden?"hidden":"normal");
            else if(!strcmp(a[3],"-opacity")) z=snprintf(b,cap,"%.9g",p->opacity_set?p->opacity/255.0:1.0);
            else if(!strcmp(a[3],"-text")&&p->kind==TG_ITEM_TEXT) z=snprintf(b,cap,"%s",p->text);
            else if(!strcmp(a[3],"-tags")) z=snprintf(b,cap,"%s",p->tags);
            else if(!strcmp(a[3],"-font")&&p->kind==TG_ITEM_TEXT) z=snprintf(b,cap,"%s",p->font);
            else if(!strcmp(a[3],"-anchor")) {
                static const char *names[]={"","nw","n","ne","w","center","e","sw","s","se"};
                z=snprintf(b,cap,"%s",p->text_anchor<=9?names[p->text_anchor]:"center");
            }
            else if(!strcmp(a[3],"-justify")) z=snprintf(b,cap,"%s","left");
            else if(!strcmp(a[3],"-window")&&p->kind==TG_ITEM_WINDOW) {
                const tg_widget *child=tg_ui_get(u,p->window_id);
                return result(b,cap,child?child->path:"",TG_OK);
            }
            else if(!strcmp(a[3],"-height")&&p->kind==TG_ITEM_WINDOW) z=snprintf(b,cap,"%d",p->window_height);
            else if(!strcmp(a[3],"-width")) z=snprintf(b,cap,"%u",p->kind==TG_ITEM_WINDOW?(unsigned)p->window_width:p->width);
            else if(!strcmp(a[3],"-fill")) z=p->has_fill?snprintf(b,cap,"#%06lx",(unsigned long)p->fill):snprintf(b,cap,"%s","");
            else if(!strcmp(a[3],"-outline")) z=p->has_outline?snprintf(b,cap,"#%06lx",(unsigned long)p->outline):snprintf(b,cap,"%s","");
            else if(!strcmp(a[3],"-capstyle")) z=snprintf(b,cap,"%s",p->round_cap?"round":"butt");
            else if(!strcmp(a[3],"-joinstyle")) z=snprintf(b,cap,"%s",p->round_join?"round":"bevel");
            else z=snprintf(b,cap,"%s","");
            return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR;
        }
        if(!strcmp(a[1],"itemcget")||n<5||!(n%2)) goto bad;
        for(int pass=0;pass<2;pass++) for(size_t j=0;j<u->drawing->count;j++) {
            tg_canvas_item *item=&u->drawing->item[j];
            if(!matches(item,owner,a[2])) continue;
            tg_canvas_item copy=*item;
            for(size_t k=3;k<n;k+=2) {
                if(copy.kind==TG_ITEM_IMAGE&&!strcmp(a[k],"-image")) {
                    const tg_image_slot *image=tg_image_find(u,a[k+1]);
                    if(!image&&*a[k+1]) goto bad;
                    copy.image_id=image?image->id:0;
                } else if(!option(&copy,a[k],a[k+1])) return option_error(b,cap,a[0],a[1],a[k]);
            }
            if(pass) *item=copy;
        }
        return result(b,cap,"",TG_OK);
    }
    if(strcmp(a[1],"create")) return -1;
    if(n>=7&&!strcmp(a[2],"image")) {
        tg_canvas_item p={0}; p.owner=owner; p.kind=TG_ITEM_IMAGE; p.text_anchor=5;
        if((n-5)%2||!coord(a[3],&p.x0)||!coord(a[4],&p.y0)) goto bad;
        for(size_t k=5;k<n;k+=2) {
            if(!strcmp(a[k],"-image")) {
                const tg_image_slot *s=tg_image_find(u,a[k+1]);
                if(!s) goto bad;
                p.image_id=s->id;
            } else if(!strcmp(a[k],"-anchor")) {
                if(!option(&p,a[k],a[k+1])||!text_anchor(a[k+1])) goto bad;
            } else if(!option(&p,a[k],a[k+1])) return option_error(b,cap,a[0],a[1],a[k]);
        }
        if(cap<11) goto bad;
        uint32_t id=tg_canvas_item_add(u,p);
        if(!id) return result(b,cap,"canvas item limit or allocation failure",TG_ERROR);
        snprintf(b,cap,"%lu",(unsigned long)id); return TG_OK;
    }
    if(n>=7&&!strcmp(a[2],"window")) {
        tg_canvas_item p={0}; p.owner=owner; p.kind=TG_ITEM_WINDOW; p.text_anchor=5;
        if((n-5)%2||!coord(a[3],&p.x0)||!coord(a[4],&p.y0)) goto bad;
        for(size_t k=5;k<n;k+=2) {
            if(!strcmp(a[k],"-window")) p.window_id=tg_ui_find(u,a[k+1]);
            else if(!option(&p,a[k],a[k+1])) return option_error(b,cap,a[0],a[1],a[k]);
        }
        const tg_widget *child=tg_ui_get(u,p.window_id);
        if(!child||child->parent!=owner||child->layout!=TG_FREE||child->kind==TG_MENU||child->kind==TG_TOPLEVEL||
           tg_canvas_window_item(u,owner,child->id)||cap<11) goto bad;
        uint32_t id=tg_canvas_item_add(u,p);
        if(!id) return result(b,cap,"canvas item limit or allocation failure",TG_ERROR);
        snprintf(b,cap,"%lu",(unsigned long)id); return TG_OK;
    }
    if(n>=5&&!strcmp(a[2],"text")) {
        tg_canvas_item p={0}; p.owner=owner; p.kind=TG_ITEM_TEXT; p.text_scale=1; p.text_anchor=5; p.has_fill=1;
        if((n-5)%2||!coord(a[3],&p.x0)||!coord(a[4],&p.y0)) goto bad;
        for(size_t k=5;k<n;k+=2) if(!option(&p,a[k],a[k+1])) return option_error(b,cap,a[0],a[1],a[k]);
        if(cap<11) goto bad;
        uint32_t id=tg_canvas_item_add(u,p);
        if(!id) return result(b,cap,"canvas item limit or allocation failure",TG_ERROR);
        snprintf(b,cap,"%lu",(unsigned long)id); return TG_OK;
    }
    if(n<7) goto bad;
    tg_canvas_item p={0}; p.owner=owner; p.width=1;
    if(!strcmp(a[2],"line")) { p.kind=TG_ITEM_LINE; p.has_fill=1; }
    else if(!strcmp(a[2],"rectangle")) { p.kind=TG_ITEM_RECT; p.has_outline=1; }
    else if(!strcmp(a[2],"oval")) { p.kind=TG_ITEM_OVAL; p.has_outline=1; }
    else if(!strcmp(a[2],"arc")) { p.kind=TG_ITEM_OVAL; p.has_outline=1; p.is_arc=1; p.arc_start=0.0; p.arc_extent=90.0; p.arc_style=0; }
    else if(!strcmp(a[2],"polygon")) { p.kind=TG_ITEM_POLYGON; p.has_fill=1; p.round_join=1; }
    else goto bad;
    size_t first=7;
    if(p.kind==TG_ITEM_LINE||p.kind==TG_ITEM_POLYGON) {
        first=3;
        int number;
        while(first<n&&coord(a[first],&number)) first++;
        if(!line_points(&p,first-3,a+3)) goto bad;
    } else if(!coord(a[3],&p.x0)||!coord(a[4],&p.y0)||!coord(a[5],&p.x1)||!coord(a[6],&p.y1)) goto bad;
    if((n-first)%2) goto bad;
    for(size_t k=first;k<n;k+=2) if(!option(&p,a[k],a[k+1])) return option_error(b,cap,a[0],a[1],a[k]);
    if(cap<11) goto bad;
    uint32_t id=tg_canvas_item_add(u,p);
    if(!id) return result(b,cap,"canvas item limit or allocation failure",TG_ERROR);
    snprintf(b,cap,"%lu",(unsigned long)id); return TG_OK;
bad:
    snprintf(b,cap,"unsupported canvas operation %.80s on %.160s (argument %.80s)",a[1],a[0],n>2?a[2]:"");
    return TG_ERROR;
}
