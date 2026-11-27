#include "TKGPUChecked.H"
int tg_size_add(size_t a,size_t b,size_t *out) {
    if(!out||a>SIZE_MAX-b) return TG_ERROR;
    *out=a+b; return TG_OK;
}
int tg_size_mul(size_t a,size_t b,size_t *out) {
    if(!out||(b&&a>SIZE_MAX/b)) return TG_ERROR;
    *out=a*b; return TG_OK;
}
int tg_size_grow(size_t current,size_t required,size_t limit,size_t *out) {
    if(!out||required>limit||current>limit) return TG_ERROR;
    size_t n=current;
    if(n<required&&!n) n=limit<16?limit:16;
    while(n<required) {
        size_t delta=n/2+1;
        if(delta>limit-n) n=limit;
        else n+=delta;
    }
    *out=n; return TG_OK;
}
