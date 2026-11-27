#include "TKGPUHost.H"
#include "TKGPUCompat.H"
#include "../../core/TKGPUCoreInternal.H"
#include "../../core/TKGPUList.H"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>

int TKGPU_HostInteger(const char *s,int64_t *out) {
    if(!s||!out) return TKGPU_HOST_ERROR;
    while(isspace((unsigned char)*s)) s++;
    const char *p=s;
    int negative=0;
    if(*p=='+'||*p=='-') negative=*p++=='-';
    int radix=10;
    if(p[0]=='0'&&(p[1]=='x'||p[1]=='X'||p[1]=='b'||p[1]=='B'||p[1]=='o'||p[1]=='O')) {
        char prefix=p[1];
        radix=prefix=='x'||prefix=='X'?16:prefix=='b'||prefix=='B'?2:8;
        p+=2;
    } else if(p[0]=='0'&&isdigit((unsigned char)p[1])) radix=8;
    uint64_t limit=(uint64_t)INT64_MAX+(negative?1u:0u),v=0;
    const char *first=p;
    for(;*p;p++) {
        unsigned char ch=(unsigned char)*p;
        unsigned d=ch>='0'&&ch<='9'?ch-'0':ch>='a'&&ch<='f'?ch-'a'+10:ch>='A'&&ch<='F'?ch-'A'+10:255;
        if(d>=(unsigned)radix) break;
        if(v>(limit-d)/(unsigned)radix) return TKGPU_HOST_ERROR;
        v=v*(unsigned)radix+d;
    }
    if(p==first) return TKGPU_HOST_ERROR;
    while(isspace((unsigned char)*p)) p++;
    if(*p) return TKGPU_HOST_ERROR;
    *out=negative?(v==(uint64_t)INT64_MAX+1u?INT64_MIN:-(int64_t)v):(int64_t)v;
    return TKGPU_HOST_OK;
}
int TKGPU_HostDouble(const char *s,double *out) { return Tcl_GetDouble(NULL,s,out); }
int TKGPU_HostBoolean(const char *s,int *out) { return Tcl_GetBoolean(NULL,s,out); }
int TKGPU_HostJoin(size_t n,const char *const a[],char **out) {
    if(!out||n>MAX_ARGS||(n&&!a)) return TKGPU_HOST_ERROR;
    *out=NULL;
    char buffer[MAX_WORD]="";
    for(size_t k=0;k<n;k++) {
        if(!a[k]||!tg_list_append(buffer,sizeof buffer,a[k])) return TKGPU_HOST_ERROR;
    }
    *out=tg_copy_string(buffer);
    return *out?TKGPU_HOST_OK:TKGPU_HOST_ERROR;
}
