#ifndef TKGPU_CHECKED_H
#define TKGPU_CHECKED_H
#include "TKGPURuntime.H"
int tg_size_add(size_t a,size_t b,size_t *out);
int tg_size_mul(size_t a,size_t b,size_t *out);
int tg_size_grow(size_t current,size_t required,size_t limit,size_t *out);
#endif
