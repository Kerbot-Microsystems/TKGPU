#include "TKGPUCompat.H"
#include "../object/TKGPUObjectInternal.H"
#include <stdlib.h>
#include <limits.h>
#include <string.h>

static size_t decode_units(const unsigned char *p,size_t n,Tcl_UniChar out[2],size_t *count);

/* The ABI exposes UTF-16 units. The renderer stores paired supplementary
   characters as scalar UTF-8; preserve lone units and embedded NUL for
   object round trips using the Tcl 8.6 modified representation. */
Tcl_Obj *Tcl_NewUnicodeObj(const Tcl_UniChar *text,int length) {
    size_t n=0,k,used=0;
    char *bytes;
    Tcl_Obj *o;
    if(length < -1 || (!text && length!=0)) return NULL;
    if(length<0) { while(text[n]) { if(n>=(size_t)INT_MAX) return NULL; n++; } }
    else n=(size_t)length;
    if(n>((size_t)INT_MAX-1)/3) return NULL;
    bytes=malloc(n*3+1);
    if(!bytes) return NULL;
    for(k=0;k<n;k++) {
        unsigned c=text[k];
        if(c>=0xd800&&c<=0xdbff&&k+1<n&&text[k+1]>=0xdc00&&text[k+1]<=0xdfff) {
            c=0x10000+((c-0xd800)<<10)+(text[++k]-0xdc00);
            bytes[used++]=(char)(0xf0|(c>>18)); bytes[used++]=(char)(0x80|((c>>12)&63));
            bytes[used++]=(char)(0x80|((c>>6)&63)); bytes[used++]=(char)(0x80|(c&63));
        }
        else if(c && c<0x80) bytes[used++]=(char)c;
        else if(c<0x800) {
            bytes[used++]=(char)(0xc0|(c>>6)); bytes[used++]=(char)(0x80|(c&63));
        } else {
            bytes[used++]=(char)(0xe0|(c>>12));
            bytes[used++]=(char)(0x80|((c>>6)&63)); bytes[used++]=(char)(0x80|(c&63));
        }
    }
    o=Tcl_NewStringObj(bytes,(int)used); free(bytes);
    return o;
}

Tcl_UniChar *Tcl_GetUnicodeFromObj(Tcl_Obj *o,int *length) {
    size_t pos=0,n,used=0;
    const unsigned char *s;
    if(!o) return NULL;
    if(!o->unicode) {
        s=(const unsigned char *)tg_object_bytes(o,&n);
        if(!s) return NULL;
        if(n>(size_t)INT_MAX || n>(SIZE_MAX/sizeof(uint16_t))-1) return NULL;
        o->unicode=malloc((n+1)*sizeof(uint16_t));
        if(!o->unicode) return NULL;
        while(pos<n) {
            Tcl_UniChar pair[2]; size_t units;
            pos+=decode_units(s+pos,n-pos,pair,&units);
            o->unicode[used++]=pair[0];
            if(units==2) o->unicode[used++]=pair[1];
        }
        o->unicode[used]=0; o->unicode_length=(int)used;
    }
    if(length) *length=o->unicode_length;
    return o->unicode;
}

/* Tcl 8.6 C-string APIs use individual UTF-16 units, not renderer scalars. */
int Tcl_UniCharToUtf(int ch,char *buf) {
    unsigned v=ch<0||ch>0xffff?0xfffdu:(unsigned)ch;
    if(v&&v<0x80) { buf[0]=(char)v; return 1; }
    if(v<0x800) {
        buf[0]=(char)(0xc0u|(v>>6)); buf[1]=(char)(0x80u|(v&63u)); return 2;
    }
    buf[0]=(char)(0xe0u|(v>>12));
    buf[1]=(char)(0x80u|((v>>6)&63u)); buf[2]=(char)(0x80u|(v&63u)); return 3;
}
int Tcl_UniCharLen(const Tcl_UniChar *s) {
    int n=0;
    while(s[n]) {
        if(n==INT_MAX) Tcl_Panic("Unicode length overflow");
        ++n;
    }
    return n;
}
int Tcl_UniCharNcmp(const Tcl_UniChar *a,const Tcl_UniChar *b,unsigned long n) {
    while(n--) { int diff=(int)*a++-(int)*b++; if(diff) return diff; }
    return 0;
}
char *Tcl_UniCharToUtfDString(const Tcl_UniChar *s,int length,Tcl_DString *d) {
    if(length<0) length=Tcl_UniCharLen(s);
    if(length>(INT_MAX-1)/3||(!s&&length)) Tcl_Panic("invalid Unicode string length");
    /* Encode before appending because the source may be stored in d itself. */
    char *tmp=Tcl_Alloc((unsigned)length*3u+1u); int used=0;
    for(int k=0;k<length;k++) used+=Tcl_UniCharToUtf(s[k],tmp+used);
    int start=d->length;
    Tcl_DStringAppend(d,tmp,used); Tcl_Free(tmp);
    return d->string+start;
}

/* Tcl 8.6's completeness test is a byte-count predicate, not validation.
   A continuation byte can begin the low-surrogate part of a 4-byte scalar. */
int Tcl_UtfCharComplete(const char *s,int length) {
    if(!s||length<=0) return 0;
    unsigned c=(unsigned char)*s;
    int needed=c<0x80||c==0xc1||c>=0xf5?1:
               c==0xc0||(c>=0xc2&&c<=0xdf)?2:3;
    return length>=needed;
}
Tcl_UniChar *Tcl_GetUnicode(Tcl_Obj *o) { return Tcl_GetUnicodeFromObj(o,NULL); }

/* Length-aware Tcl 8.6 decoding: preserve invalid bytes as individual
   code units, decode modified NUL and surrogate encodings, and split a
   valid supplementary scalar into two UTF-16 units. */
static size_t decode_units(const unsigned char *p,size_t n,Tcl_UniChar out[2],size_t *count) {
    unsigned a=p[0],v=a; size_t w=1;
    if(n>=2&&((a==0xc0&&p[1]==0x80)||(a>=0xc2&&a<=0xdf&&(p[1]&0xc0)==0x80))) {
        v=((a&31u)<<6)|(p[1]&63u); w=2;
    } else if(n>=3&&a>=0xe0&&a<=0xef&&(p[1]&0xc0)==0x80&&(p[2]&0xc0)==0x80&&
              (a!=0xe0||p[1]>=0xa0)) {
        v=((a&15u)<<12)|((p[1]&63u)<<6)|(p[2]&63u); w=3;
    } else if(n>=3&&a>=0xf0&&a<=0xf4&&(p[1]&0xc0)==0x80&&(p[2]&0xc0)==0x80&&
              (a!=0xf0||p[1]>=0x90)&&(a!=0xf4||p[1]<0x90)) {
        v=((a&7u)<<18)|((p[1]&63u)<<12)|((p[2]&63u)<<6);
        if(n>=4&&(p[3]&0xc0)==0x80) { v|=p[3]&63u; w=4; }
        else v=0xd800u+((v-0x10000u)>>10);
    }
    if(v>0xffffu) {
        v-=0x10000u; out[0]=(Tcl_UniChar)(0xd800u+(v>>10));
        out[1]=(Tcl_UniChar)(0xdc00u+(v&1023u)); *count=2;
    } else { out[0]=(Tcl_UniChar)v; *count=1; }
    return w;
}
int Tcl_NumUtfChars(const char *s,int length) {
    if(!s) return 0;
    size_t n=length<0?strlen(s):(size_t)length,pos=0,total=0;
    while(pos<n) {
        Tcl_UniChar pair[2]; size_t units;
        pos+=decode_units((const unsigned char *)s+pos,n-pos,pair,&units);
        total+=units;
    }
    if(total>INT_MAX) Tcl_Panic("Unicode count overflow");
    return (int)total;
}
Tcl_UniChar *Tcl_UtfToUniCharDString(const char *s,int length,Tcl_DString *d) {
    if(!s||!d) Tcl_Panic("invalid Unicode conversion");
    size_t n=length<0?strlen(s):(size_t)length;
    if(n>((size_t)INT_MAX-2)/sizeof(Tcl_UniChar)) Tcl_Panic("Unicode conversion overflow");
    Tcl_UniChar *tmp=(Tcl_UniChar *)Tcl_Alloc((unsigned)((n+1)*sizeof *tmp));
    size_t pos=0,used=0;
    while(pos<n) {
        Tcl_UniChar pair[2]; size_t units;
        pos+=decode_units((const unsigned char *)s+pos,n-pos,pair,&units);
        tmp[used++]=pair[0]; if(units==2) tmp[used++]=pair[1];
    }
    tmp[used]=0;
    int start=d->length;
    /* Reserve both bytes of the UTF-16 terminator, but exclude them from
       DStringLength. Build separately so input may alias the DString. */
    Tcl_DStringAppend(d,(const char *)tmp,(int)((used+1)*sizeof *tmp));
    d->length-=(int)sizeof *tmp;
    Tcl_Free((char *)tmp);
    return (Tcl_UniChar *)(void *)(d->string+start);
}

/* Search scalar characters, joining adjacent encoded surrogate units.
   The terminating C NUL participates in the search, unlike counted APIs. */
static const char *find_utf(const char *s,int ch,int last) {
    if(!s||ch<0||ch>0x10ffff) return NULL;
    const unsigned char *p=(const unsigned char *)s;
    size_t n=strlen(s),pos=0;
    const char *found=NULL;
    while(pos<n) {
        Tcl_UniChar pair[2]; size_t units;
        size_t width=decode_units(p+pos,n-pos,pair,&units);
        unsigned v=pair[0];
        if(units==1&&v>=0xd800u&&v<=0xdbffu&&width<n-pos) {
            Tcl_UniChar next[2]; size_t nextUnits;
            size_t nextWidth=decode_units(p+pos+width,n-pos-width,next,&nextUnits);
            if(nextUnits==1&&next[0]>=0xdc00u&&next[0]<=0xdfffu) {
                pair[1]=next[0]; units=2; width+=nextWidth;
            }
        }
        if(units==2) v=0x10000u+((v-0xd800u)<<10)+(pair[1]-0xdc00u);
        if(v==(unsigned)ch) {
            found=s+pos;
            if(!last) return found;
        }
        pos+=width;
    }
    return ch==0?s+n:found;
}
const char *Tcl_UtfFindFirst(const char *s,int ch) { return find_utf(s,ch,0); }
const char *Tcl_UtfFindLast(const char *s,int ch) { return find_utf(s,ch,1); }

/* UTF-16 logical indices share one traversal. Clamp at the C terminator. */
static const char *utf_index(const char *s,int index,Tcl_UniChar *unit) {
    if(unit) *unit=0;
    if(!s||index<0) return s;
    size_t n=strlen(s),pos=0;
    while(pos<n) {
        Tcl_UniChar pair[2]; size_t units;
        size_t width=decode_units((const unsigned char *)s+pos,n-pos,pair,&units);
        if(index==0) { if(unit) *unit=pair[0]; return s+pos; }
        if(units==2&&index==1) { if(unit) *unit=pair[1]; return s+pos+1; }
        index-=(int)units; pos+=width;
    }
    return s+n;
}
const char *Tcl_UtfAtIndex(const char *s,int index) { return utf_index(s,index,NULL); }
Tcl_UniChar Tcl_UniCharAtIndex(const char *s,int index) {
    Tcl_UniChar unit; utf_index(s,index,&unit); return unit;
}
const char *Tcl_UtfNext(const char *s) {
    if(!s||!*s) return s;
    size_t n=1;
    while(n<4&&s[n]) ++n;
    Tcl_UniChar pair[2]; size_t units;
    size_t width=decode_units((const unsigned char *)s,n,pair,&units);
    return s+(units==2?1:width);
}

/* Reverse traversal recognizes valid one-, two- and three-byte prefixes.
   Tcl's four-byte scalar traversal exposes the high-surrogate byte. */
const char *Tcl_UtfPrev(const char *s,const char *start) {
    if(!s||!start||s<=start) return start;
    const unsigned char *p=(const unsigned char *)s;
    size_t left=(size_t)(s-start);
    if(left>=2&&(p[-1]&0xc0)==0x80) {
        unsigned a=p[-2],b=p[-1];
        if((a==0xc0&&b==0x80)||(a>=0xc2&&a<=0xdf)||
           (a>=0xe0&&a<=0xef&&(a!=0xe0||b>=0xa0))) return s-2;
        if(left>=3&&(a&0xc0)==0x80) {
            unsigned lead=p[-3];
            if(lead>=0xe0&&lead<=0xef&&(lead!=0xe0||a>=0xa0)) return s-3;
        }
    }
    return s-1;
}
int Tcl_UtfToUniChar(const char *s,Tcl_UniChar *unit) {
    if(!s||!unit) return 0;
    const unsigned char *p=(const unsigned char *)s;
    if(p[0]<0x80) { *unit=p[0]; return 1; }
    size_t n=1; while(n<4&&p[n]) ++n;
    unsigned seed=(p[0]&0xc0)==0x80&&n>=3?*unit:0;
    if(n>=3&&seed>=0xd800&&seed<=0xdbff&&
       (p[0]&0xc0)==0x80&&(p[1]&0xc0)==0x80&&(p[2]&0xc0)==0x80) {
        unsigned high=((seed-0xd800u)<<10)+0x10000u;
        unsigned tail=((p[0]&63u)<<12)|((p[1]&63u)<<6)|(p[2]&63u);
        if((high&0x3fc00u)==(tail&0x3fc00u)) {
            *unit=(Tcl_UniChar)(0xdc00u+(tail&1023u)); return 3;
        }
    }
    Tcl_UniChar pair[2]; size_t units;
    size_t width=decode_units(p,n,pair,&units);
    *unit=pair[0]; return units==2?1:(int)width;
}
int Tcl_UtfNcmp(const char *a,const char *b,unsigned long count) {
    Tcl_UniChar x=0,y=0;
    while(count--) {
        int nx=Tcl_UtfToUniChar(a,&x),ny=Tcl_UtfToUniChar(b,&y);
        if(x!=y) return (int)x-(int)y;
        if(!*a&&!*b) return 0;
        if(*a) a+=nx;
        if(*b) b+=ny;
    }
    return 0;
}
static int utf_hex(unsigned char c) {
    if(c>='0'&&c<='9') return c-'0';
    if(c>='a'&&c<='f') return c-'a'+10;
    if(c>='A'&&c<='F') return c-'A'+10;
    return -1;
}
static unsigned utf_escape(const char *s,int *read) {
    unsigned ch='\\'; int n=1;
    if(s&&s[0]=='\\'&&s[1]) {
        unsigned c=(unsigned char)s[1]; n=2; ch=c;
        switch(c) {
        case 'a': ch=7; break;
        case 'b': ch=8; break;
        case 'f': ch=12; break;
        case 'n': ch=10; break;
        case 'r': ch=13; break;
        case 't': ch=9; break;
        case 'v': ch=11; break;
        case '\n':
            ch=' '; while(s[n]==' '||s[n]=='\t') ++n; break;
        case 'x': case 'u': case 'U': {
            int limit=c=='x'?2:c=='u'?4:8,used=0; unsigned value=0;
            while(used<limit) {
                int h=utf_hex((unsigned char)s[n]); if(h<0) break;
                value=value*16u+(unsigned)h; ++n; ++used;
                if(c=='U'&&value>0x10fffu) break;
            }
            if(used) ch=value;
            break;
        }
        default:
            if(c>='0'&&c<='7') {
                ch=c-'0';
                while(n<4&&s[n]>='0'&&s[n]<='7'&&ch<32) {
                    ch=ch*8u+(unsigned)(s[n]-'0'); ++n;
                }
            } else if(c>=0x80) {
                Tcl_UniChar first=0; int width=Tcl_UtfToUniChar(s+1,&first);
                ch=first; n=1+width;
                if(first>=0xd800&&first<=0xdbff&&s[n]) {
                    Tcl_UniChar second=first; int w=Tcl_UtfToUniChar(s+n,&second);
                    if(second>=0xdc00&&second<=0xdfff) { ch=0x10000u+((first-0xd800u)<<10)+(second-0xdc00u); n+=w; }
                }
            }
        }
    }
    if(read) *read=n;
    return ch;
}
int Tcl_UtfBackslash(const char *s,int *read,char *dst) {
    return Tcl_UniCharToUtf((int)utf_escape(s,read),dst);
}
char Tcl_Backslash(const char *s,int *read) {
    unsigned ch=utf_escape(s,read);
    return (char)(ch>0xffff?0xfffd:ch);
}

/* Decode a scalar for glob matching; the separate object API counts UTF-16
   units, but a glob '?' consumes a supplementary character as one unit. */
static unsigned match_scalar(const char **s) {
    const char *p=*s; Tcl_UniChar a=0;
    int n=Tcl_UtfToUniChar(p,&a); p+=n;
    if(a>=0xd800&&a<=0xdbff&&*p) {
        Tcl_UniChar b=a; int m=Tcl_UtfToUniChar(p,&b);
        if(b>=0xdc00&&b<=0xdfff) { *s=p+m; return 0x10000u+((a-0xd800u)<<10)+(b-0xdc00u); }
    }
    *s=p; return a;
}
/* Return the continuation after a bracket class, or NULL on mismatch. */
static unsigned match_fold(unsigned v,int nocase) {
    return nocase&&v<=0xffffu?(unsigned)Tcl_UniCharToLower((int)v):v;
}
static const char *match_class(const char *p,unsigned value,int nocase) {
    while(*p&&*p!=']') {
        unsigned lo=match_fold(match_scalar(&p),nocase),hi=lo;
        if(*p=='-') {
            ++p; if(!*p) return NULL;
            hi=match_fold(match_scalar(&p),nocase);
        }
        if((lo<=value&&value<=hi)||(hi<=value&&value<=lo)) {
            while(*p&&*p!=']') ++p;
            return *p?p+1:p;
        }
    }
    return NULL;
}
int Tcl_StringCaseMatch(const char *s,const char *pattern,int nocase) {
    if(!s||!pattern) return 0;
    const char *p=pattern,*star=NULL,*retry=NULL;
    while(1) {
        if(*p=='*') {
            do ++p; while(*p=='*');
            if(!*p) return 1;
            star=p; retry=s;
        }
        if(!*p&&!*s) return 1;
        if(*p&&*s) {
            const char *q=s;
            unsigned value=match_fold(match_scalar(&q),nocase);
            if(*p=='?') { s=q; ++p; continue; }
            if(*p=='[') {
                const char *end=match_class(p+1,value,nocase);
                if(end) { s=q; p=end; continue; }
            } else {
                const char *r=p;
                if(*r=='\\') ++r;
                if(*r&&value==match_fold(match_scalar(&r),nocase)) { s=q; p=r; continue; }
            }
        }
        if(!star||!*retry) return 0;
        (void)match_scalar(&retry); s=retry; p=star;
    }
}

int Tcl_StringMatch(const char *s,const char *pattern) {
    return Tcl_StringCaseMatch(s,pattern,0);
}

/* Tcl 8.6 with 16-bit Tcl_UniChar classifies the low 16 bits.
   A single immutable category table supports all eleven predicates. */
static const struct { uint16_t lo,hi,mask; } unicode_classes[]={
#include "TKGPUUnicodeClasses.H"
};
static unsigned unicode_mask(int ch) {
    unsigned c=(uint16_t)ch;
    size_t lo=0,hi=sizeof unicode_classes/sizeof unicode_classes[0];
    while(lo<hi) {
        size_t mid=lo+(hi-lo)/2;
        if(c<unicode_classes[mid].lo) hi=mid;
        else if(c>unicode_classes[mid].hi) lo=mid+1;
        else return unicode_classes[mid].mask;
    }
    return 0;
}
#define CLASS(name,mask) int Tcl_UniCharIs##name(int ch) { return (unicode_mask(ch)&(mask))!=0; }
CLASS(Alnum,3)
CLASS(Alpha,1)
CLASS(Control,4)
CLASS(Digit,2)
CLASS(Graph,8)
CLASS(Lower,16)
CLASS(Print,8|128)
CLASS(Punct,64)
CLASS(Space,512)
CLASS(Upper,32)
CLASS(WordChar,1|2|256)
#undef CLASS

static const struct { uint16_t lo,hi; int32_t delta[3]; } case_ranges[]={
#include "TKGPUUnicodeCase.H"
};
static int unicode_case(int ch,unsigned which) {
    if(ch<0||ch>0xffff) return ch;
    size_t lo=0,hi=sizeof case_ranges/sizeof case_ranges[0];
    while(lo<hi) {
        size_t mid=lo+(hi-lo)/2;
        if(ch<case_ranges[mid].lo) hi=mid;
        else if(ch>case_ranges[mid].hi) lo=mid+1;
        else return ch+case_ranges[mid].delta[which];
    }
    return ch;
}
Tcl_UniChar Tcl_UniCharToLower(int ch) { return (Tcl_UniChar)unicode_case(ch,0); }
Tcl_UniChar Tcl_UniCharToUpper(int ch) { return (Tcl_UniChar)unicode_case(ch,1); }
Tcl_UniChar Tcl_UniCharToTitle(int ch) { return (Tcl_UniChar)unicode_case(ch,2); }
int Tcl_UniCharNcasecmp(const Tcl_UniChar *a,const Tcl_UniChar *b,unsigned long n) {
    while(n--) {
        int x=*a++,y=*b++;
        if(x!=y) {
            int diff=Tcl_UniCharToLower(x)-Tcl_UniCharToLower(y);
            if(diff) return diff;
        }
    }
    return 0;
}

/* The in-place API must never grow beyond the caller's original buffer.
   Preserve complete scalar pairs and any case mapping that would grow. */
static int utf_change_case(char *s,unsigned mode) {
    if(!s) return 0;
    size_t n=strlen(s),read=0,write=0;
    if(n>INT_MAX) return -1;
    while(read<n) {
        Tcl_UniChar pair[2]; size_t units;
        size_t width=decode_units((const unsigned char *)s+read,n-read,pair,&units);
        int original=pair[0],mapped=original;
        if(units==1) mapped=mode==1?Tcl_UniCharToUpper(original):mode==2&&read==0?Tcl_UniCharToTitle(original):Tcl_UniCharToLower(original);
        if(mode==2&&original>=0x1c90&&original<=0x1cbf) mapped=original;
        char encoded[4]; int m=Tcl_UniCharToUtf(mapped,encoded);
        if(units==1&&mapped!=original&&(size_t)m<=width) {
            memcpy(s+write,encoded,(size_t)m); write+=(size_t)m;
        } else { memmove(s+write,s+read,width); write+=width; }
        read+=width;
    }
    s[write]=0; return (int)write;
}
int Tcl_UtfToLower(char *s) { return utf_change_case(s,0); }
int Tcl_UtfToUpper(char *s) { return utf_change_case(s,1); }
int Tcl_UtfToTitle(char *s) { return utf_change_case(s,2); }
struct utf_cmp_cursor { const unsigned char *p; size_t remaining; Tcl_UniChar pending; int has_pending; };
static int utf_cmp_next(struct utf_cmp_cursor *c) {
    if(c->has_pending) { c->has_pending=0; return c->pending; }
    if(!c->remaining) return 0;
    Tcl_UniChar pair[2]; size_t units;
    size_t width=decode_units(c->p,c->remaining,pair,&units);
    c->p+=width; c->remaining-=width;
    if(units==2) { c->pending=pair[1]; c->has_pending=1; }
    return pair[0];
}
int Tcl_UtfNcasecmp(const char *a,const char *b,unsigned long count) {
    struct utf_cmp_cursor x={(const unsigned char *)a,strlen(a),0,0},y={(const unsigned char *)b,strlen(b),0,0};
    while(count--) {
        int u=utf_cmp_next(&x),v=utf_cmp_next(&y);
        int diff=Tcl_UniCharToLower(u)-Tcl_UniCharToLower(v);
        if(diff) return diff;
        if(!x.remaining&&!y.remaining&&!x.has_pending&&!y.has_pending) return 0;
    }
    return 0;
}
static int glob_fold(int ch,int nocase) { return nocase?Tcl_UniCharToLower(ch):ch; }
static const Tcl_UniChar *uni_glob_class(const Tcl_UniChar *p,int value,int nocase) {
    while(*p&&*p!=']') {
        int lo=glob_fold(*p++,nocase),hi=lo;
        if(*p=='-') { ++p; if(!*p) return NULL; hi=glob_fold(*p++,nocase); }
        if((lo<=value&&value<=hi)||(hi<=value&&value<=lo)) {
            while(*p&&*p!=']') ++p;
            return *p?p+1:p;
        }
    }
    return NULL;
}
int Tcl_UniCharCaseMatch(const Tcl_UniChar *s,const Tcl_UniChar *pattern,int nocase) {
    if(!s||!pattern) return 0;
    const Tcl_UniChar *p=pattern,*star=NULL,*retry=NULL;
    while(1) {
        if(*p=='*') {
            do ++p; while(*p=='*');
            if(!*p) return 1;
            star=p; retry=s;
        }
        if(!*p&&!*s) return 1;
        if(*p&&*s) {
            int value=glob_fold(*s,nocase);
            if(*p=='?') { ++s; ++p; continue; }
            if(*p=='[') {
                const Tcl_UniChar *end=uni_glob_class(p+1,value,nocase);
                if(end) { ++s; p=end; continue; }
            } else {
                const Tcl_UniChar *r=p;
                if(*r=='\\') ++r;
                if(*r&&value==glob_fold(*r,nocase)) { ++s; p=r+1; continue; }
            }
        }
        if(!star||!*retry) return 0;
        s=++retry; p=star;
    }
}
