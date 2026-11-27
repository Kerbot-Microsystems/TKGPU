#include "TKGPUCompat.H"
#include "../object/TKGPUObjectInternal.H"
#include "TKGPUChecked.H"
#include "TKGPUUtf8.H"
#include "types/TKGPUCompatTypes.H"
#include <stdlib.h>
#include <limits.h>
#include <string.h>
/* Preview representation: raw bytes plus their standard UTF-8 Latin-1
   string representation. Tcl 8.6 modified UTF-8 is not yet supported. */
Tcl_Obj *Tcl_NewByteArrayObj(const unsigned char *bytes,int length) {
    if(length<0||(!bytes&&length)) return NULL;
    size_t cap;
    if(tg_size_mul((size_t)length,2,&cap)!=TG_OK||
       tg_size_add(cap,1,&cap)!=TG_OK) return NULL;
    unsigned char *text=malloc(cap);
    if(!text) return NULL;
    size_t used=0;
    for(int k=0;k<length;k++) {
        size_t n;
        if(tg_utf8_encode(bytes[k],text+used,cap-used,&n)!=TG_OK) { free(text); return NULL; }
        used+=n;
    }
    Tcl_Obj *o=tg_object_string((const char *)text,used); free(text);
    if(!o) return NULL;
    o->binary=malloc(length?(size_t)length:1);
    if(!o->binary) { tg_object_release(o); return NULL; }
    for(int k=0;k<length;k++) o->binary[k]=bytes[k];
    o->binary_length=(size_t)length;
    o->typePtr=&tg_type_bytes;
    return tg_compat_new_object(o);
}
unsigned char *Tcl_GetByteArrayFromObj(Tcl_Obj *o,int *length) {
    if(!o) return NULL;
    if(!o->binary) {
        if(!tg_object_bytes(o,NULL)) return NULL;
        /* Standard UTF-8 strings: one low byte per BMP scalar.
           Reject non-BMP instead of misrepresenting Tcl 8.6 semantics. */
        unsigned char *bytes=malloc(o->length?(size_t)o->length:1);
        if(!bytes) return NULL;
        size_t pos=0,n=0;
        while(pos<(size_t)o->length) {
            uint32_t v; size_t width;
            if(tg_utf8_decode((const unsigned char *)o->bytes+pos,(size_t)o->length-pos,&v,&width)!=TG_OK||
               v>0xffff) { free(bytes); return NULL; }
            bytes[n++]=(unsigned char)(v&255u); pos+=width;
        }
        o->binary=bytes; o->binary_length=n;
    }
    if(o->binary_length>INT_MAX) return NULL;
    if(o->typePtr!=&tg_type_bytes) { tg_object_clear_type(o); o->typePtr=&tg_type_bytes; }
    if(length) *length=(int)o->binary_length;
    return o->binary;
}

/* Keep the original standard UTF-8 bytearray string contract. The mutable
   binary representation may generate a fresh string after direct writes. */
void tg_bytearray_update_string(Tcl_Obj *o) {
    if(!o->binary) return;
    size_t cap;
    if(tg_size_mul(o->binary_length,2,&cap)!=TG_OK||
       tg_size_add(cap,1,&cap)!=TG_OK||cap>(size_t)INT_MAX+1u)
        Tcl_Panic("bytearray string overflow");
    unsigned char *text=malloc(cap); if(!text) Tcl_Panic("bytearray string allocation failed");
    size_t used=0;
    for(size_t k=0;k<o->binary_length;k++) {
        size_t n;
        if(tg_utf8_encode(o->binary[k],text+used,cap-used,&n)!=TG_OK) {
            free(text); Tcl_Panic("bytearray encoding failed");
        }
        used+=n;
    }
    text[used]=0; o->bytes=(char *)text; o->length=(int)used;
}
unsigned char *Tcl_SetByteArrayLength(Tcl_Obj *o,int length) {
    if(!o||o->refCount>1||length<0||length>INT_MAX/2)
        Tcl_Panic("invalid bytearray length");
    int old_n=0; const unsigned char *old=Tcl_GetByteArrayFromObj(o,&old_n);
    if(!old) Tcl_Panic("invalid bytearray conversion");
    unsigned char *next=calloc(length?(size_t)length:1,1);
    if(!next) Tcl_Panic("bytearray allocation failed");
    size_t keep=(size_t)(length<old_n?length:old_n);
    memcpy(next,old,keep);
    tg_object_clear_type(o); tg_object_clear_caches(o);
    free(o->bytes); o->bytes=NULL; o->length=0;
    o->binary=next; o->binary_length=(size_t)length; o->typePtr=&tg_type_bytes;
    return next;
}
