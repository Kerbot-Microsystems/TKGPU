#ifndef TKGPU_BUFFER_H
#define TKGPU_BUFFER_H
#include "TKGPURuntime.H"
/* Owned byte buffer. Always NUL terminated when allocated.
   Append source must not overlap the buffer. */
typedef struct { unsigned char *data; size_t length,capacity; } tg_buffer;
void tg_buffer_init(tg_buffer *b);
void tg_buffer_clear(tg_buffer *b);
void tg_buffer_destroy(tg_buffer *b);
int tg_buffer_reserve(tg_buffer *b,size_t length);
int tg_buffer_append(tg_buffer *b,const void *data,size_t length);
#endif
