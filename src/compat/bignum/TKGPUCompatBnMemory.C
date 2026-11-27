#include "TKGPUCompatBignum.H"
#include <stdlib.h>
#include <string.h>
int tg_bn_valid(const mp_int *a) {
    if(!a||a->used<0||a->alloc<0||a->used>a->alloc||a->alloc>TG_BN_MAX||
       (a->alloc&&!a->dp)||(a->sign!=0&&a->sign!=1)) return 0;
    if(a->used&&!a->dp[a->used-1]) return 0;
    for(int k=0;k<a->used;k++) if(a->dp[k]>TG_BN_MASK) return 0;
    return 1;
}
int tg_bn_reserve(mp_int *a,int n) {
    if(n<0||n>TG_BN_MAX) return TKGPU_MP_VAL;
    if(n<=a->alloc) return TKGPU_MP_OKAY;
    int cap=a->alloc?a->alloc:4;
    while(cap<n) cap*=2;
    uint32_t *p=realloc(a->dp,(size_t)cap*sizeof *p);
    if(!p) return TKGPU_MP_MEM;
    memset(p+a->alloc,0,(size_t)(cap-a->alloc)*sizeof *p);
    a->dp=p; a->alloc=cap; return TKGPU_MP_OKAY;
}
int TclBN_mp_init(mp_int *a) {
    if(!a) return TKGPU_MP_VAL;
    *a=(mp_int){0}; return tg_bn_reserve(a,4);
}
void TclBN_mp_clear(mp_int *a) {
    if(a) { free(a->dp); *a=(mp_int){0}; }
}
