#include "TKGPUCompatBignum.H"
size_t TclBN_mp_unsigned_bin_size(const mp_int *a) {
    if(!tg_bn_valid(a)||!a->used) return 0;
    uint32_t top=a->dp[a->used-1]; size_t bits=(size_t)(a->used-1)*TG_BN_BITS;
    while(top) { bits++; top>>=1; }
    return (bits+7)/8;
}
int TclBN_mp_to_unsigned_bin_n(const mp_int *a,unsigned char *b,unsigned long *length) {
    if(!length||!tg_bn_valid(a)) return TKGPU_MP_VAL;
    size_t n=TclBN_mp_unsigned_bin_size(a);
    if(n>*length||(!b&&n)) return TKGPU_MP_VAL;
    for(size_t k=0;k<n;k++) {
        size_t bit=k*8,index=bit/TG_BN_BITS; unsigned shift=(unsigned)(bit%TG_BN_BITS);
        uint64_t v=a->dp[index];
        if(index+1<(size_t)a->used) v|=(uint64_t)a->dp[index+1]<<TG_BN_BITS;
        b[n-1-k]=(unsigned char)(v>>shift);
    }
    *length=(unsigned long)n; return TKGPU_MP_OKAY;
}
