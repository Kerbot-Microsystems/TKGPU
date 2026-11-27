#ifndef TKGPU_UTF8_H
#define TKGPU_UTF8_H
#include "TKGPURuntime.H"
/* Strict standard UTF-8, not Tcl 8.6 modified UTF-8. */
int tg_utf8_decode(const unsigned char *s,size_t length,uint32_t *scalar,size_t *used);
int tg_utf8_encode(uint32_t scalar,unsigned char *out,size_t capacity,size_t *used);
int tg_utf8_count(const unsigned char *s,size_t length,size_t *count);
/* Feed one UTF-16 unit: 1 scalar, 0 pending high surrogate, -1 invalid low. */
int tg_utf16_feed(uint16_t *pending,uint16_t unit,uint32_t *scalar);
#endif
