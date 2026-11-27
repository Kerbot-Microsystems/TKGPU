#include "TKGPUControls.H"
#include "../ttk/TKGPUChoice.H"
#include "TKGPUUtf8.H"
#include "TKGPUText.H"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
static size_t text_offset(const char *text,size_t index) {
    size_t n=strlen(text),p=0;
    while(p<n&&index--) {
        uint32_t v; size_t k;
        if(tg_utf8_decode((const unsigned char *)text+p,n-p,&v,&k)!=TG_OK) k=1;
        p+=k;
    }
    return p;
}
static size_t offset(const tg_widget *w,size_t index) { return text_offset(w->text,index); }
static size_t count(const tg_widget *w) {
    size_t n=0;
    if(tg_utf8_count((const unsigned char *)w->text,strlen(w->text),&n)!=TG_OK) return 0;
    return n;
}
/* A 1023-byte source contains at most 1023 scalars; a four-byte mask
   therefore fits in 4096 bytes. All display operations use this same run. */
const char *tg_entry_display_text(const tg_widget *w,char out[TG_ENTRY_DISPLAY_CAP]) {
    if(!w->show[0]) return w->text;
    size_t total=count(w),length=strlen(w->show),at=0;
    if(length>4) { out[0]=0; return out; }
    for(size_t k=0;k<total&&at+length<TG_ENTRY_DISPLAY_CAP;k++) {
        memcpy(out+at,w->show,length); at+=length;
    }
    out[at]=0; return out;
}
int tg_entry_caret_rect(const tg_widget *w,tg_rect *r) {
    if(!w||!r||w->rect.w<=0||w->rect.h<=0) return TG_ERROR;
    char masked[TG_ENTRY_DISPLAY_CAP]; const char *shown=tg_entry_display_text(w,masked);
    int x=0,h=7,width=0;
    if(tg_text_native_enabled()&&tg_text_font_caret(shown,w->font_spec,text_offset(shown,w->cursor),&x)==TG_OK)
        (void)tg_text_font_measure(*shown?shown:"Mg",w->font_spec,&width,&h);
    else x=(int)w->cursor*6;
    *r=(tg_rect){w->rect.x+w->border_width+w->highlight_width+w->text_padx+3+x-w->entry_scroll,
                 w->rect.y+(w->rect.h-h)/2,1,h};
    return TG_OK;
}
static int entry_available(const tg_widget *w) {
    int n=w->rect.w-2*(w->border_width+w->highlight_width+w->text_padx+3);
    if(tg_choice_kind(w->kind)) n-=13;
    return n>0?n:0;
}
static int entry_width(const tg_widget *w,const char *shown) {
    int width=0,height=0;
    if(tg_text_native_enabled()&&tg_text_font_measure(shown,w->font_spec,&width,&height)==TG_OK)
        return width>0?width:0;
    return (int)count(w)*6;
}
static int entry_clamp_scroll(int pos,int width,int available) {
    int max=width>available?width-available:0;
    return pos<0?0:pos>max?max:pos;
}
static void ensure_visible(tg_widget *w) {
    char masked[TG_ENTRY_DISPLAY_CAP]; const char *shown=tg_entry_display_text(w,masked);
    int caret=0,available=entry_available(w);
    if(available<1) return;
    if(!tg_text_native_enabled()||tg_text_font_caret(shown,w->font_spec,text_offset(shown,w->cursor),&caret)!=TG_OK) caret=(int)w->cursor*6;
    if(caret<w->entry_scroll) w->entry_scroll=caret;
    else if(caret>=w->entry_scroll+available) w->entry_scroll=caret-available+1;
    w->entry_scroll=entry_clamp_scroll(w->entry_scroll,entry_width(w,shown)+1,available);
}
static size_t pixel_index(const tg_widget *w,int x) {
    char masked[TG_ENTRY_DISPLAY_CAP]; const char *shown=tg_entry_display_text(w,masked);
    size_t n=count(w);
    if(tg_text_native_enabled()&&strcmp(tg_text_native_backend(),"bitmap")) {
        size_t byte=0,chars=0;
        if(tg_text_font_hit(shown,w->font_spec,x-w->border_width-w->highlight_width-w->text_padx-3+w->entry_scroll,&byte)==TG_OK&&byte<=strlen(shown)&&
           tg_utf8_count((const unsigned char *)shown,byte,&chars)==TG_OK)
            return chars<n?chars:n;
    }
    /* Retain the existing bitmap hit-test rounding, adding the scroll offset. */
    x+=w->entry_scroll;
    if(x<=3) return 0;
    size_t pos=(size_t)x/6;
    return pos<n?pos:n;
}
static int index_of(const tg_widget *w,const char *s,size_t *v) {
    if(!strcmp(s,"end")) { *v=count(w); return 1; }
    if(!strcmp(s,"insert")) { *v=w->cursor; return 1; }
    if(!strcmp(s,"anchor")) { *v=w->sel_anchor; return 1; }
    if(!strcmp(s,"sel.first")||!strcmp(s,"sel.last")) {
        if(w->sel_first>=w->sel_last) return 0;
        *v=!strcmp(s,"sel.first")?w->sel_first:w->sel_last; return 1;
    }
    if(*s=='@') {
        char *end; errno=0; long x=strtol(s+1,&end,10);
        if(errno||end==s+1||*end||x< -16384||x>16384) return 0;
        *v=pixel_index(w,(int)x); return 1;
    }
    char *end; errno=0; long n=strtol(s,&end,10);
    if(errno||end==s||*end) return 0;
    *v=n<0?0:(unsigned long)n>count(w)?count(w):(size_t)n; return 1;
}
int tg_entry_index(const tg_widget *w,const char *s,size_t *v) { return index_of(w,s,v); }
size_t tg_entry_offset(const tg_widget *w,size_t n) { return offset(w,n); }
static int insert(tg_widget *w,size_t at,const char *s) {
    if(w->state!=TG_STATE_NORMAL) return TG_OK;
    size_t n=strlen(s),len=strlen(w->text),chars;
    if(n>=sizeof w->text-len||tg_utf8_count((const unsigned char *)s,n,&chars)!=TG_OK) return TG_ERROR;
    size_t p=offset(w,at);
    memmove(w->text+p+n,w->text+p,len-p+1); memcpy(w->text+p,s,n);
    if(w->cursor>=at) w->cursor+=chars;
    if(w->sel_first<w->sel_last) {
        if(at<=w->sel_first) w->sel_first+=chars;
        if(at<w->sel_last) w->sel_last+=chars;
    }
    if(w->sel_anchor>=at) w->sel_anchor+=chars;
    return TG_OK;
}
static size_t after_erase(size_t pos,size_t first,size_t last) {
    return pos<=first?pos:pos<last?first:pos-(last-first);
}
static void erase(tg_widget *w,size_t first,size_t last) {
    if(w->state!=TG_STATE_NORMAL||last<=first) return;
    size_t a=offset(w,first),b=offset(w,last);
    memmove(w->text+a,w->text+b,strlen(w->text+b)+1);
    w->cursor=after_erase(w->cursor,first,last);
    w->sel_first=after_erase(w->sel_first,first,last);
    w->sel_last=after_erase(w->sel_last,first,last);
    w->sel_anchor=after_erase(w->sel_anchor,first,last);
}
/* Native IME replacement uses UTF-8 byte boundaries, never half a scalar. */
int tg_entry_replace_bytes(tg_widget *w,size_t first,size_t last,const char *text) {
    if(!w||!text||w->state!=TG_STATE_NORMAL) return TG_ERROR;
    size_t length=strlen(w->text),n=strlen(text),a,b,added;
    if(first>last||last>length||n>=sizeof w->text-(length-(last-first))||
       tg_utf8_count((const unsigned char *)w->text,first,&a)!=TG_OK||
       tg_utf8_count((const unsigned char *)w->text,last,&b)!=TG_OK||
       tg_utf8_count((const unsigned char *)text,n,&added)!=TG_OK) return TG_ERROR;
    for(size_t k=0;k<n;k++) if((unsigned char)text[k]<32||(unsigned char)text[k]==127) return TG_ERROR;
    memmove(w->text+first+n,w->text+last,length-last+1);
    memcpy(w->text+first,text,n);
    w->cursor=a+added; w->sel_anchor=w->cursor; w->sel_first=w->sel_last=0;
    ensure_visible(w); return TG_OK;
}
static int entry_xview(tg_widget *w,size_t n,char **a,char *out,size_t cap) {
    if(!cap) return TG_ERROR;
    char masked[TG_ENTRY_DISPLAY_CAP]; const char *shown=tg_entry_display_text(w,masked);
    int width=entry_width(w,shown),available=entry_available(w);
    int pos=entry_clamp_scroll(w->entry_scroll,width,available);
    if(n==2) {
        double first=width>available&&width>0?(double)pos/width:0;
        double last=width>available&&width>0?(double)(pos+available)/width:1;
        int z=snprintf(out,cap,"%.12g %.12g",first,last);
        return z>=0&&(size_t)z<cap?TG_OK:TG_ERROR;
    }
    if(n==3) {
        size_t at;
        if(!index_of(w,a[2],&at)) return TG_ERROR;
        if(!tg_text_native_enabled()||tg_text_font_caret(shown,w->font_spec,text_offset(shown,at),&pos)!=TG_OK)
            pos=(int)at*6;
    } else if(n==4&&!strcmp(a[2],"moveto")) {
        char *end; errno=0; double v=strtod(a[3],&end);
        if(errno||end==a[3]||*end||!isfinite(v)) return TG_ERROR;
        if(v<0) v=0; if(v>1) v=1;
        pos=(int)(width*v+0.5);
    } else if(n==5&&!strcmp(a[2],"scroll")) {
        char *end; errno=0; long delta=strtol(a[3],&end,10);
        if(errno||end==a[3]||*end||delta< -100000||delta>100000) return TG_ERROR;
        int step=(!strcmp(a[4],"units")||!strcmp(a[4],"unit"))?6:(!strcmp(a[4],"pages")||!strcmp(a[4],"page"))?(available>6?available-6:6):0;
        if(!step) return TG_ERROR;
        int64_t target=(int64_t)pos+(int64_t)step*delta;
        pos=target<0?0:target>width?width:(int)target;
    } else return TG_ERROR;
    w->entry_scroll=entry_clamp_scroll(pos,width,available);
    out[0]=0; return TG_OK;
}
int tg_entry_command(tg_widget *w,size_t n,char **a,char *out,size_t cap) {
    if((w->kind!=TG_ENTRY&&!tg_choice_kind(w->kind))||n<2) return -1;
    size_t first,last;
    if(!strcmp(a[1],"xview")) return entry_xview(w,n,a,out,cap);
    if(!strcmp(a[1],"selection")) {
        if(n==3&&!strcmp(a[2],"present")) {
            if(cap<2) return TG_ERROR;
            strcpy(out,w->sel_first<w->sel_last?"1":"0"); return TG_OK;
        }
        if(!cap) return TG_ERROR;
        if(n==3&&!strcmp(a[2],"clear")) w->sel_first=w->sel_last=0;
        else if(n==5&&!strcmp(a[2],"range")&&index_of(w,a[3],&first)&&index_of(w,a[4],&last)) {
            w->sel_first=first; w->sel_last=last>first?last:first;
        } else if(n==4&&!strcmp(a[2],"from")&&index_of(w,a[3],&first)) w->sel_anchor=first;
        else if(n==4&&(!strcmp(a[2],"to")||!strcmp(a[2],"adjust"))&&index_of(w,a[3],&first)) {
            if(!strcmp(a[2],"adjust")&&w->sel_first<w->sel_last) {
                size_t middle=w->sel_first+(w->sel_last-w->sel_first)/2;
                w->sel_anchor=first<middle?w->sel_last:w->sel_first;
            }
            w->sel_first=first<w->sel_anchor?first:w->sel_anchor;
            w->sel_last=first>w->sel_anchor?first:w->sel_anchor;
        } else return TG_ERROR;
        out[0]=0; return TG_OK;
    }
    if(!strcmp(a[1],"get")&&n==2) {
        if(strlen(w->text)>=cap) return TG_ERROR;
        strcpy(out,w->text); return TG_OK;
    }
    if(!strcmp(a[1],"index")&&n==3&&index_of(w,a[2],&first)) {
        int k=snprintf(out,cap,"%lu",(unsigned long)first);
        return k>=0&&(size_t)k<cap?TG_OK:TG_ERROR;
    }
    if(!strcmp(a[1],"icursor")&&n==3&&index_of(w,a[2],&first)) w->cursor=first;
    else if(!strcmp(a[1],"insert")&&n==4&&index_of(w,a[2],&first)) {
        if(insert(w,first,a[3])!=TG_OK) return TG_ERROR;
    } else if(!strcmp(a[1],"delete")&&(n==3||n==4)&&index_of(w,a[2],&first)) {
        last=first<count(w)?first+1:first;
        if(n==4&&!index_of(w,a[3],&last)) return TG_ERROR;
        erase(w,first,last);
    } else return -1;
    ensure_visible(w);
    if(cap) out[0]=0;
    return TG_OK;
}
int tg_entry_click(tg_widget *w,int x) {
    if(!w||(w->kind!=TG_ENTRY&&!tg_choice_kind(w->kind))||w->state==TG_STATE_DISABLED) return 0;
    w->cursor=pixel_index(w,x-w->rect.x); w->sel_anchor=w->cursor;
    w->sel_first=w->sel_last=0; ensure_visible(w); return 1;
}
static size_t visual_step(const tg_widget *w,int direction,int selected,size_t total) {
    if(tg_text_native_enabled()) {
        char masked[TG_ENTRY_DISPLAY_CAP];
        const char *shown=tg_entry_display_text(w,masked);
        if(selected) {
            int left,right;
            if(tg_text_font_caret(shown,w->font_spec,text_offset(shown,w->sel_first),&left)==TG_OK&&
               tg_text_font_caret(shown,w->font_spec,text_offset(shown,w->sel_last),&right)==TG_OK)
                return ((direction<0)==(left<=right))?w->sel_first:w->sel_last;
        } else {
            size_t byte=text_offset(shown,w->cursor),chars=0;
            if(tg_text_font_move(shown,w->font_spec,&byte,direction)==TG_OK&&
               byte<=strlen(shown)&&tg_utf8_count((const unsigned char *)shown,byte,&chars)==TG_OK)
                return chars<total?chars:total;
        }
    }
    if(selected) return direction<0?w->sel_first:w->sel_last;
    return direction<0?(w->cursor?w->cursor-1:0):(w->cursor<total?w->cursor+1:total);
}
static int entry_is_word_sep(unsigned char c) {
    return c==' '||c=='\t'||c=='/'||c=='\\'||c=='.'||c==','||c==':'||c==';'||
           c=='('||c==')'||c=='['||c==']'||c=='{'||c=='}'||c=='"'||c=='\''||
           c=='-'||c=='='||c=='+';
}
int tg_entry_edit(tg_widget *w,uint32_t key) {
    if(!w||(w->kind!=TG_ENTRY&&!tg_choice_kind(w->kind))||w->state==TG_STATE_DISABLED) return 0;
    int ctrl=(key&TG_MOD_CONTROL)!=0;
    uint32_t k=key&TG_KEY_VALUE_MASK;
    size_t n=count(w);
    if(w->cursor>n) w->cursor=n;
    int selected=w->sel_first<w->sel_last;
    switch(k) {
    case TG_KEY_LEFT: w->cursor=visual_step(w,-1,selected,n); break;
    case TG_KEY_RIGHT: w->cursor=visual_step(w,1,selected,n); break;
    case TG_KEY_HOME: w->cursor=0; break;
    case TG_KEY_END: w->cursor=n; break;
    case TG_KEY_BACKSPACE:
    case TG_KEY_DELETE:
        if(w->state!=TG_STATE_NORMAL) return 0;
        if(selected) { w->cursor=w->sel_first; erase(w,w->sel_first,w->sel_last); }
        else if(k==TG_KEY_BACKSPACE&&w->cursor) {
            size_t start=w->cursor-1;
            if(ctrl) {
                while(start>0&&entry_is_word_sep((unsigned char)w->text[offset(w,start)])) start--;
                while(start>0&&!entry_is_word_sep((unsigned char)w->text[offset(w,start-1)])) start--;
            }
            erase(w,start,w->cursor);
        } else if(k==TG_KEY_DELETE&&w->cursor<n) {
            size_t stop=w->cursor+1;
            if(ctrl) {
                size_t p=w->cursor;
                while(p<n&&entry_is_word_sep((unsigned char)w->text[offset(w,p)])) p++;
                while(p<n&&!entry_is_word_sep((unsigned char)w->text[offset(w,p)])) p++;
                stop=p>w->cursor?p:w->cursor+1;
            }
            erase(w,w->cursor,stop);
        }
        break;
    default: {
        if(w->state!=TG_STATE_NORMAL) return 1;
        if(k<32||k==127||k>0x10ffff) return 0;
        unsigned char bytes[5]; size_t used;
        if(tg_utf8_encode(k,bytes,4,&used)!=TG_OK) return 0;
        bytes[used]=0;
        /* Verify capacity before removing selected text. */
        size_t removed=selected?offset(w,w->sel_last)-offset(w,w->sel_first):0;
        if(strlen(w->text)-removed+used>=sizeof w->text) return 0;
        if(selected) { w->cursor=w->sel_first; erase(w,w->sel_first,w->sel_last); }
        if(insert(w,w->cursor,(const char *)bytes)!=TG_OK) return 0;
        break;
    }
    }
    w->sel_first=w->sel_last=0; w->sel_anchor=w->cursor; ensure_visible(w); return 1;
}

int tg_entry_drag(tg_widget *w,int x) {
    if(!w||w->state==TG_STATE_DISABLED) return 0;
    w->cursor=pixel_index(w,x-w->rect.x);
    w->sel_first=w->cursor<w->sel_anchor?w->cursor:w->sel_anchor;
    w->sel_last=w->cursor>w->sel_anchor?w->cursor:w->sel_anchor;
    ensure_visible(w); return 1;
}
int tg_entry_shift(tg_widget *w,uint32_t key) {
    if(!w) return 0;
    size_t anchor=w->sel_first<w->sel_last?w->sel_anchor:w->cursor;
    w->sel_first=w->sel_last=0;
    int result=tg_entry_edit(w,key);
    w->sel_anchor=anchor;
    w->sel_first=w->cursor<anchor?w->cursor:anchor;
    w->sel_last=w->cursor>anchor?w->cursor:anchor;
    return result;
}
