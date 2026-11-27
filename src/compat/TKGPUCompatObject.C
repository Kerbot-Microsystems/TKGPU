#include "TKGPUCompat.H"
#include "../object/TKGPUObjectInternal.H"
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include "TKGPUChecked.H"
#include "types/TKGPUCompatTypes.H"
/* Tcl constructors return unowned objects; use retain to keep them. */
Tcl_Obj *Tcl_NewStringObj(const char *s,int length) {
    if(length< -1||(!s&&length!=0)) return NULL;
    size_t n=length<0?strlen(s):(size_t)length;
    return tg_compat_new_object(tg_object_string(s,n));
}
char *Tcl_GetStringFromObj(Tcl_Obj *o,int *length) {
    size_t n; const char *s=tg_object_bytes(o,&n);
    if(!s||n>INT_MAX) return NULL;
    if(length) *length=(int)n;
    return (char *)s;
}
char *Tcl_GetString(Tcl_Obj *o) { return Tcl_GetStringFromObj(o,NULL); }
void TKGPU_ReleaseObject(Tcl_Obj *o) { tg_object_release(o); }
Tcl_Obj *TKGPU_RetainObject(Tcl_Obj *o) { return tg_object_retain(o); }

Tcl_Obj *Tcl_NewObj(void) { return Tcl_NewStringObj("",0); }
/* Mutation is only legal for unshared objects. Allocate before replacing
   the old representation, including when input aliases the old bytes. */
static void replace_string(Tcl_Obj *o,char *bytes,int length) {
    tg_object_clear_type(o); tg_object_clear_caches(o);
    free(o->bytes); o->bytes=bytes; o->length=length;
}
void Tcl_SetStringObj(Tcl_Obj *o,const char *s,int length) {
    if(!o||o->refCount>1||length< -1||(!s&&length!=0)) abort();
    size_t n=length<0?strlen(s):(size_t)length;
    if(n>INT_MAX) abort();
    char *next=malloc(n+1); if(!next) abort();
    if(n) memcpy(next,s,n); next[n]=0;
    replace_string(o,next,(int)n);
}
void Tcl_AppendToObj(Tcl_Obj *o,const char *s,int length) {
    if(!o||o->refCount>1||length< -1||(!s&&length!=0)) abort();
    size_t old,n=length<0?strlen(s):(size_t)length,total;
    const char *value=tg_object_bytes(o,&old);
    if(!value||tg_size_add(old,n,&total)!=TG_OK||total>INT_MAX) abort();
    if(!n) return;
    char *next=malloc(total+1); if(!next) abort();
    memcpy(next,value,old); memcpy(next+old,s,n); next[total]=0;
    replace_string(o,next,(int)total);
}
void Tcl_InvalidateStringRep(Tcl_Obj *o) {
    if(!o) return;
    /* Only types capable of reconstructing the string can invalidate it.
       The existing built-in preview representations are string-backed. */
    if(!o->typePtr||!o->typePtr->updateStringProc) return;
    if(o->typePtr==&tg_type_bytes) {
        /* The binary buffer is authoritative and must survive invalidation.
           Drop only derived caches, including old list children. */
        unsigned char *binary=o->binary; size_t length=o->binary_length;
        o->binary=NULL; o->binary_length=0;
        tg_object_clear_caches(o);
        o->binary=binary; o->binary_length=length;
    } else tg_object_clear_caches(o);
    free(o->bytes); o->bytes=NULL; o->length=0;
}

/* Consume a newly constructed representation, leaving the target reference
   count and address intact. The constructor copied any aliased input. */
static void assign_object(Tcl_Obj *o,Tcl_Obj *next) {
    if(!o||o->refCount>1||!next) abort();
    int refs=o->refCount;
    tg_object_clear_type(o); tg_object_clear_caches(o); free(o->bytes);
    *o=*next; o->refCount=refs; free(next);
}
void Tcl_SetIntObj(Tcl_Obj *o,int value) { assign_object(o,Tcl_NewIntObj(value)); }
void Tcl_SetLongObj(Tcl_Obj *o,long value) { assign_object(o,Tcl_NewLongObj(value)); }
void Tcl_SetWideIntObj(Tcl_Obj *o,long long value) { assign_object(o,Tcl_NewWideIntObj(value)); }
void Tcl_SetDoubleObj(Tcl_Obj *o,double value) { assign_object(o,Tcl_NewDoubleObj(value)); }
void Tcl_SetBignumObj(Tcl_Obj *o,mp_int *value) {
    if(!o||o->refCount>1||!value) Tcl_Panic("invalid or shared bignum mutation");
    assign_object(o,Tcl_NewBignumObj(value));
}
void Tcl_SetByteArrayObj(Tcl_Obj *o,const unsigned char *value,int length) {
    assign_object(o,Tcl_NewByteArrayObj(value,length));
}

void Tcl_SetBooleanObj(Tcl_Obj *o,int value) { Tcl_SetIntObj(o,value!=0); }
void Tcl_SetListObj(Tcl_Obj *o,int count,Tcl_Obj *const values[]) {
    if(!o||o->refCount>1||count<0||(count&&!values)) abort();
    for(int k=0;k<count;k++) if(!values[k]||values[k]==o) abort();
    assign_object(o,Tcl_NewListObj(count,values));
}
void Tcl_SetUnicodeObj(Tcl_Obj *o,const Tcl_UniChar *s,int length) {
    assign_object(o,Tcl_NewUnicodeObj(s,length));
}
int Tcl_GetCharLength(Tcl_Obj *o) {
    int n=0; (void)Tcl_GetUnicodeFromObj(o,&n); return n;
}
Tcl_UniChar Tcl_GetUniChar(Tcl_Obj *o,int index) {
    int n; const Tcl_UniChar *s=Tcl_GetUnicodeFromObj(o,&n);
    return s&&index>=0&&index<n?s[index]:0;
}
Tcl_Obj *Tcl_GetRange(Tcl_Obj *o,int first,int last) {
    int n; const Tcl_UniChar *s=Tcl_GetUnicodeFromObj(o,&n);
    if(!s) return NULL;
    if(first<0) first=0;
    if(last>=n) last=n-1;
    return last<first?Tcl_NewObj():Tcl_NewUnicodeObj(s+first,last-first+1);
}
void Tcl_AppendUnicodeToObj(Tcl_Obj *o,const Tcl_UniChar *s,int length) {
    if(!o||o->refCount>1) abort();
    Tcl_Obj *part=Tcl_NewUnicodeObj(s,length);
    if(!part) abort();
    int n; const char *bytes=Tcl_GetStringFromObj(part,&n);
    if(!bytes) abort();
    Tcl_AppendToObj(o,bytes,n);
    TKGPU_ReleaseObject(part);
}

void Tcl_AppendObjToObj(Tcl_Obj *o,Tcl_Obj *part) {
    if(!o||o->refCount>1||!part) abort();
    int n; const char *s=Tcl_GetStringFromObj(part,&n);
    if(!s) abort();
    Tcl_AppendToObj(o,s,n);
}

int Tcl_AttemptSetObjLength(Tcl_Obj *o,int length) {
    if(!o||o->refCount>1||length<0) return 0;
    size_t n; const char *old=tg_object_bytes(o,&n);
    if(!old) return 0;
    char *next=malloc((size_t)length+1); if(!next) return 0;
    size_t keep=n<(size_t)length?n:(size_t)length;
    memcpy(next,old,keep);
    memset(next+keep,0,(size_t)length-keep+1);
    replace_string(o,next,length); return 1;
}
void Tcl_SetObjLength(Tcl_Obj *o,int length) {
    if(!Tcl_AttemptSetObjLength(o,length)) Tcl_Panic("invalid or failed object length change");
}

/* Collect arguments before mutating the object so strings borrowed from
   its current representation remain valid throughout the variadic call. */
void Tcl_AppendStringsToObjVA(Tcl_Obj *o,va_list source) {
    Tcl_DString d; Tcl_DStringInit(&d);
    va_list args; va_copy(args,source);
    const char *part;
    while((part=va_arg(args,const char *))!=NULL) Tcl_DStringAppend(&d,part,-1);
    va_end(args);
    Tcl_AppendToObj(o,d.string,d.length); Tcl_DStringFree(&d);
}

void Tcl_AppendStringsToObj(Tcl_Obj *o,...) {
    va_list args; va_start(args,o);
    Tcl_AppendStringsToObjVA(o,args); va_end(args);
}

/* Compute the reference's byte-limited prefix without probing beyond n.
   UtfPrev reproduces the Tcl 8.6 legacy prefix-boundary behavior. */
static int limited_prefix(const char *s,int n,int limit) {
    if(limit>=n) return n;
    if(limit<=0) return 0;
    const char *p=Tcl_UtfPrev(s+limit+1,s);
    return (int)(p-s);
}
void Tcl_AppendLimitedToObj(Tcl_Obj *o,const char *s,int n,int limit,const char *ellipsis) {
    if(!o||o->refCount>1||n< -1||(!s&&n!=0)) Tcl_Panic("invalid limited append");
    if(limit<=0||!s) return;
    if(n<0) {
        size_t length=strlen(s);
        if(length>INT_MAX) Tcl_Panic("limited append length overflow");
        n=(int)length;
    }
    if(n<=limit) { Tcl_AppendToObj(o,s,n); return; }
    if(!ellipsis) ellipsis="...";
    size_t length=strlen(ellipsis);
    if(length>INT_MAX) Tcl_Panic("ellipsis length overflow");
    int en=limited_prefix(ellipsis,(int)length,limit);
    int sn=limited_prefix(s,n,limit-en);
    /* Either argument may alias the old string representation. */
    Tcl_DString tmp; Tcl_DStringInit(&tmp);
    Tcl_DStringAppend(&tmp,s,sn);
    Tcl_DStringAppend(&tmp,ellipsis,en);
    Tcl_AppendToObj(o,tmp.string,tmp.length);
    Tcl_DStringFree(&tmp);
}
