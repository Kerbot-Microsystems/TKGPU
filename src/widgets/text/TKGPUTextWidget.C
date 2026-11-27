#include "TKGPUTextWidget.H"
#include "TKGPUTextHistory.H"
#include <limits.h>
#include "TKGPUUtf8.H"
#include "TKGPUText.H"
#include "TKGPURaster.H"
#include "../../core/TKGPUList.H"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
enum { LIMIT=1024*1024, COL=6 };
#define ROW tg_text_row_height(12)
enum { TEXT_TAGS=32, TEXT_MARKS=32 };
typedef struct { char name[64]; size_t pos; int left; } text_mark;
typedef struct { char name[64]; uint32_t bg,fg; int has_bg,has_fg; } text_tag;
struct tg_text_document {
    char *bytes; size_t length,cursor,sel_anchor,capacity; int top,left,line_count,modified;
    int preferred_x,preferred_x_valid; size_t preferred_cursor;
    tg_visual_cursor visual; size_t visual_line_start;
    uint32_t *tags; text_tag style[TEXT_TAGS]; unsigned tag_count;
    text_mark marks[TEXT_MARKS]; unsigned mark_count;
    tg_text_history history;
};
static int reserve(tg_text_document *d,size_t needed) {
    if(needed<=d->capacity) return 1;
    if(needed>LIMIT+2u) return 0;
    size_t cap=d->capacity?d->capacity:256;
    while(cap<needed) { cap*=2; if(cap>LIMIT+2u) cap=LIMIT+2u; }
    char *bytes=calloc(cap,1); uint32_t *tags=calloc(cap,sizeof *tags);
    if(!bytes||!tags) { free(bytes); free(tags); return 0; }
    if(d->bytes) memcpy(bytes,d->bytes,d->length+1);
    if(d->tags) memcpy(tags,d->tags,(d->length+1)*sizeof *tags);
    free(d->bytes); free(d->tags); d->bytes=bytes; d->tags=tags; d->capacity=cap;
    return 1;
}
tg_text_document *tg_document_create(void) {
    tg_text_document *d=calloc(1,sizeof *d);
    if(!d) return NULL;
    if(!reserve(d,2)) { free(d); return NULL; }
    d->bytes[0]='\n'; d->length=1; d->line_count=1; d->history.automatic=1; return d;
}
tg_text_document *tg_document_clone(const tg_text_document *d) {
    if(!d) return NULL;
    tg_text_document *p=malloc(sizeof *p); if(!p) return NULL;
    *p=*d; p->bytes=NULL; p->tags=NULL; p->capacity=0;
    /* Render snapshots do not duplicate the edit journal or own its pointers. */
    p->history.first=p->history.last=p->history.applied=NULL;
    p->history.bytes=p->history.count=p->history.groups=0; p->history.separate=1;
    if(!reserve(p,d->length+1)) { free(p); return NULL; }
    memcpy(p->bytes,d->bytes,d->length+1);
    memcpy(p->tags,d->tags,(d->length+1)*sizeof *p->tags);
    return p;
}
void tg_document_destroy(tg_text_document *d) { if(d) { tg_history_reset(&d->history); free(d->bytes); free(d->tags); free(d); } }
static size_t next(const tg_text_document *d,size_t p) {
    if(p>=d->length) return d->length;
    uint32_t scalar; size_t used;
    if(tg_utf8_decode((const unsigned char *)d->bytes+p,d->length-p,&scalar,&used)!=TG_OK) used=1;
    return p+used;
}
static size_t prev(const tg_text_document *d,size_t p) {
    if(!p) return 0;
    do { p--; } while(p&&((unsigned char)d->bytes[p]&0xc0)==0x80);
    return p;
}
static int decimal(const char **s,int *v) {
    const char *p=*s; int n=0;
    if(*p<'0'||*p>'9') return 0;
    while(*p>='0'&&*p<='9') { n=n*10+(*p++-'0'); if(n>100000) return 0; }
    *s=p; *v=n; return 1;
}
static size_t location(const tg_text_document *d,int row,int col) {
    size_t p=0;
    while(row>1&&p<d->length) { if(d->bytes[p++]=='\n') row--; }
    while(col>0&&p<d->length&&d->bytes[p]!='\n') { p=next(d,p); col--; }
    return p;
}
static void coords(const tg_text_document *d,size_t p,int *r,int *c) {
    *r=1; *c=0;
    for(size_t k=0;k<p&&k<d->length;k=next(d,k)) {
        if(d->bytes[k]=='\n') { (*r)++; *c=0; } else (*c)++;
    }
}
static int mark_find(const tg_text_document *d,const char *name) {
    for(unsigned k=0;k<d->mark_count;k++) if(!strcmp(d->marks[k].name,name)) return (int)k;
    return -1;
}
static int mark_prefix(const tg_text_document *d,const char **s,size_t *p) {
    size_t best=0;
    for(unsigned k=0;k<d->mark_count;k++) {
        size_t len=strlen(d->marks[k].name);
        if(len>best&&!strncmp(*s,d->marks[k].name,len)&&
           (!(*s)[len]||(*s)[len]==' '||(*s)[len]=='+'||(*s)[len]=='-')) {
            best=len; *p=d->marks[k].pos;
        }
    }
    if(!best) return 0;
    *s+=best; return 1;
}
static int index_of(const tg_text_document *d,const char *s,size_t *p) {
    if(!strncmp(s,"end",3)) { *p=d->length; s+=3; }
    else if(!strncmp(s,"insert",6)) { *p=d->cursor; s+=6; }
    else if(!strncmp(s,"sel.first",9)||!strncmp(s,"sel.last",8)) {
        int last=!strncmp(s,"sel.last",8),tag=-1;
        for(unsigned k=0;k<d->tag_count;k++) if(!strcmp(d->style[k].name,"sel")) { tag=(int)k; break; }
        if(tag<0) return 0;
        uint32_t mask=UINT32_C(1)<<tag;
        size_t at=last?d->length:0;
        if(last) {
            while(at&&!(d->tags[at-1]&mask)) at--;
            if(!at) return 0;
        } else {
            while(at<d->length&&!(d->tags[at]&mask)) at=next(d,at);
            if(at==d->length) return 0;
        }
        *p=at; s+=last?8:9;
    }
    else if(!mark_prefix(d,&s,p)) {
        int r,c;
        if(!decimal(&s,&r)||*s++!='.'||r<1) return 0;
        if(!strncmp(s,"end",3)) {
            *p=location(d,r,0);
            while(*p<d->length&&d->bytes[*p]!='\n') *p=next(d,*p);
            s+=3;
        } else {
            if(!decimal(&s,&c)) return 0;
            *p=location(d,r,c);
        }
    }
    /* Apply modifiers from left to right. Saturate counts at the document
       bound rather than overflowing or iterating beyond the possible range. */
    for(;;) {
        while(*s==' '||*s=='\t') s++;
        if(!*s) return 1;
        if(!strncmp(s,"linestart",9)&&(!s[9]||s[9]==' '||s[9]=='\t'||s[9]=='+'||s[9]=='-')) {
            while(*p&&d->bytes[*p-1]!='\n') *p=prev(d,*p);
            s+=9; continue;
        }
        if(!strncmp(s,"lineend",7)&&(!s[7]||s[7]==' '||s[7]=='\t'||s[7]=='+'||s[7]=='-')) {
            while(*p<d->length&&d->bytes[*p]!='\n') *p=next(d,*p);
            s+=7; continue;
        }
        int sign=*s++;
        if(sign!='+'&&sign!='-') return 0;
        while(*s==' '||*s=='\t') s++;
        if(*s<'0'||*s>'9') return 0;
        int n=0;
        while(*s>='0'&&*s<='9') {
            int digit=*s++-'0';
            n=n>(LIMIT+2-digit)/10?LIMIT+2:n*10+digit;
        }
        while(*s==' '||*s=='\t') s++;
        const char *unit=s;
        while(*s>='a'&&*s<='z') s++;
        size_t len=(size_t)(s-unit);
        if((len==1&&*unit=='l')||(len==4&&!memcmp(unit,"line",4))||(len==5&&!memcmp(unit,"lines",5))) {
            int row,col; coords(d,*p,&row,&col);
            row+=sign=='+'?n:-n;
            if(row<1) row=1;
            if(row>d->line_count) *p=d->length;
            else *p=location(d,row,col);
        } else if((len==1&&*unit=='c')||(len==4&&!memcmp(unit,"char",4))||
                  (len==5&&!memcmp(unit,"chars",5))||(len==7&&!memcmp(unit,"indices",7))) {
            while(n--&&((sign=='+'&&*p<d->length)||(sign=='-'&&*p)))
                *p=sign=='+'?next(d,*p):prev(d,*p);
        } else return 0;
    }
}
/* Raw application: callers have reserved the largest required capacity. */
static void splice_raw(tg_text_document *d,size_t a,size_t b,const char *s,size_t n,uint32_t mask) {
    /* The final newline is never removed; update only the changed span. */
    int delta=0;
    for(size_t j=a;j<b;j++) if(d->bytes[j]=='\n') delta--;
    for(size_t j=0;j<n;j++) if(s[j]=='\n') delta++;
    d->line_count+=delta;
    memmove(d->tags+a+n,d->tags+b,(d->length-b+1)*sizeof d->tags[0]);
    for(size_t j=0;j<n;j++) d->tags[a+j]=mask;
    memmove(d->bytes+a+n,d->bytes+b,d->length-b+1);
    memcpy(d->bytes+a,s,n); d->length=d->length-(b-a)+n;
    d->visual.valid=d->preferred_x_valid=0;
    if(d->cursor>=b) d->cursor=d->cursor-(b-a)+n;
    else if(d->cursor>=a) d->cursor=a+n;
    if(d->cursor>=d->length) d->cursor=d->length-1;
    for(unsigned k=0;k<d->mark_count;k++) {
        text_mark *m=&d->marks[k];
        if(m->pos<a) continue;
        if(m->pos>b) m->pos=m->pos-(b-a)+n;
        else if(m->pos==b&&b>a) m->pos=a+n;
        else m->pos=a+(m->left?0:n);
        if(m->pos>=d->length) m->pos=d->length-1;
    }
}
static int splice_mask(tg_widget *w,size_t a,size_t b,const char *s,uint32_t mask) {
    tg_text_document *d=w->document;
    if(w->state==TG_STATE_DISABLED) return TG_OK;
    if(a>=d->length) a=d->length-1;
    if(b>=d->length) b=d->length-1;
    if(b<a) b=a;
    size_t n=strlen(s),count;
    if(n>LIMIT-(d->length-1-(b-a))||tg_utf8_count((const unsigned char *)s,n,&count)!=TG_OK) return TG_ERROR;
    if(a==b&&!n) return TG_OK;
    if(!reserve(d,d->length-(b-a)+n+1)) return TG_ERROR;
    if(!tg_history_record(&d->history,a,d->bytes+a,b-a,s,n)) return TG_ERROR;
    splice_raw(d,a,b,s,n,mask);
    d->modified=d->history.revision!=d->history.clean;
    return TG_OK;
}
static int splice(tg_widget *w,size_t a,size_t b,const char *s) {
    return splice_mask(w,a,b,s,0);
}
static int history_replay(tg_widget *w,int redo) {
    tg_text_document *d=w->document;
    tg_text_history *h=&d->history;
    if(!h->enabled||w->state==TG_STATE_DISABLED) return TG_ERROR;
    tg_text_edit *first=redo?tg_history_redo(h):h->applied;
    if(!first) return TG_ERROR;
    uint64_t group=first->group;
    size_t length=d->length,peak=length;
    /* Preflight entire group: no partial undo on allocation failure. */
    for(tg_text_edit *e=first;e&&e->group==group;e=redo?e->next:e->prev) {
        size_t remove=redo?e->old_n:e->new_n,add=redo?e->new_n:e->old_n;
        if(e->at>=length||remove>length-1-e->at||add>LIMIT-(length-1-remove)) return TG_ERROR;
        length=length-remove+add; if(length>peak) peak=length;
    }
    if(!reserve(d,peak+1)) return TG_ERROR;
    for(tg_text_edit *e=first;e&&e->group==group;e=redo?e->next:e->prev) {
        size_t remove=redo?e->old_n:e->new_n,add=redo?e->new_n:e->old_n;
        /* Tk replays untagged insertion: inherit only shared adjacent tags. */
        uint32_t mask=e->at?d->tags[e->at-1]&d->tags[e->at+remove]:0;
        splice_raw(d,e->at,e->at+remove,redo?tg_history_new(e):tg_history_old(e),add,mask);
        h->applied=redo?e:e->prev; h->revision=redo?e->after:e->before;
    }
    h->separate=1;
    d->modified=h->revision!=h->clean;
    return TG_OK;
}
/* Validate configuration before committing changes to the live document. */
static int history_option_id(const char *key) {
    if(!strcmp(key,"-undo")) return 1;
    if(!strcmp(key,"-autoseparators")) return 2;
    if(!strcmp(key,"-maxundo")) return 3;
    return 0;
}
static int history_bool(const char *v,int *flag) {
    if(!strcmp(v,"1")||!strcmp(v,"true")||!strcmp(v,"yes")||!strcmp(v,"on")) { *flag=1; return 1; }
    if(!strcmp(v,"0")||!strcmp(v,"false")||!strcmp(v,"no")||!strcmp(v,"off")) { *flag=0; return 1; }
    return 0;
}
int tg_document_history_option(tg_text_document *d,const char *key,const char *v) {
    int id=history_option_id(key),value;
    if(!id) return -1;
    if(id==3) {
        char *end; errno=0; long x=strtol(v,&end,10);
        if(errno||end==v||*end||x<INT_MIN||x>INT_MAX) return TG_ERROR;
        value=(int)x;
    } else if(!history_bool(v,&value)) return TG_ERROR;
    if(d) {
        if(id==1) d->history.enabled=value;
        else if(id==2) d->history.automatic=value;
        else { d->history.max_groups=value; tg_history_trim(&d->history); }
        d->history.separate=1;
    }
    return TG_OK;
}
int tg_document_history_query(const tg_text_document *d,const char *key,char *out,size_t cap) {
    int id=history_option_id(key);
    if(!id||!d) return -1;
    int z=snprintf(out,cap,"%d",id==1?d->history.enabled:id==2?d->history.automatic:d->history.max_groups);
    return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR;
}
static int lines(const tg_text_document *d) {
    return d->line_count;
}
static int document_row_height(const tg_widget *w) {
    int tw=0,th=0;
    if(tg_text_native_enabled()&&tg_text_font_measure("Mg",w->font_spec,&tw,&th)==TG_OK&&th>0&&th<512) return th+2;
    return ROW;
}
static int visible(const tg_widget *w) {
    int h=document_row_height(w),n=(w->rect.h-4)/h;
    return n>0?n:1;
}
static int visible_width(const tg_widget *w) {
    int avail=w?w->rect.w-6:0;
    return avail>1?avail:1;
}
static int text_native_active(void) {
    return tg_text_native_enabled()&&strcmp(tg_text_native_backend(),"bitmap")!=0;
}
static size_t next_utf8_in(const char *s,size_t len,size_t pos) {
    if(pos>=len) return len;
    uint32_t scalar; size_t used;
    if(tg_utf8_decode((const unsigned char *)s+pos,len-pos,&scalar,&used)!=TG_OK||!used) used=1;
    return pos+used<=len?pos+used:len;
}
static int measure_slice_px(const tg_widget *w,const char *s,size_t len,int native_mode) {
    if(!len) return 0;
    if(native_mode) {
        size_t cap=len<4095?len:4095;
        char buf[4096]; memcpy(buf,s,cap); buf[cap]=0;
        int tw=0,th=0;
        if(tg_text_font_measure(buf,w->font_spec,&tw,&th)==TG_OK&&tw>=0) return tw;
    }
    size_t cols=0;
    if(tg_utf8_count((const unsigned char *)s,len,&cols)!=TG_OK) cols=len;
    return (int)(cols*COL);
}
static size_t next_wrap_segment(const tg_widget *w,const char *line,size_t len,size_t off,int native_mode) {
    if(off>=len) return 0;
    size_t rem=len-off;
    if(!w||w->wrap==0||w->rect.w<=12) return rem;
    int avail=visible_width(w);
    if(measure_slice_px(w,line+off,rem,native_mode)<=avail) return rem;
    size_t cut=off;
    if(!native_mode) {
        int max_cols=avail/COL;
        if(max_cols<1) max_cols=1;
        size_t p=off; int cols=0;
        while(p<len&&cols<max_cols) { p=next_utf8_in(line,len,p); cols++; }
        cut=p;
    } else {
        /* Binary search UTF-8 character boundaries for exact pixel fit. */
        size_t lo=0,hi=0;
        size_t scan=off;
        while(scan<len&&hi<4096) { scan=next_utf8_in(line,len,scan); hi++; }
        size_t best_chars=1;
        lo=1;
        while(lo<=hi) {
            size_t mid=lo+(hi-lo)/2;
            size_t p=off;
            for(size_t k=0;k<mid&&p<len;k++) p=next_utf8_in(line,len,p);
            int tw=measure_slice_px(w,line+off,p-off,1);
            if(tw<=avail) { best_chars=mid; lo=mid+1; }
            else { if(mid==0) break; hi=mid-1; }
        }
        size_t p=off;
        for(size_t k=0;k<best_chars&&p<len;k++) p=next_utf8_in(line,len,p);
        cut=p;
    }
    if(cut<=off) cut=next_utf8_in(line,len,off);
    if(w->wrap==2&&cut<len) {
        size_t ws=cut;
        while(ws>off) {
            size_t prev_p=ws-1;
            while(prev_p>off&&((unsigned char)line[prev_p]&0xc0)==0x80) prev_p--;
            if(line[prev_p]==' '||line[prev_p]=='\t') {
                size_t after_ws=prev_p+1;
                if(after_ws>off) { cut=after_ws; break; }
            }
            ws=prev_p;
        }
    }
    return cut>off?cut-off:next_utf8_in(line,len,off)-off;
}
static int visual_line_count(const tg_widget *w) {
    if(!w||!w->document) return 1;
    const tg_text_document *d=w->document;
    if(w->wrap==0||w->rect.w<=12) return lines(d);
    int native_mode=text_native_active();
    int total=0;
    for(size_t p=0;p<d->length;) {
        size_t n=strcspn(d->bytes+p,"\n");
        if(n==0) { total++; }
        else {
            size_t off=0;
            while(off<n) {
                size_t seg=next_wrap_segment(w,d->bytes+p,n,off,native_mode);
                if(!seg) break;
                off+=seg; total++;
            }
        }
        p+=n+1;
    }
    return total>0?total:1;
}
static int clamp_top(const tg_widget *w,int top) {
    int max=visual_line_count(w)-visible(w); if(max<0) max=0;
    return top<0?0:top>max?max:top;
}
void tg_document_view_fractions(const tg_widget *w,double *first,double *last) {
    *first=0; *last=1;
    if(!w||!w->document) return;
    int total=visual_line_count(w),top=clamp_top(w,w->document->top),end=top+visible(w);
    if(total>0) { *first=(double)top/total; *last=end>=total?1.0:(double)end/total; }
}
static int max_line_width(const tg_widget *w) {
    if(!w||!w->document||w->wrap!=0) return 0;
    const tg_text_document *d=w->document;
    size_t best_start=0,best_len=0;
    for(size_t p=0;p<d->length;) {
        size_t n=strcspn(d->bytes+p,"\n");
        if(n>best_len) { best_len=n; best_start=p; }
        p+=n+1;
    }
    if(!best_len) return 0;
    if(text_native_active()) {
        size_t cap=best_len<4095?best_len:4095;
        char buf[4096]; memcpy(buf,d->bytes+best_start,cap); buf[cap]=0;
        int tw=0,th=0;
        if(tg_text_font_measure(buf,w->font_spec,&tw,&th)==TG_OK&&tw>0) return tw;
    }
    size_t cols=0;
    if(tg_utf8_count((const unsigned char *)d->bytes+best_start,best_len,&cols)!=TG_OK) cols=best_len;
    return (int)(cols*COL);
}
static int clamp_left(const tg_widget *w,int left) {
    int max=max_line_width(w)-visible_width(w);
    if(max<0) max=0;
    return left<0?0:left>max?max:left;
}
void tg_document_xview_fractions(const tg_widget *w,double *first,double *last) {
    *first=0; *last=1;
    if(!w||!w->document) return;
    int total=max_line_width(w),avail=visible_width(w);
    if(total<=avail||total<=0) return;
    int left=clamp_left(w,w->document->left);
    *first=(double)left/total;
    int end=left+avail;
    *last=end>=total?1.0:(double)end/total;
}
static void see(tg_widget *w,size_t p) {
    int r,c; coords(w->document,p,&r,&c); (void)c; r--;
    int top=w->document->top;
    if(r<top) top=r; else if(r>=top+visible(w)) top=r-visible(w)+1;
    w->document->top=clamp_top(w,top);
}
static int output(const char *s,size_t n,char *out,size_t cap) {
    if(n>=cap) return TG_ERROR;
    memcpy(out,s,n); out[n]=0; return TG_OK;
}
/* Simple Unicode case mapping for the Latin/Cyrillic alphabets used by
   the application. Exact searches compare UTF-8 without folding. */
static uint32_t search_fold(uint32_t v) {
    if((v>='A'&&v<='Z')||(v>=0x00c0&&v<=0x00d6)||(v>=0x00d8&&v<=0x00de)||(v>=0x0410&&v<=0x042f)) return v+32;
    if(v>=0x0400&&v<=0x040f) return v+80;
    if(v==0x0178) return 0x00ff;
    if(v==0x1e9e) return 0x00df;
    return v;
}
static int regex_is_word(unsigned char c) {
    return (c>='0'&&c<='9')||(c>='A'&&c<='Z')||(c>='a'&&c<='z')||c=='_';
}
static const char *regex_find_alt_end(const char *p,const char *end) {
    int depth=0,in_class=0;
    while(p<end) {
        if(*p=='\\'&&p+1<end) { p+=2; continue; }
        if(in_class) { if(*p==']') in_class=0; p++; continue; }
        if(*p=='[') in_class=1;
        else if(*p=='(') depth++;
        else if(*p==')') { if(!depth) break; depth--; }
        else if(*p=='|'&&!depth) break;
        p++;
    }
    return p;
}
static int regex_match_class(const char *pat,size_t len,unsigned char c,int nocase) {
    int negate=0; size_t i=0;
    if(i<len&&pat[i]=='^') { negate=1; i++; }
    int matched=0;
    unsigned char cf=(unsigned char)search_fold(c);
    while(i<len) {
        unsigned char lo=(unsigned char)pat[i]; int is_lo_esc=0;
        if(lo=='\\'&&i+1<len) {
            unsigned char esc=(unsigned char)pat[i+1];
            if(esc=='w'||esc=='W'||esc=='d'||esc=='D'||esc=='s'||esc=='S') {
                int hit=0;
                if(esc=='w'||esc=='W') hit=regex_is_word(c);
                else if(esc=='d'||esc=='D') hit=(c>='0'&&c<='9');
                else hit=(c==' '||c=='\t'||c=='\r'||c=='\n'||c=='\v'||c=='\f');
                if(esc=='W'||esc=='D'||esc=='S') hit=!hit;
                if(hit) matched=1;
                i+=2; continue;
            }
            if(esc=='n') lo='\n'; else if(esc=='t') lo='\t'; else if(esc=='r') lo='\r';
            else lo=esc;
            i+=2; is_lo_esc=1;
        } else i++;
        if(!is_lo_esc&&i+1<len&&pat[i]=='-'&&pat[i+1]!=']') {
            i++;
            unsigned char hi=(unsigned char)pat[i];
            if(hi=='\\'&&i+1<len) { hi=(unsigned char)pat[i+1]; i+=2; } else i++;
            unsigned char lof=nocase?(unsigned char)search_fold(lo):lo;
            unsigned char hif=nocase?(unsigned char)search_fold(hi):hi;
            unsigned char cv=nocase?cf:c;
            if(lof<=hif&&cv>=lof&&cv<=hif) matched=1;
        } else {
            if(nocase?(cf==(unsigned char)search_fold(lo)):(c==lo)) matched=1;
        }
    }
    return negate?!matched:matched;
}
static int regex_match_seq(const tg_text_document *d,const char *p,const char *pend,size_t pos,size_t stop,int nocase,size_t *out_end,int depth);
static int regex_match_alt(const tg_text_document *d,const char *p,const char *pend,size_t pos,size_t stop,int nocase,size_t *out_end,int depth) {
    if(depth>32) return 0;
    const char *cur=p;
    for(;;) {
        const char *bar=regex_find_alt_end(cur,pend);
        if(regex_match_seq(d,cur,bar,pos,stop,nocase,out_end,depth+1)) return 1;
        if(bar>=pend) break;
        cur=bar+1;
    }
    return 0;
}
static int regex_match_atom(const tg_text_document *d,const char *atom,const char *atom_end,size_t pos,size_t stop,int nocase,size_t *next_pos,int depth) {
    if(atom>=atom_end) return 0;
    if(*atom=='(') return regex_match_alt(d,atom+1,atom_end-1,pos,stop,nocase,next_pos,depth+1);
    if(pos>=stop) return 0;
    unsigned char c=(unsigned char)d->bytes[pos];
    if(*atom=='.') {
        if(c=='\n') return 0;
        *next_pos=next(d,pos); return 1;
    }
    if(*atom=='[') {
        if(!regex_match_class(atom+1,(size_t)(atom_end-atom-2),c,nocase)) return 0;
        *next_pos=next(d,pos); return 1;
    }
    if(*atom=='\\'&&atom+1<atom_end) {
        unsigned char esc=(unsigned char)atom[1];
        if(esc=='w'||esc=='W') { int ok=regex_is_word(c); if(esc=='W') ok=!ok; if(!ok) return 0; *next_pos=next(d,pos); return 1; }
        if(esc=='d'||esc=='D') { int ok=(c>='0'&&c<='9'); if(esc=='D') ok=!ok; if(!ok) return 0; *next_pos=next(d,pos); return 1; }
        if(esc=='s'||esc=='S') { int ok=(c==' '||c=='\t'||c=='\r'||c=='\n'||c=='\v'||c=='\f'); if(esc=='S') ok=!ok; if(!ok) return 0; *next_pos=next(d,pos); return 1; }
        unsigned char target=esc=='n'?'\n':esc=='t'?'\t':esc=='r'?'\r':esc;
        if(nocase?(search_fold(c)!=search_fold(target)):(c!=target)) return 0;
        *next_pos=next(d,pos); return 1;
    }
    if(nocase?(search_fold(c)!=search_fold((unsigned char)*atom)):(c!=(unsigned char)*atom)) return 0;
    *next_pos=next(d,pos); return 1;
}
static int regex_match_seq(const tg_text_document *d,const char *p,const char *pend,size_t pos,size_t stop,int nocase,size_t *out_end,int depth) {
    if(depth>48) return 0;
    if(p>=pend) { *out_end=pos; return 1; }
    if(*p=='^') {
        if(pos!=0&&d->bytes[pos-1]!='\n') return 0;
        return regex_match_seq(d,p+1,pend,pos,stop,nocase,out_end,depth);
    }
    if(*p=='$') {
        if(pos<stop&&d->bytes[pos]!='\n') return 0;
        return regex_match_seq(d,p+1,pend,pos,stop,nocase,out_end,depth);
    }
    if(*p=='\\'&&p+1<pend&&(p[1]=='b'||p[1]=='B')) {
        int left=pos>0&&regex_is_word((unsigned char)d->bytes[pos-1]);
        int right=pos<d->length&&regex_is_word((unsigned char)d->bytes[pos]);
        int at_bound=(left!=right);
        if(p[1]=='b'?!at_bound:at_bound) return 0;
        return regex_match_seq(d,p+2,pend,pos,stop,nocase,out_end,depth);
    }
    const char *atom=p,*atom_end=p+1;
    if(*p=='\\'&&p+1<pend) atom_end=p+2;
    else if(*p=='[') {
        const char *q=p+1;
        if(q<pend&&*q=='^') q++;
        if(q<pend&&*q==']') q++;
        while(q<pend&&*q!=']') { if(*q=='\\'&&q+1<pend) q+=2; else q++; }
        if(q>=pend) return 0;
        atom_end=q+1;
    } else if(*p=='(') {
        const char *q=p+1; int dpar=1,inc=0;
        while(q<pend&&dpar>0) {
            if(*q=='\\'&&q+1<pend) { q+=2; continue; }
            if(inc) { if(*q==']') inc=0; q++; continue; }
            if(*q=='[') inc=1;
            else if(*q=='(') dpar++;
            else if(*q==')') dpar--;
            q++;
        }
        if(dpar!=0) return 0;
        atom_end=q;
    }
    const char *rest=atom_end;
    int min_rep=1,max_rep=1,lazy=0;
    if(rest<pend&&(*rest=='*'||*rest=='+'||*rest=='?')) {
        if(*rest=='*') { min_rep=0; max_rep=100000; }
        else if(*rest=='+') { min_rep=1; max_rep=100000; }
        else { min_rep=0; max_rep=1; }
        rest++;
        if(rest<pend&&*rest=='?') { lazy=1; rest++; }
    }
    if(min_rep==1&&max_rep==1) {
        size_t np=pos;
        if(!regex_match_atom(d,atom,atom_end,pos,stop,nocase,&np,depth)) return 0;
        return regex_match_seq(d,rest,pend,np,stop,nocase,out_end,depth);
    }
    if(lazy) {
        size_t cur=pos;
        for(int count=0;count<=max_rep;count++) {
            if(count>=min_rep&&regex_match_seq(d,rest,pend,cur,stop,nocase,out_end,depth+1)) return 1;
            size_t np=cur;
            if(!regex_match_atom(d,atom,atom_end,cur,stop,nocase,&np,depth)||np==cur) break;
            cur=np;
        }
        return 0;
    }
    size_t positions[256]; int npos=0; positions[0]=pos;
    size_t cur=pos;
    while(npos<255&&npos<max_rep) {
        size_t np=cur;
        if(!regex_match_atom(d,atom,atom_end,cur,stop,nocase,&np,depth)||np==cur) break;
        cur=np; positions[++npos]=cur;
    }
    for(int c=npos;c>=min_rep;c--) {
        if(regex_match_seq(d,rest,pend,positions[c],stop,nocase,out_end,depth+1)) return 1;
    }
    return 0;
}
static int search_matches(const tg_text_document *d,size_t p,size_t stop,const char *needle,int nocase,int is_regexp) {
    if(is_regexp) {
        size_t dummy=p;
        return regex_match_alt(d,needle,needle+strlen(needle),p,stop,nocase,&dummy,0);
    }
    size_t n=strlen(needle),k=0;
    if(!nocase) return n<=stop-p&&!memcmp(d->bytes+p,needle,n);
    while(k<n&&p<stop) {
        uint32_t a,b; size_t na,nb;
        if(tg_utf8_decode((const unsigned char *)d->bytes+p,stop-p,&a,&na)!=TG_OK||
           tg_utf8_decode((const unsigned char *)needle+k,n-k,&b,&nb)!=TG_OK||
           search_fold(a)!=search_fold(b)) return 0;
        p+=na; k+=nb;
    }
    return k==n;
}
static int document_search(const tg_text_document *d,size_t n,char **a,char *out,size_t cap) {
    size_t k=2,start,stop=d->length; int nocase=0,backwards=0,is_regexp=0;
    while(k<n&&a[k][0]=='-') {
        if(!strcmp(a[k],"--")) { k++; break; }
        if(!strcmp(a[k],"-nocase")) nocase=1;
        else if(!strcmp(a[k],"-backwards")) backwards=1;
        else if(!strcmp(a[k],"-forwards")) backwards=0;
        else if(!strcmp(a[k],"-regexp")) is_regexp=1;
        else if(!strcmp(a[k],"-exact")) is_regexp=0;
        else return TG_ERROR;
        k++;
    }
    if((n-k!=2&&n-k!=3)||!index_of(d,a[k+1],&start)) return TG_ERROR;
    if(backwards) stop=0;
    if(n-k==3&&!index_of(d,a[k+2],&stop)) return TG_ERROR;
    if(!*a[k]) return output("",0,out,cap);
    size_t match=d->length;
    if(backwards) {
        size_t p=start;
        while(p>=stop) {
            if(p<d->length&&search_matches(d,p,d->length,a[k],nocase,is_regexp)) { match=p; break; }
            if(!p) break;
            p=prev(d,p);
        }
    } else for(size_t p=start;p<stop;p=next(d,p)) {
        if(search_matches(d,p,stop,a[k],nocase,is_regexp)) { match=p; break; }
    }
    if(match==d->length) return output("",0,out,cap);
    int row,col; coords(d,match,&row,&col);
    int z=snprintf(out,cap,"%d.%d",row,col);
    return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR;
}
static int tag_index(tg_text_document *d,const char *name,int create) {
    for(unsigned k=0;k<d->tag_count;k++) if(!strcmp(d->style[k].name,name)) return (int)k;
    if(!create||!*name||strlen(name)>=sizeof d->style[0].name||d->tag_count==TEXT_TAGS) return -1;
    unsigned k=d->tag_count++;
    strcpy(d->style[k].name,name);
    if(!strcmp(name,"sel")) {
        d->style[k].has_bg=1; d->style[k].bg=0x2563eb;
        d->style[k].has_fg=1; d->style[k].fg=0xffffff;
    }
    return (int)k;
}
static int tag_color(const char *s,uint32_t *color) {
    if(!strcmp(s,"yellow")) *color=0xffff00;
    else if(!strcmp(s,"black")) *color=0;
    else if(!strcmp(s,"white")) *color=0xffffff;
    else if(!strcmp(s,"red")) *color=0xff0000;
    else if(!strcmp(s,"blue")) *color=0x0000ff;
    else if(!strcmp(s,"green")) *color=0x00ff00;
    else if(s[0]=='#'&&strlen(s)==7&&strspn(s+1,"0123456789abcdefABCDEF")==6) *color=(uint32_t)strtoul(s+1,NULL,16);
    else return 0;
    return 1;
}
/* Named marks use byte offsets internally but share UTF-8 index resolution.
   The fixed capacity avoids allocations during edits and snapshot cloning. */
static int document_mark(tg_text_document *d,size_t n,char **a,char *out,size_t cap) {
    if(n<3) return TG_ERROR;
    if(!strcmp(a[2],"names")&&n==3) {
        out[0]=0;
        if(!tg_list_append(out,cap,"insert")) return TG_ERROR;
        for(unsigned k=0;k<d->mark_count;k++)
            if(!tg_list_append(out,cap,d->marks[k].name)) return TG_ERROR;
        return TG_OK;
    }
    if(n<4) return TG_ERROR;
    if(!strcmp(a[2],"set")&&n==5) {
        size_t p;
        if(!index_of(d,a[4],&p)) return TG_ERROR;
        if(p>=d->length) p=d->length-1;
        if(!strcmp(a[3],"insert")) { d->cursor=p; d->visual.valid=d->preferred_x_valid=0; return output("",0,out,cap); }
        /* Do not shadow built-in or numeric text indices. */
        if(!*a[3]||strlen(a[3])>=sizeof d->marks[0].name||
           !strncmp(a[3],"end",3)||!strncmp(a[3],"insert",6)||
           !strcmp(a[3],"current")||strpbrk(a[3]," \t\r\n+-")||
           (a[3][0]>='0'&&a[3][0]<='9')) return TG_ERROR;
        int k=mark_find(d,a[3]);
        if(k<0) {
            if(d->mark_count==TEXT_MARKS) return TG_ERROR;
            k=(int)d->mark_count++;
            memset(&d->marks[k],0,sizeof d->marks[k]);
            strcpy(d->marks[k].name,a[3]);
        }
        d->marks[k].pos=p;
        return output("",0,out,cap);
    }
    if(!strcmp(a[2],"unset")) {
        for(size_t j=3;j<n;j++) {
            int k=mark_find(d,a[j]);
            if(k>=0) {
                memmove(d->marks+k,d->marks+k+1,(d->mark_count-(unsigned)k-1)*sizeof d->marks[0]);
                d->mark_count--;
            }
        }
        return output("",0,out,cap);
    }
    if(!strcmp(a[2],"gravity")&&(n==4||n==5)) {
        int k=mark_find(d,a[3]);
        if(k<0) {
            if(n==4&&!strcmp(a[3],"insert")) return output("right",5,out,cap);
            return TG_ERROR;
        }
        if(n==4) return d->marks[k].left?output("left",4,out,cap):output("right",5,out,cap);
        if(strcmp(a[4],"left")&&strcmp(a[4],"right")) return TG_ERROR;
        d->marks[k].left=!strcmp(a[4],"left");
        return output("",0,out,cap);
    }
    return TG_ERROR;
}
static void step_coords(const tg_text_document *d,size_t *p,int *row,int *col) {
    if(*p>=d->length) return;
    if(d->bytes[*p]=='\n') { (*row)++; *col=0; } else (*col)++;
    *p=next(d,*p);
}
/* Range bounds constrain the beginning, not the ending position. */
static int tag_neighbor(tg_text_document *d,size_t n,char **a,char *out,size_t cap) {
    if(n!=5&&n!=6) return TG_ERROR;
    int backwards=!strcmp(a[2],"prevrange");
    size_t start,stop=backwards?0:d->length;
    if(!index_of(d,a[4],&start)||(n==6&&!index_of(d,a[5],&stop))) return TG_ERROR;
    int index=tag_index(d,a[3],0);
    if(index<0||(!backwards&&start>=stop)||(backwards&&start<=stop))
        return output("",0,out,cap);
    uint32_t bit=UINT32_C(1)<<index;
    size_t first,last;
    if(!backwards) {
        first=start;
        if(first&&first<d->length&&(d->tags[first]&bit)&&(d->tags[prev(d,first)]&bit))
            while(first<d->length&&(d->tags[first]&bit)) first=next(d,first);
        while(first<stop&&!(d->tags[first]&bit)) first=next(d,first);
        if(first>=stop) return output("",0,out,cap);
    } else {
        first=start;
        do { first=prev(d,first); } while(first>stop&&!(d->tags[first]&bit));
        if(!(d->tags[first]&bit)) return output("",0,out,cap);
        while(first&&(d->tags[prev(d,first)]&bit)) first=prev(d,first);
        if(first<stop) return output("",0,out,cap);
    }
    last=first;
    while(last<d->length&&(d->tags[last]&bit)) last=next(d,last);
    int row,col,row2,col2;
    coords(d,first,&row,&col); row2=row; col2=col;
    for(size_t p=first;p<last;) step_coords(d,&p,&row2,&col2);
    int z=snprintf(out,cap,"%d.%d %d.%d",row,col,row2,col2);
    return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR;
}
static int document_tag(tg_text_document *d,size_t n,char **a,char *out,size_t cap) {
    if(n<3||!cap) return TG_ERROR;
    if(!strcmp(a[2],"nextrange")||!strcmp(a[2],"prevrange"))
        return tag_neighbor(d,n,a,out,cap);
    if(!strcmp(a[2],"names")&&(n==3||n==4)) {
        size_t p=0; uint32_t mask=UINT32_MAX;
        if(n==4) {
            if(!index_of(d,a[3],&p)) return TG_ERROR;
            mask=p<d->length?d->tags[p]:0;
        }
        out[0]=0;
        for(unsigned k=0;k<d->tag_count;k++)
            if((mask&(UINT32_C(1)<<k))&&!tg_list_append(out,cap,d->style[k].name)) return TG_ERROR;
        return TG_OK;
    }
    if(n<4) return TG_ERROR;
    if(!strcmp(a[2],"delete")) {
        for(size_t j=3;j<n;j++) {
            int k=tag_index(d,a[j],0);
            if(k<0) continue;
            uint32_t bit=UINT32_C(1)<<k,lower=bit-1;
            for(size_t p=0;p<d->length;p++)
                d->tags[p]=(d->tags[p]&lower)|((d->tags[p]>>1)&~lower);
            memmove(d->style+k,d->style+k+1,(d->tag_count-(unsigned)k-1)*sizeof d->style[0]);
            d->tag_count--;
            memset(&d->style[d->tag_count],0,sizeof d->style[0]);
        }
        return output("",0,out,cap);
    }
    if(!strcmp(a[2],"cget")&&n==5) {
        int k=tag_index(d,a[3],0); if(k<0) return TG_ERROR;
        const text_tag *t=&d->style[k]; uint32_t value;
        if(!strcmp(a[4],"-background")) { if(!t->has_bg) return output("",0,out,cap); value=t->bg; }
        else if(!strcmp(a[4],"-foreground")) { if(!t->has_fg) return output("",0,out,cap); value=t->fg; }
        else return TG_ERROR;
        int z=snprintf(out,cap,"#%06x",(unsigned)value);
        return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR;
    }
    int add=!strcmp(a[2],"add"),remove=!strcmp(a[2],"remove");
    if(add||remove) {
        if(n<5) return TG_ERROR;
        /* Validate every range before allocating a tag or editing any bytes. */
        for(size_t k=4;k<n;k+=2) {
            size_t p,q;
            if(!index_of(d,a[k],&p)||(k+1<n&&!index_of(d,a[k+1],&q))) return TG_ERROR;
        }
        int index=tag_index(d,a[3],add);
        if(index<0) return remove?output("",0,out,cap):TG_ERROR;
        uint32_t bit=UINT32_C(1)<<index;
        for(size_t k=4;k<n;k+=2) {
            size_t p,q;
            if(!index_of(d,a[k],&p)) return TG_ERROR;
            if(k+1<n) { if(!index_of(d,a[k+1],&q)) return TG_ERROR; }
            else q=next(d,p);
            for(size_t j=p;j<q;j++) if(add) d->tags[j]|=bit; else d->tags[j]&=~bit;
        }
        return output("",0,out,cap);
    }
    if(!strcmp(a[2],"configure")||!strcmp(a[2],"config")) {
        if(n%2) return TG_ERROR;
        int index=tag_index(d,a[3],0);
        text_tag pending={0};
        if(index>=0) pending=d->style[index];
        text_tag *t=&pending;
        for(size_t k=4;k<n;k+=2) {
            if(!strcmp(a[k],"-background")) {
                if(*a[k+1]&&!tag_color(a[k+1],&t->bg)) return TG_ERROR;
                t->has_bg=!!*a[k+1];
            } else if(!strcmp(a[k],"-foreground")) {
                if(*a[k+1]&&!tag_color(a[k+1],&t->fg)) return TG_ERROR;
                t->has_fg=!!*a[k+1];
            }
            /* Other typography options retain the existing compatibility behavior. */
        }
        if(index<0) {
            index=tag_index(d,a[3],1); if(index<0) return TG_ERROR;
            strcpy(pending.name,d->style[index].name);
        }
        d->style[index]=pending;
        return output("",0,out,cap);
    }
    if(!strcmp(a[2],"ranges")&&n==4) {
        int index=tag_index(d,a[3],0); out[0]=0; if(index<0) return TG_OK;
        uint32_t bit=UINT32_C(1)<<index; size_t used=0;
        int row=1,col=0;
        for(size_t p=0;p<d->length;) {
            if(!(d->tags[p]&bit)) { step_coords(d,&p,&row,&col); continue; }
            int r=row,c=col;
            do { step_coords(d,&p,&row,&col); } while(p<d->length&&(d->tags[p]&bit));
            int z=snprintf(out+used,cap-used,"%s%d.%d %d.%d",used?" ":"",r,c,row,col);
            if(z<0||(size_t)z>=cap-used) return TG_ERROR;
            used+=(size_t)z;
        }
        return TG_OK;
    }
    return output("",0,out,cap);
}
static void tag_colors(const tg_text_document *d,uint32_t mask,uint32_t *fg,uint32_t *bg,int *has_bg) {
    *has_bg=0;
    for(unsigned k=0;k<d->tag_count;k++) if(mask&(UINT32_C(1)<<k)) {
        const text_tag *t=&d->style[k];
        if(t->has_bg) { *bg=t->bg; *has_bg=1; }
        if(t->has_fg) *fg=t->fg;
    }
}
/* Count logical characters/indices and newline boundaries in one pass.
   Display-dependent units are deliberately not approximated. */
static int display_row_at(const tg_widget *w,size_t target) {
    const tg_text_document *d=w->document;
    int row=0,native_mode=text_native_active();
    for(size_t p=0;p<d->length;) {
        size_t length=strcspn(d->bytes+p,"\n"),off=0;
        if(!length) { if(target<=p) return row; row++; }
        while(off<length) {
            size_t segment=next_wrap_segment(w,d->bytes+p,length,off,native_mode);
            if(!segment) return row;
            if(target<p+off+segment||(off+segment==length&&target==p+length)) return row;
            off+=segment; row++;
        }
        p+=length+1;
    }
    return row;
}
static int document_count(const tg_widget *w,size_t n,char **a,char *out,size_t cap) {
    const tg_text_document *d=w->document;
    if(n<4||!cap) return TG_ERROR;
    size_t p,q;
    if(!index_of(d,a[n-2],&p)||!index_of(d,a[n-1],&q)) return TG_ERROR;
    for(size_t j=2;j<n-2;j++)
        if(strcmp(a[j],"-chars")&&strcmp(a[j],"-indices")&&strcmp(a[j],"-lines")&&strcmp(a[j],"-displaylines"))
            return TG_ERROR;
    int displaylines=display_row_at(w,q)-display_row_at(w,p);
    int sign=p>q?-1:1;
    if(p>q) { size_t tmp=p; p=q; q=tmp; }
    int chars=0,lines=0;
    for(;p<q;p=next(d,p)) {
        chars++;
        if(d->bytes[p]=='\n') lines++;
    }
    if(n==4) {
        int z=snprintf(out,cap,"%d",sign*chars);
        return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR;
    }
    out[0]=0;
    for(size_t j=2;j<n-2;j++) {
        char number[32];
        snprintf(number,sizeof number,"%d",!strcmp(a[j],"-displaylines")?displaylines:sign*(!strcmp(a[j],"-lines")?lines:chars));
        if(!tg_list_append(out,cap,number)) return TG_ERROR;
    }
    return TG_OK;
}
static size_t hit_offset(tg_widget *w,int x,int y);
static int index_of_widget(tg_widget *w,const char *s,size_t *p) {
    if(!w||!w->document||!s) return 0;
    if(s[0]=='@') {
        char *end=NULL;
        long x=strtol(s+1,&end,10);
        if(!end||end==s+1||*end!=',') return 0;
        char *end2=NULL;
        long y=strtol(end+1,&end2,10);
        if(!end2||end2==end+1) return 0;
        *p=hit_offset(w,w->rect.x+(int)x,w->rect.y+(int)y);
        return 1;
    }
    return index_of(w->document,s,p);
}
int tg_document_command(tg_widget *w,size_t n,char **a,char *out,size_t cap) {
    if(w->kind!=TG_TEXT||n<2) return -1;
    tg_text_document *d=w->document;
    if(!d||!cap) return TG_ERROR;
    const char *op=a[1]; size_t p,q;
    if(!strcmp(op,"edit")) {
        if(n<3) return TG_ERROR;
        const char *edit=a[2];
        tg_text_history *h=&d->history;
        if(!strcmp(edit,"modified")) {
            if(n==3) return output(d->modified?"1":"0",1,out,cap);
            int flag;
            if(n!=4||!history_bool(a[3],&flag)) return TG_ERROR;
            d->modified=flag; h->clean=flag?UINT64_MAX:h->revision;
            h->separate=1; return output("",0,out,cap);
        }
        if(n!=3) return TG_ERROR;
        if(!strcmp(edit,"canundo")||!strcmp(edit,"canredo")) {
            int yes=h->enabled&&(!strcmp(edit,"canundo")?h->applied!=NULL:tg_history_redo(h)!=NULL);
            return output(yes?"1":"0",1,out,cap);
        }
        if(!strcmp(edit,"separator")) { h->separate=1; return output("",0,out,cap); }
        if(!strcmp(edit,"reset")) { tg_history_reset(h); return output("",0,out,cap); }
        if(!strcmp(edit,"undo")||!strcmp(edit,"redo")) {
            if(history_replay(w,!strcmp(edit,"redo"))!=TG_OK) return TG_ERROR;
            d->top=clamp_top(w,d->top); see(w,d->cursor);
            return output("",0,out,cap);
        }
        return TG_ERROR;
    }
    if(!strcmp(op,"count")) return document_count(w,n,a,out,cap);
    if(!strcmp(op,"compare")) {
        if(n!=5||!index_of_widget(w,a[2],&p)||!index_of_widget(w,a[4],&q)) return TG_ERROR;
        const char *relation=a[3]; int yes;
        if(!strcmp(relation,"<")) yes=p<q;
        else if(!strcmp(relation,"<=")) yes=p<=q;
        else if(!strcmp(relation,"==")) yes=p==q;
        else if(!strcmp(relation,"!=")) yes=p!=q;
        else if(!strcmp(relation,">=")) yes=p>=q;
        else if(!strcmp(relation,">")) yes=p>q;
        else return TG_ERROR;
        return output(yes?"1":"0",1,out,cap);
    }
    if(!strcmp(op,"mark")) return document_mark(d,n,a,out,cap);
    if(!strcmp(op,"search")) return document_search(d,n,a,out,cap);
    if(!strcmp(op,"tag")&&n>=3) return document_tag(d,n,a,out,cap);
    if(!strcmp(op,"index")&&n==3&&index_of_widget(w,a[2],&p)) {
        int r,c; coords(d,p,&r,&c); int z=snprintf(out,cap,"%d.%d",r,c);
        return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR;
    }
    if(!strcmp(op,"get")&&(n==3||n==4)&&index_of_widget(w,a[2],&p)) {
        q=next(d,p); if(n==4&&!index_of_widget(w,a[3],&q)) return TG_ERROR;
        if(q<p) q=p;
        return output(d->bytes+p,q-p,out,cap);
    }
    if(!strcmp(op,"insert")&&n>=4&&index_of_widget(w,a[2],&p)) {
        if(w->state==TG_STATE_DISABLED) return output("",0,out,cap);
        for(size_t k=3;k<n;k+=2) {
            if(p>=d->length) p=d->length-1;
            uint32_t mask=0;
            if(k+1<n) {
                const char *list=a[k+1]; char name[64]; int rc;
                while((rc=tg_list_next(&list,name,sizeof name))>0) {
                    int tag=tag_index(d,name,1); if(tag<0) return TG_ERROR;
                    mask|=UINT32_C(1)<<tag;
                }
                if(rc<0) return TG_ERROR;
            }
            if(splice_mask(w,p,p,a[k],mask)!=TG_OK) return TG_ERROR;
            p+=strlen(a[k]);
        }
    } else if(!strcmp(op,"delete")&&(n==3||n==4)&&index_of(d,a[2],&p)) {
        q=next(d,p); if(n==4&&!index_of(d,a[3],&q)) return TG_ERROR;
        if(q>p&&splice(w,p,q,"")!=TG_OK) return TG_ERROR;
    } else if(!strcmp(op,"replace")&&n>=5&&index_of(d,a[2],&p)&&index_of(d,a[3],&q)) {
        if(q<p||splice(w,p,q,a[4])!=TG_OK) return TG_ERROR;
    } else if(!strcmp(op,"tag")||!strcmp(op,"window")||!strcmp(op,"image")||!strcmp(op,"scan")||!strcmp(op,"dump")) {
        /* Supported text subcommands */
    } else if(!strcmp(op,"see")&&n==3&&index_of_widget(w,a[2],&p)) {
        see(w,p);
    } else if(!strcmp(op,"dlineinfo")&&n==3) {
        if(!strcmp(a[2],"end")||!strcmp(a[2],"end-0c")) { out[0]=0; return TG_OK; }
        const char *spec=a[2]; int req_row=0;
        if(decimal(&spec,&req_row)&&*spec=='.'&&req_row>d->line_count) { out[0]=0; return TG_OK; }
        if(!index_of_widget(w,a[2],&p)) { out[0]=0; return TG_OK; }
        int row,col; coords(d,p,&row,&col);
        if(row>d->line_count) { out[0]=0; return TG_OK; }
        int top=clamp_top(w,d->top),vis=visible(w);
        if(row-1<top||row-1>=top+vis) { out[0]=0; return TG_OK; }
        int h=document_row_height(w);
        int z=snprintf(out,cap,"0 %d %d %d %d",2+(row-1-top)*h,w->rect.w>0?w->rect.w:100,h,(int)(h*0.75));
        return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR;
    } else if(!strcmp(op,"bbox")&&n==3) {
        if(!index_of_widget(w,a[2],&p)) { out[0]=0; return TG_OK; }
        int row,col; coords(d,p,&row,&col);
        int top=clamp_top(w,d->top),vis=visible(w);
        if(row-1<top||row-1>=top+vis) { out[0]=0; return TG_OK; }
        int h=document_row_height(w),x=col*COL;
        if(tg_text_native_enabled()) {
            size_t start=location(d,row,0);
            if(start<d->length) {
                size_t len=strcspn(d->bytes+start,"\n");
                if(len<4096&&p>=start&&p-start<=len) {
                    char line[4096]; memcpy(line,d->bytes+start,len); line[len]=0;
                    const tg_visual_cursor *v=d->visual_line_start==start?&d->visual:NULL;
                    (void)tg_text_font_visual_caret(line,w->font_spec,p-start,&x,v);
                }
            }
        }
        int z=snprintf(out,cap,"%d %d %d %d",3+x-clamp_left(w,d->left),2+(row-1-top)*h,COL,h);
        return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR;
    } else if(!strcmp(op,"yview")) {
        if(n==2) {
            double first,last; tg_document_view_fractions(w,&first,&last);
            int z=snprintf(out,cap,"%.12g %.12g",first,last);
            return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR;
        }
        if(n==3&&index_of(d,a[2],&p)) { int r,c; coords(d,p,&r,&c); d->top=clamp_top(w,r-1); }
        else if(n==4&&!strcmp(a[2],"moveto")) {
            char *end; errno=0; double fraction=strtod(a[3],&end);
            if(errno||end==a[3]||*end||!(fraction>=-1000000&&fraction<=1000000)) return TG_ERROR;
            if(fraction<0) fraction=0; if(fraction>1) fraction=1;
            d->top=clamp_top(w,(int)(fraction*visual_line_count(w)+0.5));
        } else if(n==5&&!strcmp(a[2],"scroll")) {
            char *end; errno=0; long delta=strtol(a[3],&end,10);
            if(errno||end==a[3]||*end||delta<-100000||delta>100000) return TG_ERROR;
            int step=(!strcmp(a[4],"units")||!strcmp(a[4],"unit"))?1:(!strcmp(a[4],"pages")||!strcmp(a[4],"page"))?visible(w):0;
            if(!step) return TG_ERROR;
            d->top=clamp_top(w,clamp_top(w,d->top)+(int)delta*step);
        } else return TG_ERROR;
    } else if(!strcmp(op,"xview")) {
        if(n==2) {
            double first,last; tg_document_xview_fractions(w,&first,&last);
            int z=snprintf(out,cap,"%.12g %.12g",first,last);
            return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR;
        }
        if(n==3&&index_of(d,a[2],&p)) {
            int r,c; coords(d,p,&r,&c);
            d->left=clamp_left(w,c*COL);
        } else if(n==4&&!strcmp(a[2],"moveto")) {
            char *end; errno=0; double fraction=strtod(a[3],&end);
            if(errno||end==a[3]||*end||!(fraction>=-1000000&&fraction<=1000000)) return TG_ERROR;
            if(fraction<0) fraction=0; if(fraction>1) fraction=1;
            d->left=clamp_left(w,(int)(fraction*max_line_width(w)+0.5));
        } else if(n==5&&!strcmp(a[2],"scroll")) {
            char *end; errno=0; long delta=strtol(a[3],&end,10);
            if(errno||end==a[3]||*end||delta<-100000||delta>100000) return TG_ERROR;
            int avail=visible_width(w);
            int step=(!strcmp(a[4],"units")||!strcmp(a[4],"unit"))?16:(!strcmp(a[4],"pages")||!strcmp(a[4],"page"))?avail:0;
            if(!step) return TG_ERROR;
            d->left=clamp_left(w,clamp_left(w,d->left)+(int)delta*step);
        } else return TG_ERROR;
    } else return -1;
    d->top=clamp_top(w,d->top); d->left=clamp_left(w,d->left); out[0]=0; return TG_OK;
}
static int set_selection_range(tg_text_document *d,size_t a,size_t b) {
    if(!d||!d->tags) return 0;
    int tag=tag_index(d,"sel",a<b?1:0);
    if(tag<0) return 0;
    if(!d->style[tag].has_bg) {
        d->style[tag].has_bg=1; d->style[tag].bg=0x2563eb;
        if(!d->style[tag].has_fg) { d->style[tag].has_fg=1; d->style[tag].fg=0xffffff; }
    }
    uint32_t bit=UINT32_C(1)<<tag;
    int changed=0;
    if(a>d->length) a=d->length;
    if(b>d->length) b=d->length;
    for(size_t k=0;k<d->length;k++) {
        uint32_t want=(k>=a&&k<b)?bit:0;
        if((d->tags[k]&bit)!=want) {
            if(want) d->tags[k]|=bit; else d->tags[k]&=~bit;
            changed=1;
        }
    }
    return changed;
}
static size_t hit_offset(tg_widget *w,int x,int y) {
    tg_text_document *d=w->document;
    int left=w->wrap?0:clamp_left(w,d->left);
    int row=(y-w->rect.y-2)/document_row_height(w);
    if(row<0) row=0;
    d->preferred_x_valid=d->visual.valid=0;
    int target_vis=clamp_top(w,d->top)+row;
    int native_mode=text_native_active();
    if(w->wrap>0&&w->rect.w>12) {
        int vis_r=0;
        for(size_t p=0;p<d->length;) {
            size_t n=strcspn(d->bytes+p,"\n");
            size_t off=0;
            do {
                size_t seg=n?next_wrap_segment(w,d->bytes+p,n,off,native_mode):0;
                if(vis_r==target_vis) {
                    if(!seg) return p+off<d->length?p+off:(d->length?d->length-1:0);
                    if(native_mode) {
                        char *slice=malloc(seg+1);
                        if(slice) {
                            memcpy(slice,d->bytes+p+off,seg); slice[seg]=0;
                            size_t byte=0;
                            if(tg_text_font_visual_hit(slice,w->font_spec,x-w->rect.x-3,&byte,&d->visual)==TG_OK&&byte<=seg) {
                                size_t pos=p+off+byte;
                                d->visual_line_start=p+off;
                                free(slice);
                                if(pos>=d->length) pos=d->length-1;
                                return pos;
                            }
                            free(slice);
                        }
                    }
                    int col=(x-w->rect.x-3)/COL;
                    if(col<0) col=0;
                    size_t q=p+off;
                    while(col>0&&q<p+off+seg) { q=next(d,q); col--; }
                    if(q>=d->length) q=d->length-1;
                    return q;
                }
                vis_r++;
                off+=seg;
            } while(off<n);
            p+=n+1;
        }
        return d->length?d->length-1:0;
    }
    int col=(x-w->rect.x-3+left)/COL;
    int logical_row=target_vis+1;
    size_t pos=location(d,logical_row,col>0?col:0);
    if(native_mode) {
        size_t start=location(d,logical_row,0);
        if(start<d->length) {
            size_t len=strcspn(d->bytes+start,"\n"),byte=0;
            char *line=malloc(len+1);
            if(line) {
                memcpy(line,d->bytes+start,len); line[len]=0;
                if(tg_text_font_visual_hit(line,w->font_spec,x-w->rect.x-3+left,&byte,&d->visual)==TG_OK&&byte<=len) {
                    pos=start+byte; d->visual_line_start=start;
                }
                free(line);
            }
        }
    }
    if(pos>=d->length) pos=d->length-1;
    return pos;
}
int tg_document_click(tg_widget *w,int x,int y) {
    tg_text_document *d=w->document; if(!d) return 0;
    d->cursor=hit_offset(w,x,y);
    d->sel_anchor=d->cursor;
    int sel_changed=set_selection_range(d,d->cursor,d->cursor);
    return sel_changed?2:1;
}
int tg_document_drag(tg_widget *w,int x,int y) {
    tg_text_document *d=w->document; if(!d) return 0;
    size_t pos=hit_offset(w,x,y);
    int moved=pos!=d->cursor;
    d->cursor=pos;
    size_t lo=d->sel_anchor<pos?d->sel_anchor:pos;
    size_t hi=d->sel_anchor<pos?pos:d->sel_anchor;
    int sel_changed=set_selection_range(d,lo,hi);
    return sel_changed?2:(moved?1:0);
}
/* The same shaped line is used for hit testing, paint and navigation. */
static int visual_line(const tg_text_document *d,int row,char *line,size_t cap,size_t *start,size_t *len) {
    *start=location(d,row,0);
    if(*start>=d->length) return 0;
    *len=strcspn(d->bytes+*start,"\n");
    if(*len>=cap) return 0;
    memcpy(line,d->bytes+*start,*len); line[*len]=0; return 1;
}
const char *tg_document_input_text(const tg_widget *w,size_t *length,size_t *cursor) {
    const tg_text_document *d=w?w->document:NULL;
    if(!d||!length||!cursor) return NULL;
    *length=d->length?d->length-1:0; *cursor=d->cursor;
    if(*cursor>*length) *cursor=*length;
    return d->bytes;
}
int tg_document_replace_bytes(tg_widget *w,size_t first,size_t last,const char *text) {
    if(!w||!w->document||!text||w->state!=TG_STATE_NORMAL) return TG_ERROR;
    tg_text_document *d=w->document; size_t count;
    if(first>last||last>=d->length||
       tg_utf8_count((const unsigned char *)d->bytes,first,&count)!=TG_OK||
       tg_utf8_count((const unsigned char *)d->bytes,last,&count)!=TG_OK) return TG_ERROR;
    if(splice(w,first,last,text)!=TG_OK) return TG_ERROR;
    d->cursor=first+strlen(text); d->visual.valid=d->preferred_x_valid=0;
    d->top=clamp_top(w,d->top); see(w,d->cursor); return TG_OK;
}
int tg_document_caret_rect(const tg_widget *w,tg_rect *r) {
    if(!w||!r||!w->document||w->rect.w<=0||w->rect.h<=0) return TG_ERROR;
    const tg_text_document *d=w->document;
    int row,col; coords(d,d->cursor,&row,&col);
    int h=document_row_height(w),x=col*COL;
    char line[4096]; size_t start,len;
    if(tg_text_native_enabled()&&visual_line(d,row,line,sizeof line,&start,&len)&&d->cursor-start<=len) {
        const tg_visual_cursor *v=d->visual_line_start==start?&d->visual:NULL;
        (void)tg_text_font_visual_caret(line,w->font_spec,d->cursor-start,&x,v);
    }
    *r=(tg_rect){w->rect.x+3+x-clamp_left(w,d->left),w->rect.y+2+(row-1-clamp_top(w,d->top))*h,1,h};
    return TG_OK;
}
static int visual_document_move(tg_widget *w,uint32_t key) {
    if(!tg_text_native_enabled()) return 0;
    tg_text_document *d=w->document;
    int row,col; coords(d,d->cursor,&row,&col);
    char line[4096]; size_t start,len;
    if(!visual_line(d,row,line,sizeof line,&start,&len)||d->cursor-start>len) return 0;
    size_t byte=d->cursor-start;
    if(d->visual_line_start!=start||d->visual.offset!=byte) d->visual.valid=0;
    if(key==TG_KEY_LEFT||key==TG_KEY_RIGHT) {
        size_t old=byte; tg_visual_cursor before=d->visual;
        if(tg_text_font_visual_move(line,w->font_spec,&byte,key==TG_KEY_LEFT?-1:1,&d->visual)!=TG_OK||byte>len) {
            d->visual.valid=0; return 0;
        }
        d->visual_line_start=start;
        int affinity_changed=d->visual.valid&&(!before.valid||before.index!=d->visual.index||
            before.trailing!=d->visual.trailing||before.weak!=d->visual.weak);
        if(byte==old&&!affinity_changed) {
            /* At a shaped visual endpoint, cross only the corresponding logical
               line boundary. Do not fall back to the opposite RTL direction. */
            int first,last;
            if(tg_text_font_caret(line,w->font_spec,0,&first)!=TG_OK||
               tg_text_font_caret(line,w->font_spec,len,&last)!=TG_OK) return 0;
            int rtl=first>last,forward=(key==TG_KEY_RIGHT)!=rtl;
            if(forward&&byte==len&&d->cursor<d->length-1) { d->cursor=next(d,d->cursor); d->visual.valid=0; }
            else if(!forward&&!byte&&d->cursor) { d->cursor=prev(d,d->cursor); d->visual.valid=0; }
            return 1;
        }
        d->cursor=start+byte; return 1;
    }
    if(key==TG_KEY_ARROW_UP||key==TG_KEY_ARROW_DOWN) {
        int other=row+(key==TG_KEY_ARROW_UP?-1:1),pixel;
        if(other<1||other>lines(d)) return 1;
        if(d->preferred_x_valid&&d->preferred_cursor==d->cursor) pixel=d->preferred_x;
        else if(tg_text_font_visual_caret(line,w->font_spec,byte,&pixel,&d->visual)!=TG_OK) return 0;
        if(!visual_line(d,other,line,sizeof line,&start,&len)||
           tg_text_font_visual_hit(line,w->font_spec,pixel,&byte,&d->visual)!=TG_OK||byte>len) {
            d->visual.valid=0; return 0;
        }
        d->visual_line_start=start;
        d->cursor=start+byte; d->preferred_x=pixel; d->preferred_x_valid=1;
        d->preferred_cursor=d->cursor; return 1;
    }
    return 0;
}
static int doc_is_word_sep(unsigned char c) {
    return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='/'||c=='\\'||c=='.'||c==','||
           c==':'||c==';'||c=='('||c==')'||c=='['||c==']'||c=='{'||c=='}'||
           c=='"'||c=='\''||c=='-'||c=='='||c=='+';
}
static size_t doc_word_prev(const tg_text_document *d,size_t p) {
    if(!p) return 0;
    size_t q=prev(d,p);
    while(q>0&&doc_is_word_sep((unsigned char)d->bytes[q])) q=prev(d,q);
    while(q>0&&!doc_is_word_sep((unsigned char)d->bytes[prev(d,q)])) q=prev(d,q);
    return q;
}
static size_t doc_word_next(const tg_text_document *d,size_t p) {
    size_t end=d->length?d->length-1:0;
    if(p>=end) return end;
    size_t q=p;
    while(q<end&&doc_is_word_sep((unsigned char)d->bytes[q])) q=next(d,q);
    while(q<end&&!doc_is_word_sep((unsigned char)d->bytes[q])) q=next(d,q);
    return q;
}
static int document_sel_bounds(tg_text_document *d,size_t *lo,size_t *hi) {
    if(!d||!d->tags) return 0;
    int tag=tag_index(d,"sel",0);
    if(tag<0) return 0;
    uint32_t bit=UINT32_C(1)<<tag;
    size_t a=d->length,b=0;
    for(size_t k=0;k<d->length;k++) if(d->tags[k]&bit) {
        if(a==d->length) a=k;
        b=k+1;
    }
    if(a>=b) return 0;
    if(lo) *lo=a;
    if(hi) *hi=b;
    return 1;
}
int tg_document_key(tg_widget *w,uint32_t key,int text) {
    tg_text_document *d=w->document; if(!d||w->state==TG_STATE_DISABLED) return 0;
    unsigned mods=text?0:(key&~TG_KEY_VALUE_MASK);
    uint32_t k=text?key:(key&TG_KEY_VALUE_MASK);
    if(text||(k!=TG_KEY_ARROW_UP&&k!=TG_KEY_ARROW_DOWN)) d->preferred_x_valid=0;
    if(text||(k!=TG_KEY_LEFT&&k!=TG_KEY_RIGHT&&k!=TG_KEY_ARROW_UP&&k!=TG_KEY_ARROW_DOWN)) d->visual.valid=0;
    if(text) {
        unsigned char b[5]; size_t used;
        if(k<32||tg_utf8_encode(k,b,sizeof b-1,&used)!=TG_OK) return 0;
        b[used]=0;
        size_t lo=d->cursor,hi=d->cursor;
        if(document_sel_bounds(d,&lo,&hi)) {
            if(splice(w,lo,hi,(const char *)b)!=TG_OK) return 0;
            set_selection_range(d,d->cursor,d->cursor);
        } else {
            if(splice(w,d->cursor,d->cursor,(const char *)b)!=TG_OK) return 0;
        }
    } else {
        if(mods&TG_MOD_ALT) return 0;
        int ctrl=(mods&TG_MOD_CONTROL)!=0,shift=(mods&TG_MOD_SHIFT)!=0;
        size_t old_cursor=d->cursor,lo=0,hi=0;
        int has_sel=document_sel_bounds(d,&lo,&hi);
        switch(k) {
        case TG_KEY_LEFT:
            if(ctrl) return 0;
            if(!shift&&has_sel) d->cursor=lo;
            else if(!visual_document_move(w,k)) d->cursor=prev(d,d->cursor);
            break;
        case TG_KEY_RIGHT:
            if(ctrl) return 0;
            if(!shift&&has_sel) d->cursor=hi<d->length?hi:(d->length?d->length-1:0);
            else if(!visual_document_move(w,k)&&d->cursor<d->length-1) d->cursor=next(d,d->cursor);
            break;
        case TG_KEY_BACKSPACE:
            if(has_sel) {
                if(splice(w,lo,hi,"")!=TG_OK) return 0;
                d->cursor=lo;
                set_selection_range(d,d->cursor,d->cursor);
            } else if(d->cursor) {
                size_t start=ctrl?doc_word_prev(d,d->cursor):prev(d,d->cursor);
                if(splice(w,start,d->cursor,"")!=TG_OK) return 0;
            }
            see(w,d->cursor); return 1;
        case TG_KEY_DELETE:
            if(shift) return 0;
            if(has_sel) {
                if(splice(w,lo,hi,"")!=TG_OK) return 0;
                d->cursor=lo;
                set_selection_range(d,d->cursor,d->cursor);
            } else {
                size_t stop=ctrl?doc_word_next(d,d->cursor):next(d,d->cursor);
                if(stop>d->cursor&&splice(w,d->cursor,stop,"")!=TG_OK) return 0;
            }
            see(w,d->cursor); return 1;
        case TG_KEY_RETURN:
            if(ctrl) return 0;
            if(has_sel) {
                if(splice(w,lo,hi,"\n")!=TG_OK) return 0;
                set_selection_range(d,d->cursor,d->cursor);
            } else if(splice(w,d->cursor,d->cursor,"\n")!=TG_OK) return 0;
            see(w,d->cursor); return 1;
        case TG_KEY_HOME: case TG_KEY_END: case TG_KEY_ARROW_UP: case TG_KEY_ARROW_DOWN: {
            if(ctrl) {
                if(k==TG_KEY_HOME) d->cursor=0;
                else if(k==TG_KEY_END) d->cursor=d->length?d->length-1:0;
                else return 0;
            } else if(!visual_document_move(w,k)) {
                int r,c; coords(d,d->cursor,&r,&c);
                if(k==TG_KEY_HOME) c=0;
                if(k==TG_KEY_END) c=LIMIT;
                if(k==TG_KEY_ARROW_UP&&r>1) r--;
                if(k==TG_KEY_ARROW_DOWN&&r<lines(d)) r++;
                d->cursor=location(d,r,c); if(d->cursor>=d->length) d->cursor=d->length-1;
            }
            break;
        }
        default: return 0;
        }
        if(shift) {
            if(!has_sel) d->sel_anchor=old_cursor;
            size_t a=d->sel_anchor<d->cursor?d->sel_anchor:d->cursor;
            size_t b=d->sel_anchor<d->cursor?d->cursor:d->sel_anchor;
            set_selection_range(d,a,b);
        } else {
            d->sel_anchor=d->cursor;
            if(has_sel) set_selection_range(d,d->cursor,d->cursor);
        }
    }
    see(w,d->cursor); return 1;
}
/* Shape entire runs instead of drawing isolated UTF-8 scalars: Arabic
   joining, combining marks and bidi context require the surrounding text. */
static void native_document_paint(const tg_widget *w,tg_canvas *canvas,uint32_t fg,int focused) {
    tg_rect clip=w->rect;
    int height=document_row_height(w);
    if(w->kind==TG_MESSAGE) {
        const char *p=w->text;
        int y=clip.y+2;
        while(*p&&y<clip.y+clip.h) {
            size_t len=strcspn(p,"\n"),n=0,last_space=0;
            char line[256];
            while(n<len&&n<sizeof line-1) {
                uint32_t scalar=0; size_t used;
                if(tg_utf8_decode((const unsigned char *)p+n,len-n,&scalar,&used)!=TG_OK) used=1;
                if(n+used>=sizeof line) break;
                size_t end=n+used; memcpy(line,p,end); line[end]=0;
                int tw,th;
                if(n&&tg_text_measure(line,1,&tw,&th)==TG_OK&&tw>clip.w-6) {
                    if(last_space) n=last_space;
                    break;
                }
                n=end;
                if(scalar==' '||scalar=='\t') last_space=n;
            }
            if(n) {
                memcpy(line,p,n); line[n]=0;
                tg_rect row;
                if(tg_rect_intersect(clip,(tg_rect){clip.x,y,clip.w,height},&row))
                    tg_text_draw_row(canvas,row,clip.x+3,line,fg);
                p+=n;
                while(*p==' '||*p=='\t') p++;
            } else if(len) break;
            if(*p=='\n') p++;
            y+=height;
        }
        return;
    }
    const tg_text_document *d=w->document;
    if(!d) return;
    int top=clamp_top(w,d->top),left=w->wrap?0:clamp_left(w,d->left),vis_r=0;
    int max_vis=top+visible(w);
    int text_x=clip.x+3-left;
    for(size_t p=0;p<d->length&&vis_r<max_vis;) {
        size_t n=strcspn(d->bytes+p,"\n");
        size_t off=0;
        do {
            size_t seg=n?next_wrap_segment(w,d->bytes+p,n,off,1):0;
            if(vis_r>=top&&vis_r<max_vis) {
                char *line=malloc(seg+1); if(!line) return;
                if(seg) memcpy(line,d->bytes+p+off,seg);
                line[seg]=0;
                int y=clip.y+2+(vis_r-top)*height;
                tg_rect row;
                if(tg_rect_intersect(clip,(tg_rect){clip.x,y,clip.w,height},&row)) {
                    size_t base=p+off;
                    for(size_t a=0;a<seg;) {
                        size_t b=a+1; uint32_t mask=d->tags[base+a];
                        while(b<seg&&d->tags[base+b]==mask) b++;
                        if(mask) {
                            uint32_t ink=fg,bg=0; int has_bg;
                            tag_colors(d,mask,&ink,&bg,&has_bg);
                            if(has_bg) (void)tg_text_font_selection(canvas,row,text_x,y,line,w->font_spec,a,b,bg);
                        }
                        a=b;
                    }
                    if(seg) (void)tg_text_font_draw(canvas,row,text_x,y,line,w->font_spec,fg);
                    for(size_t a=0;a<seg;) {
                        size_t b=a+1; uint32_t mask=d->tags[base+a];
                        while(b<seg&&d->tags[base+b]==mask) b++;
                        if(mask) {
                            uint32_t ink=fg,bg=0; int has_bg,x1,x2;
                            tag_colors(d,mask,&ink,&bg,&has_bg);
                            if(ink!=fg&&tg_text_font_caret(line,w->font_spec,a,&x1)==TG_OK&&
                               tg_text_font_caret(line,w->font_spec,b,&x2)==TG_OK) {
                                if(x2<x1) { int tmp=x1; x1=x2; x2=tmp; }
                                tg_rect run;
                                if(tg_rect_intersect(row,(tg_rect){text_x+x1,y,x2-x1,height},&run))
                                    (void)tg_text_font_draw(canvas,run,text_x,y,line,w->font_spec,ink);
                            }
                        }
                        a=b;
                    }
                    int is_last_seg=(off+seg>=n);
                    if(focused&&d->cursor>=base&&(is_last_seg?d->cursor<=base+seg:d->cursor<base+seg)) {
                        int tw=0;
                        const tg_visual_cursor *state=d->visual_line_start==base?&d->visual:NULL;
                        if(tg_text_font_visual_caret(line,w->font_spec,d->cursor-base,&tw,state)!=TG_OK) {
                            line[d->cursor-base]=0;
                            int th=0; (void)tg_text_measure(line,1,&tw,&th);
                        }
                        tg_raster_fill(canvas,row,(tg_rect){text_x+tw,y+2,1,height-4},fg);
                    }
                }
                free(line);
            }
            vis_r++;
            off+=seg;
        } while(off<n&&vis_r<max_vis);
        p+=n+1;
    }
}
void tg_document_paint(const tg_widget *w,tg_canvas *canvas,uint32_t fg,int focused) {
    if(tg_text_native_enabled()&&strcmp(tg_text_native_backend(),"bitmap")) {
        native_document_paint(w,canvas,fg,focused); return;
    }
    tg_rect clip=w->rect;
    if(w->kind==TG_MESSAGE) {
        size_t len=strlen(w->text),p=0; int x=0,y=0,cols=clip.w>6?(clip.w-6)/COL:1;
        if(cols<1) cols=1;
        while(p<len&&y*ROW<clip.h) {
            if(w->text[p]=='\n') { p++; x=0; y++; continue; }
            if(x>=cols) { x=0; y++; }
            uint32_t scalar; size_t used;
            if(tg_utf8_decode((const unsigned char *)w->text+p,len-p,&scalar,&used)!=TG_OK) used=1;
            char s[5]; memcpy(s,w->text+p,used); s[used]=0;
            tg_text_draw(canvas,clip,clip.x+3+x*COL,clip.y+2+y*ROW,s,1,fg);
            x++; p+=used;
        }
        return;
    }
    const tg_text_document *d=w->document; if(!d) return;
    int top=clamp_top(w,d->top),left=w->wrap?0:clamp_left(w,d->left),vis_r=0;
    int max_vis=top+visible(w);
    for(size_t p=0;p<d->length&&vis_r<max_vis;) {
        size_t n=strcspn(d->bytes+p,"\n");
        size_t off=0;
        do {
            size_t seg=n?next_wrap_segment(w,d->bytes+p,n,off,0):0;
            if(vis_r>=top&&vis_r<max_vis) {
                int y=clip.y+2+(vis_r-top)*ROW;
                int c=0;
                size_t base=p+off,end=base+seg;
                for(size_t q=base;q<end;) {
                    size_t nxt=next(d,q);
                    int x=clip.x+3+c*COL-left;
                    if(focused&&q==d->cursor) tg_raster_fill(canvas,clip,(tg_rect){x,y,1,9},fg);
                    if(x+COL>clip.x&&x<clip.x+clip.w) {
                        char s[5]; memcpy(s,d->bytes+q,nxt-q); s[nxt-q]=0;
                        uint32_t ink=fg,bg=0; int has_bg;
                        tag_colors(d,d->tags[q],&ink,&bg,&has_bg);
                        if(has_bg) tg_raster_fill(canvas,clip,(tg_rect){x,y,COL,ROW},bg);
                        tg_text_draw(canvas,clip,x,y,s,1,ink);
                    }
                    c++; q=nxt;
                }
                if(focused&&off+seg>=n&&d->cursor==end) {
                    int x=clip.x+3+c*COL-left;
                    tg_raster_fill(canvas,clip,(tg_rect){x,y,1,9},fg);
                }
            }
            vis_r++;
            off+=seg;
        } while(off<n&&vis_r<max_vis);
        p+=n+1;
    }
}
