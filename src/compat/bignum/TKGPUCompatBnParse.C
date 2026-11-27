#include "TKGPUCompatBignum.H"
#include <stdlib.h>
#include <string.h>
static int space(unsigned char c) {
    return c==' '||(c>='\t'&&c<='\r');
}
/* Tcl 8.6 integer syntax, bounded by the preview's limb/input limits. */
int tg_bn_read_tcl(mp_int *a,const char *s,size_t n) {
    if(!a||!s) return TKGPU_MP_VAL;
    const char *nul=memchr(s,0,n);
    if(nul) n=(size_t)(nul-s);
    if(n>1024u*1024u) return TKGPU_MP_VAL;
    while(n&&space((unsigned char)*s)) { s++; n--; }
    while(n&&space((unsigned char)s[n-1])) n--;
    int neg=0,radix=10;
    if(n&&(*s=='+'||*s=='-')) { neg=*s=='-'; s++; n--; }
    if(!n) return TKGPU_MP_VAL;
    if(n>1&&s[0]=='0') {
        switch(s[1]) {
        case 'x': case 'X': radix=16; s+=2; n-=2; break;
        case 'b': case 'B': radix=2; s+=2; n-=2; break;
        case 'o': case 'O': radix=8; s+=2; n-=2; break;
        default: radix=8; break;
        }
    }
    if(!n) return TKGPU_MP_VAL;
    for(size_t k=0;k<n;k++) {
        unsigned char c=(unsigned char)s[k]; int d;
        if(c>='0'&&c<='9') d=c-'0';
        else if(c>='a'&&c<='f') d=c-'a'+10;
        else if(c>='A'&&c<='F') d=c-'A'+10;
        else return TKGPU_MP_VAL;
        if(d>=radix) return TKGPU_MP_VAL;
    }
    char *digits=malloc(n+1);
    if(!digits) return TKGPU_MP_MEM;
    memcpy(digits,s,n); digits[n]=0;
    int rc=TclBN_mp_read_radix(a,digits,radix); free(digits);
    if(!rc&&a->used) a->sign=neg;
    return rc;
}
