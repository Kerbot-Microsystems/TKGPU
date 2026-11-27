#include "TKGPUCompat.H"
#include "../core/TKGPUList.H"
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Public Tcl 8.6 DString layout, independently implemented allocation.
   All capacity arithmetic includes the terminating byte. */
void Tcl_DStringInit(Tcl_DString *d) {
    d->string=d->staticSpace; d->length=0;
    d->spaceAvl=(int)sizeof d->staticSpace; d->string[0]=0;
}
void Tcl_DStringFree(Tcl_DString *d) {
    if(d->string!=d->staticSpace) Tcl_Free(d->string);
    Tcl_DStringInit(d);
}
static void reserve(Tcl_DString *d,int length) {
    if(length<0||length==INT_MAX) Tcl_Panic("DString length overflow");
    int needed=length+1;
    if(needed<=d->spaceAvl) return;
    int capacity=needed<=INT_MAX/2?needed*2:needed;
    char *next=Tcl_Alloc((unsigned)capacity);
    memcpy(next,d->string,(size_t)d->length+1);
    if(d->string!=d->staticSpace) Tcl_Free(d->string);
    d->string=next; d->spaceAvl=capacity;
}
char *Tcl_DStringAppend(Tcl_DString *d,const char *s,int length) {
    if(!s&&length!=0) Tcl_Panic("DString null append");
    size_t n=length<0?strlen(s):(size_t)length;
    if(n>(size_t)(INT_MAX-1-d->length)) Tcl_Panic("DString append overflow");
    uintptr_t base=(uintptr_t)d->string,src=(uintptr_t)s;
    int aliased=s&&src>=base&&src-base<=(uintptr_t)d->length;
    size_t offset=aliased?(size_t)(src-base):0;
    if(aliased&&n>(size_t)d->length+1-offset) Tcl_Panic("DString invalid alias");
    reserve(d,d->length+(int)n);
    if(aliased) s=d->string+offset;
    if(n) memmove(d->string+d->length,s,n);
    d->length+=(int)n; d->string[d->length]=0; return d->string;
}
void Tcl_DStringSetLength(Tcl_DString *d,int length) {
    if(length<0) length=0;
    reserve(d,length);
    if(length>d->length) memset(d->string+d->length,0,(size_t)(length-d->length));
    d->length=length; d->string[length]=0;
}
static int escaped(const char *s,int index) {
    int odd=0;
    while(index>0&&s[--index]=='\\') odd=!odd;
    return odd;
}
static int separator(const Tcl_DString *d) {
    int k=d->length-1;
    if(k<0) return 0;
    char ch=d->string[k];
    if(!escaped(d->string,k)&&(ch=='{'||ch==' '||ch=='\t'||ch=='\n'||ch=='\r'||ch=='\v'||ch=='\f')) return 0;
    return 1;
}
char *Tcl_DStringAppendElement(Tcl_DString *d,const char *element) {
    if(!element) Tcl_Panic("DString null element");
    size_t n=strlen(element);
    if(n>(size_t)(INT_MAX-3)/2) Tcl_Panic("DString element overflow");
    size_t capacity=n*2+3;
    char *quoted=Tcl_Alloc((unsigned)capacity); quoted[0]=0;
    if(!tg_list_append(quoted,capacity,element)) { Tcl_Free(quoted); Tcl_Panic("DString element encoding failed"); }
    int sep=separator(d);
    /* '#' only needs quoting in the first element of a list. The core
       formatter is conservative; omit these braces for plain later words. */
    const char *out=quoted;
    if(sep&&element[0]=='#'&&!strpbrk(element," \t\r\n\v\f{}[]$;\\\"")) out=element;
    /* quoted is independent of d; an aliased element must not survive growth. */
    if(out==element) {
        memcpy(quoted,element,n+1); out=quoted;
    }
    if(sep) Tcl_DStringAppend(d," ",1);
    Tcl_DStringAppend(d,out,-1); Tcl_Free(quoted); return d->string;
}
void Tcl_DStringStartSublist(Tcl_DString *d) {
    if(separator(d)) Tcl_DStringAppend(d," ",1);
    Tcl_DStringAppend(d,"{",1);
}
void Tcl_DStringEndSublist(Tcl_DString *d) { Tcl_DStringAppend(d,"}",1); }
void Tcl_DStringGetResult(Tcl_Interp *i,Tcl_DString *d) {
    Tcl_DStringInit(d);
    Tcl_Obj *o=Tcl_GetObjResult(i); int n=0;
    const char *s=Tcl_GetStringFromObj(o,&n);
    if(s) Tcl_DStringAppend(d,s,n);
    Tcl_ResetResult(i);
}
void Tcl_DStringResult(Tcl_Interp *i,Tcl_DString *d) {
    Tcl_Obj *o=Tcl_NewStringObj(d->string,d->length);
    if(!o) Tcl_Panic("DString result allocation failed");
    TKGPU_RetainObject(o); Tcl_SetObjResult(i,o); TKGPU_ReleaseObject(o);
    Tcl_DStringFree(d);
}
