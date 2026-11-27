#include "TKGPUCompatBignum.H"
#include <stdlib.h>
#include <string.h>
char *tg_bn_decimal(const mp_int *a) {
    if(!tg_bn_valid(a)) return NULL;
    /* 28 binary bits require at most 9 decimal digits. */
    size_t cap=(size_t)a->used*9+3;
    char *s=malloc(cap); if(!s) return NULL;
    if(!a->used) { strcpy(s,"0"); return s; }
    uint32_t *v=malloc((size_t)a->used*sizeof *v);
    if(!v) { free(s); return NULL; }
    memcpy(v,a->dp,(size_t)a->used*sizeof *v);
    int used=a->used; size_t n=0;
    while(used) {
        uint64_t rem=0;
        for(int k=used;k>0;k--) {
            uint64_t x=(rem<<TG_BN_BITS)|v[k-1];
            v[k-1]=(uint32_t)(x/10); rem=x%10;
        }
        s[n++]=(char)('0'+rem);
        while(used&&!v[used-1]) used--;
    }
    free(v);
    if(a->sign) s[n++]='-';
    for(size_t k=0;k<n/2;k++) { char c=s[k]; s[k]=s[n-1-k]; s[n-1-k]=c; }
    s[n]=0; return s;
}
