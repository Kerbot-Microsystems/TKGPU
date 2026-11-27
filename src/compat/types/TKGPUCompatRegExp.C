#include "TKGPUCompat.H"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CAP 16

struct Tcl_RegExp_ {
    char *pattern;
    int flags;
    int nsubs;
    const char *last_text;
    Tcl_RegExpIndices matches[MAX_CAP];
};

static int char_eq(unsigned char a,unsigned char b,int nocase) {
    if(!nocase) return a==b;
    return tolower(a)==tolower(b);
}

static int match_class(const char *pat,const char **next,unsigned char ch,int nocase) {
    int neg=0;
    if(*pat=='^') { neg=1; ++pat; }
    int matched=0,first=1;
    while(*pat&&(*pat!=']'||first)) {
        first=0;
        unsigned char c1=(unsigned char)*pat++;
        if(c1=='\\'&&*pat) c1=(unsigned char)*pat++;
        if(*pat=='-'&&pat[1]&&pat[1]!=']') {
            ++pat;
            unsigned char c2=(unsigned char)*pat++;
            if(c2=='\\'&&*pat) c2=(unsigned char)*pat++;
            unsigned char t=nocase?(unsigned char)tolower(ch):ch;
            unsigned char lo=nocase?(unsigned char)tolower(c1):c1;
            unsigned char hi=nocase?(unsigned char)tolower(c2):c2;
            if(t>=lo&&t<=hi) matched=1;
        } else if(char_eq(ch,c1,nocase)) {
            matched=1;
        }
    }
    if(*pat==']') ++pat;
    *next=pat;
    return neg?!matched:matched;
}

static int match_atom(const char *pat,const char **next,unsigned char ch,int nocase) {
    if(*pat=='.') { *next=pat+1; return ch!='\0'; }
    if(*pat=='[') return match_class(pat+1,next,ch,nocase);
    if(*pat=='\\'&&pat[1]) {
        char e=pat[1]; *next=pat+2;
        if(e=='d') return isdigit(ch)!=0;
        if(e=='D') return isdigit(ch)==0;
        if(e=='s') return isspace(ch)!=0;
        if(e=='S') return isspace(ch)==0;
        if(e=='w') return (isalnum(ch)||ch=='_')!=0;
        if(e=='W') return (isalnum(ch)||ch=='_')==0;
        if(e=='n') return ch=='\n';
        if(e=='t') return ch=='\t';
        if(e=='r') return ch=='\r';
        return char_eq(ch,(unsigned char)e,nocase);
    }
    *next=pat+1;
    return char_eq(ch,(unsigned char)*pat,nocase);
}

static const char *skip_atom(const char *p) {
    if(*p=='\\'&&p[1]) return p+2;
    if(*p=='[') {
        ++p; if(*p=='^') ++p; if(*p==']') ++p;
        while(*p&&*p!=']') { if(*p=='\\'&&p[1]) p+=2; else ++p; }
        return *p==']'?p+1:p;
    }
    return *p?p+1:p;
}

static const char *match_here(const char *pat,const char *end,const char *text,const char *base,
                              int nocase,Tcl_RegExpIndices *caps,int *cap_idx) {
    while(pat<end) {
        if(*pat=='$') {
            if(pat+1==end) return *text=='\0'?text:NULL;
        }
        if(*pat=='(') {
            int depth=1;
            const char *q=pat+1;
            while(q<end&&depth>0) {
                if(*q=='\\'&&q+1<end) { q+=2; continue; }
                if(*q=='(') ++depth;
                else if(*q==')') --depth;
                ++q;
            }
            if(depth==0) {
                int idx=(*cap_idx<MAX_CAP)?(*cap_idx)++:-1;
                const char *inner_end=q-1;
                const char *after_group=q;
                const char *m=match_here(pat+1,inner_end,text,base,nocase,caps,cap_idx);
                if(!m) return NULL;
                if(idx>=0) { caps[idx].start=(long)(text-base); caps[idx].end=(long)(m-base); }
                pat=after_group; text=m;
                continue;
            }
        }
        const char *after=skip_atom(pat);
        char quant=(after<end&&(*after=='*'||*after=='+'||*after=='?'))?*after:0;
        const char *rest=quant?after+1:after;
        if(quant=='*'||quant=='+') {
            const char *t=text;
            const char *dummy=NULL;
            if(quant=='+') {
                if(!*t||!match_atom(pat,&dummy,(unsigned char)*t,nocase)) return NULL;
                ++t;
            }
            const char *start_min=t;
            while(*t&&match_atom(pat,&dummy,(unsigned char)*t,nocase)) ++t;
            for(const char *p=t;p>=start_min;p--) {
                int saved_idx=*cap_idx;
                const char *r=match_here(rest,end,p,base,nocase,caps,cap_idx);
                if(r) return r;
                *cap_idx=saved_idx;
            }
            return NULL;
        }
        if(quant=='?') {
            const char *dummy=NULL;
            if(*text&&match_atom(pat,&dummy,(unsigned char)*text,nocase)) {
                int saved_idx=*cap_idx;
                const char *r=match_here(rest,end,text+1,base,nocase,caps,cap_idx);
                if(r) return r;
                *cap_idx=saved_idx;
            }
            pat=rest;
            continue;
        }
        const char *dummy=NULL;
        if(!*text||!match_atom(pat,&dummy,(unsigned char)*text,nocase)) return NULL;
        pat=after;
        ++text;
    }
    return text;
}

static const char *match_branches(const char *pat,const char *end,const char *text,const char *base,
                                  int nocase,Tcl_RegExpIndices *caps) {
    const char *p=pat;
    for(;;) {
        int depth=0;
        const char *q=p;
        while(q<end) {
            if(*q=='\\'&&q+1<end) { q+=2; continue; }
            if(*q=='(') ++depth;
            else if(*q==')'&&depth>0) --depth;
            else if(*q=='|'&&depth==0) break;
            ++q;
        }
        int cap_idx=1;
        const char *sub=p;
        int anchored=(sub<q&&*sub=='^');
        if(anchored) {
            if(text==base) {
                const char *m=match_here(sub+1,q,text,base,nocase,caps,&cap_idx);
                if(m) return m;
            }
        } else {
            const char *m=match_here(sub,q,text,base,nocase,caps,&cap_idx);
            if(m) return m;
        }
        if(q>=end) break;
        p=q+1;
    }
    return NULL;
}

/* Reject malformed syntax before matching: a compile failure is distinct
   from a valid expression that happens not to match the supplied text. */
static const char *validate_pattern(const char *p) {
    const char *begin=p;
    int groups=0,atom=0,quantified=0;
    for(;*p;p++) {
        if(*p=='\\') {
            if(!p[1]) return "REG_EESCAPE";
            ++p; atom=1; quantified=0; continue;
        }
        if(*p=='[') {
            ++p;
            if(*p=='^') ++p;
            if(*p==']') ++p;
            const char *class_start=p;
            for(;*p&&*p!=']';p++) {
                if(*p=='\\'&&p[1]) { ++p; continue; }
                if(*p=='-'&&p>class_start&&p[1]&&p[1]!=']'&&p[1]!='\\'&&
                   (unsigned char)p[-1]<128&&(unsigned char)p[1]<128&&p[-1]>p[1]) return "REG_ERANGE";
                if(*p=='['&&(p[1]==':'||p[1]=='.'||p[1]=='=')) {
                    char delimiter=*++p;
                    do { ++p; } while(*p&&!(p[0]==delimiter&&p[1]==']'));
                    if(!*p) return "REG_EBRACK";
                    ++p;
                }
            }
            if(!*p) return "REG_EBRACK";
            atom=1; quantified=0; continue;
        }
        if(*p=='(') { groups++; atom=0; quantified=0; continue; }
        if(*p==')') {
            if(!groups) return "REG_EPAREN";
            groups--; atom=1; quantified=0; continue;
        }
        if(*p=='|') { atom=0; quantified=0; continue; }
        if(*p=='*'||*p=='+'||*p=='?') {
            /* ARE group prefixes and non-greedy suffixes are legal syntax. */
            if(*p=='?'&&p>begin&&p[-1]=='(') continue;
            if(*p=='?'&&quantified==1) { quantified=2; continue; }
            if(!atom||quantified) return "REG_BADRPT";
            quantified=1; continue;
        }
        atom=(*p!='^'&&*p!='$'); quantified=0;
    }
    return groups?"REG_EPAREN":NULL;
}
static void regexp_error(Tcl_Interp *i,const char *code) {
    if(!i) return;
    const char *message=!strcmp(code,"REG_EBRACK")?"brackets [] not balanced":
        !strcmp(code,"REG_EPAREN")?"parentheses () not balanced":
        !strcmp(code,"REG_EESCAPE")?"invalid escape sequence":
        !strcmp(code,"REG_ERANGE")?"invalid character range":"quantifier operand invalid";
    Tcl_SetErrorCode(i,"REGEXP",code,message,(char *)NULL);
    Tcl_SetObjResult(i,Tcl_ObjPrintf("couldn't compile regular expression pattern: %s",message));
}

static struct Tcl_RegExp_ *compile_re(Tcl_Interp *i,const char *pattern,int flags) {
    if(!pattern) return NULL;
    const char *invalid=validate_pattern(pattern);
    if(invalid) { regexp_error(i,invalid); return NULL; }
    struct Tcl_RegExp_ *re=calloc(1,sizeof *re);
    if(!re) Tcl_Panic("cannot allocate regexp");
    size_t n=strlen(pattern);
    re->pattern=malloc(n+1);
    if(!re->pattern) Tcl_Panic("cannot copy regexp pattern");
    memcpy(re->pattern,pattern,n+1);
    re->flags=flags;
    int subs=0;
    for(size_t k=0;k<n;k++) {
        if(pattern[k]=='\\'&&k+1<n) { ++k; continue; }
        if(pattern[k]=='(') ++subs;
    }
    re->nsubs=subs;
    for(int k=0;k<MAX_CAP;k++) { re->matches[k].start=-1; re->matches[k].end=-1; }
    return re;
}

Tcl_RegExp Tcl_RegExpCompile(Tcl_Interp *i,const char *pattern) {
    return compile_re(i,pattern,0);
}

Tcl_RegExp Tcl_GetRegExpFromObj(Tcl_Interp *i,Tcl_Obj *patObj,int flags) {
    if(!patObj) return NULL;
    return compile_re(i,Tcl_GetString(patObj),flags);
}

int Tcl_RegExpExec(Tcl_Interp *i,Tcl_RegExp re,const char *text,const char *start) {
    (void)i;
    if(!re||!text) return -1;
    if(!start) start=text;
    re->last_text=text;
    for(int k=0;k<MAX_CAP;k++) { re->matches[k].start=-1; re->matches[k].end=-1; }
    int nocase=(re->flags&2)!=0;
    const char *pend=re->pattern+strlen(re->pattern);
    for(const char *t=text;;++t) {
        const char *m=match_branches(re->pattern,pend,t,text,nocase,re->matches);
        if(m) {
            re->matches[0].start=(long)(t-text);
            re->matches[0].end=(long)(m-text);
            return 1;
        }
        if(!*t) break;
    }
    return 0;
}

int Tcl_RegExpMatch(Tcl_Interp *i,const char *text,const char *pattern) {
    Tcl_RegExp re=Tcl_RegExpCompile(i,pattern);
    if(!re) return -1;
    int rc=Tcl_RegExpExec(i,re,text,text);
    free(re->pattern);
    free(re);
    return rc;
}

void Tcl_RegExpRange(Tcl_RegExp re,int index,const char **startPtr,const char **endPtr) {
    if(!re||index<0||index>=MAX_CAP||!re->last_text||re->matches[index].start<0) {
        if(startPtr) *startPtr=NULL;
        if(endPtr) *endPtr=NULL;
        return;
    }
    if(startPtr) *startPtr=re->last_text+re->matches[index].start;
    if(endPtr) *endPtr=re->last_text+re->matches[index].end;
}

int Tcl_RegExpExecObj(Tcl_Interp *i,Tcl_RegExp re,Tcl_Obj *textObj,int offset,int nmatches,int flags) {
    (void)nmatches; (void)flags;
    if(!re||!textObj) return -1;
    int len=0;
    const char *text=Tcl_GetStringFromObj(textObj,&len);
    if(offset<0) offset=0;
    if(offset>len) offset=len;
    int rc=Tcl_RegExpExec(i,re,text+offset,text);
    if(rc==1&&offset>0) {
        for(int k=0;k<MAX_CAP;k++) {
            if(re->matches[k].start>=0) {
                re->matches[k].start+=offset;
                re->matches[k].end+=offset;
            }
        }
    }
    return rc;
}

int Tcl_RegExpMatchObj(Tcl_Interp *i,Tcl_Obj *textObj,Tcl_Obj *patObj) {
    if(!textObj||!patObj) return -1;
    return Tcl_RegExpMatch(i,Tcl_GetString(textObj),Tcl_GetString(patObj));
}

void Tcl_RegExpGetInfo(Tcl_RegExp re,Tcl_RegExpInfo *infoPtr) {
    if(!re||!infoPtr) return;
    infoPtr->nsubs=re->nsubs;
    infoPtr->matches=re->matches;
    infoPtr->extendStart=re->matches[0].start;
    infoPtr->reserved=0;
}
