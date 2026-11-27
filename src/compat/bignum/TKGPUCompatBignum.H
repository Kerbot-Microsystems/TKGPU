#ifndef TKGPU_COMPAT_BIGNUM_H
#define TKGPU_COMPAT_BIGNUM_H
#include "TKGPUCompat.H"
enum { TG_BN_BITS=28,TG_BN_MAX=4096 };
#define TG_BN_MASK UINT32_C(0x0fffffff)
int tg_bn_valid(const mp_int *);
int tg_bn_reserve(mp_int *,int);
char *tg_bn_decimal(const mp_int *);
int tg_bn_read_tcl(mp_int *,const char *,size_t);
#endif
