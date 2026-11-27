#ifndef TKGPU_ABI_BIGNUM_H
#define TKGPU_ABI_BIGNUM_H
#include <stdint.h>
/* Only the Tcl 8.6 configuration with 28-bit digits in uint32_t
   storage is targeted here. Other digit configurations differ. */
typedef struct TKGPUAbiBignum {
    int used,alloc,sign;
    uint32_t *dp;
} mp_int;
enum { TKGPU_MP_OKAY=0,TKGPU_MP_MEM=-2,TKGPU_MP_VAL=-3 };
#endif
