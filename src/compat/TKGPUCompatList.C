#include "TKGPUCompat.H"
#include "../core/TKGPUCoreInternal.H"
#include "../core/TKGPUList.H"
#include <limits.h>
#include <string.h>
int Tcl_SplitList(Tcl_Interp *i,const char *source,int *argc,const char ***argv) {
    if(!source||!argc||!argv) return TG_ERROR;
    Tcl_Obj *list=Tcl_NewStringObj(source,-1);
    if(!list||!TKGPU_RetainObject(list)) return TG_ERROR;
    int count=0; Tcl_Obj **items=NULL;
    if(Tcl_ListObjGetElements(i,list,&count,&items)!=TG_OK) {
        TKGPU_ReleaseObject(list); return TG_ERROR;
    }
    size_t bytes=0;
    for(int k=0;k<count;k++) {
        int length=0;
        if(!Tcl_GetStringFromObj(items[k],&length)||bytes>SIZE_MAX-(size_t)length-1) goto error;
        bytes+=(size_t)length+1;
    }
    if((size_t)count>SIZE_MAX/sizeof(char *)-1) goto error;
    size_t head=((size_t)count+1)*sizeof(char *);
    if(bytes>SIZE_MAX-head||head+bytes>UINT_MAX) goto error;
    char *memory=Tcl_AttemptAlloc((unsigned int)(head+bytes));
    if(!memory) goto error;
    const char **vector=(const char **)memory;
    char *dest=memory+head;
    for(int k=0;k<count;k++) {
        int length=0;
        const char *value=Tcl_GetStringFromObj(items[k],&length);
        memcpy(dest,value,(size_t)length+1); vector[k]=dest; dest+=(size_t)length+1;
    }
    vector[count]=NULL; *argc=count; *argv=vector;
    TKGPU_ReleaseObject(list);
    return TG_OK;
error:
    TKGPU_ReleaseObject(list);
    if(i) tg_set_result(i,"list allocation overflow or failure",TG_ERROR);
    return TG_ERROR;
}

static int concat_space(char c) {
    return c==' '||c=='\t'||c=='\r'||c=='\n'||c=='\v'||c=='\f';
}
static void concat_part(Tcl_DString *d,const char *s,int length) {
    size_t n=(size_t)length;
    while(n&&concat_space(*s)) { ++s; --n; }
    /* Preserve the whitespace immediately following a trailing backslash,
       including double backslashes, as Tcl_Concat does. */
    while(n&&concat_space(s[n-1])&&(n<2||s[n-2]!='\\')) --n;
    if(n) {
        if(d->length) Tcl_DStringAppend(d," ",1);
        Tcl_DStringAppend(d,s,(int)n);
    }
}
static char *finish_string(Tcl_DString *d) {
    char *out=Tcl_Alloc((unsigned)d->length+1u);
    memcpy(out,d->string,(size_t)d->length+1);
    Tcl_DStringFree(d); return out;
}
char *Tcl_Merge(int count,const char *const argv[]) {
    if(count<0||(count&&!argv)) Tcl_Panic("invalid Merge arguments");
    Tcl_DString d; Tcl_DStringInit(&d);
    for(int k=0;k<count;k++) Tcl_DStringAppendElement(&d,argv[k]);
    return finish_string(&d);
}
char *Tcl_Concat(int count,const char *const argv[]) {
    if(count<0||(count&&!argv)) Tcl_Panic("invalid Concat arguments");
    Tcl_DString d; Tcl_DStringInit(&d);
    for(int k=0;k<count;k++) {
        if(!argv[k]) Tcl_Panic("null Concat argument");
        size_t n=strlen(argv[k]);
        if(n>INT_MAX) Tcl_Panic("Concat length overflow");
        concat_part(&d,argv[k],(int)n);
    }
    return finish_string(&d);
}
Tcl_Obj *Tcl_ConcatObj(int count,Tcl_Obj *const argv[]) {
    if(count<0||(count&&!argv)) return NULL;
    Tcl_DString d; Tcl_DStringInit(&d);
    for(int k=0;k<count;k++) {
        int n=0; const char *s=Tcl_GetStringFromObj(argv[k],&n);
        if(!s||memchr(s,0,(size_t)n)) { Tcl_DStringFree(&d); return NULL; }
        concat_part(&d,s,n);
    }
    Tcl_Obj *out=Tcl_NewStringObj(d.string,d.length);
    Tcl_DStringFree(&d); return out;
}

/* The public flags reserve bits 1 and 8 for caller options; bits 2/4
   encode the selected quoting strategy. Work on bytes, including NUL. */
static int list_escape_char(unsigned char c) {
    switch(c) {
    case '\t': return 't'; case '\n': return 'n'; case '\r': return 'r';
    case '\v': return 'v'; case '\f': return 'f';
    case ' ': case '[': case ']': case '$': case ';': case '"':
    case '\\': case '{': case '}': return c;
    default: return 0;
    }
}
static int counted_len(const char *s,int n) {
    if(n>=0) return n;
    size_t z=s?strlen(s):0; return z>INT_MAX?-1:(int)z;
}
int Tcl_ScanCountedElement(const char *s,int length,int *flags) {
    int n=counted_len(s,length),depth=0,mode=0,broken=0,skip=0;
    if(n<0||(!s&&n)||!flags) return -1;
    if(n==0) { *flags=2; return 2; }
    int hash=s[0]=='#'; size_t escaped=(size_t)n+(unsigned)hash;
    if(s[0]=='{'||s[0]=='"') mode=2;
    for(int k=0;k<n;k++) {
        unsigned char c=(unsigned char)s[k];
        if(list_escape_char(c)) ++escaped;
        if(skip) { skip=0; continue; }
        switch(c) {
        case '\\':
            mode=2;
            if(k==n-1||s[k+1]=='\n') broken=1;
            skip=1; break;
        case '{': ++depth; break;
        case '}': if(--depth<0) broken=1; break;
        case ' ': case '\t': case '\n': case '\r': case '\v': case '\f':
        case '$': case '[': case ';': mode=2; break;
        case ']': case '"': if(!mode) mode=6; break;
        }
    }
    if(depth||broken) mode=4;
    if(hash&&mode==6) mode=2;
    size_t size=(size_t)n;
    if(mode==4) size=escaped;
    else if(mode||hash) { size+=2+(unsigned)(hash&&mode); if(mode&&escaped>size) size=escaped; }
    if(size>INT_MAX) return -1;
    *flags=mode; return (int)size;
}
int Tcl_ScanElement(const char *s,int *flags) { return Tcl_ScanCountedElement(s,-1,flags); }
int Tcl_ConvertCountedElement(const char *s,int length,char *dst,int flags) {
    int n=counted_len(s,length);
    if(n<0||(!s&&n)||!dst) return -1;
    unsigned mode=(unsigned)flags&6u; int hash=n&&s[0]=='#'&&!(flags&8);
    if(!n) { memcpy(dst,"{}",3); return 2; }
    if((mode==2||mode==6)&&(flags&1)) mode=4;
    if(hash&&mode!=4) mode=2;
    if(mode==2) {
        if(n>INT_MAX-2) return -1;
        dst[0]='{'; memcpy(dst+1,s,(size_t)n); dst[n+1]='}'; dst[n+2]=0; return n+2;
    }
    if(!mode) { memcpy(dst,s,(size_t)n); dst[n]=0; return n; }
    if(n>INT_MAX/2) return -1;
    int used=0;
    if(hash) dst[used++]='\\';
    for(int k=0;k<n;k++) {
        unsigned char c=(unsigned char)s[k]; int esc=list_escape_char(c);
        if(mode==6&&(c=='{'||c=='}')) esc=0;
        if(esc) { dst[used++]='\\'; dst[used++]=(char)esc; }
        else dst[used++]=(char)c;
    }
    dst[used]=0; return used;
}
int Tcl_ConvertElement(const char *s,char *dst,int flags) { return Tcl_ConvertCountedElement(s,-1,dst,flags); }
