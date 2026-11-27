#include "TKGPUCompatBignum.H"
#include <string.h>
static void zero(mp_int *a) {
    if(a->used) memset(a->dp,0,(size_t)a->used*sizeof *a->dp);
    a->used=0; a->sign=0;
}
static int digit(unsigned char c,int radix) {
    if(c>='0'&&c<='9') return c-'0';
    if(c>='A'&&c<='Z') return c-'A'+10;
    if(c>='a'&&c<='z') return c-'a'+(radix<=36?10:36);
    if(c=='+') return 62;
    if(c=='/') return 63;
    return -1;
}
int TclBN_mp_read_radix(mp_int *a,const char *s,int radix) {
    if(!tg_bn_valid(a)) return TKGPU_MP_VAL;
    zero(a);
    if(!s||radix<2||radix>64) return TKGPU_MP_VAL;
    int neg=*s=='-'; if(neg) s++;
    for(;*s&&*s!='\r'&&*s!='\n';s++) {
        int d=digit((unsigned char)*s,radix);
        if(d<0||d>=radix) { zero(a); return TKGPU_MP_VAL; }
        uint64_t carry=(unsigned)d;
        for(int k=0;k<a->used;k++) {
            uint64_t v=(uint64_t)a->dp[k]*(unsigned)radix+carry;
            a->dp[k]=(uint32_t)v&TG_BN_MASK; carry=v>>TG_BN_BITS;
        }
        if(carry) {
            int rc=tg_bn_reserve(a,a->used+1);
            if(rc) { zero(a); return rc; }
            a->dp[a->used++]=(uint32_t)carry;
        }
    }
    a->sign=a->used&&neg; return TKGPU_MP_OKAY;
}
