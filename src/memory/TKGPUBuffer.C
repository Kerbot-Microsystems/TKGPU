#include "TKGPUBuffer.H"
#include "TKGPUChecked.H"
#include <stdlib.h>
#include <string.h>
void tg_buffer_init(tg_buffer *b) {
    if(b) { b->data=NULL; b->length=0; b->capacity=0; }
}
void tg_buffer_clear(tg_buffer *b) {
    if(b) { b->length=0; if(b->data) b->data[0]=0; }
}
void tg_buffer_destroy(tg_buffer *b) {
    if(b) { free(b->data); tg_buffer_init(b); }
}
int tg_buffer_reserve(tg_buffer *b,size_t length) {
    if(!b) return TG_ERROR;
    size_t required,capacity;
    if(tg_size_add(length,1,&required)!=TG_OK) return TG_ERROR;
    if(required<=b->capacity) return TG_OK;
    if(tg_size_grow(b->capacity,required,SIZE_MAX,&capacity)!=TG_OK) return TG_ERROR;
    unsigned char *p=realloc(b->data,capacity);
    if(!p) return TG_ERROR;
    b->data=p; b->capacity=capacity;
    b->data[b->length]=0; return TG_OK;
}
int tg_buffer_append(tg_buffer *b,const void *data,size_t length) {
    if(!b||(!data&&length)) return TG_ERROR;
    size_t total;
    if(tg_size_add(b->length,length,&total)!=TG_OK||
       tg_buffer_reserve(b,total)!=TG_OK) return TG_ERROR;
    if(length) memcpy(b->data+b->length,data,length);
    b->length=total; b->data[total]=0; return TG_OK;
}
