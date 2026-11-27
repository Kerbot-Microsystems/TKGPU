#include "TKGPUObjectInternal.H"
#include "TKGPUChecked.H"
#include <stdlib.h>
#include <limits.h>
#include <string.h>
tg_object *tg_object_string(const char *bytes,size_t length) {
    size_t capacity;
    if(length>INT_MAX||(!bytes&&length)||tg_size_add(length,1,&capacity)!=TG_OK) return NULL;
    tg_object *o=calloc(1,sizeof(*o));
    if(!o) return NULL;
    o->bytes=malloc(capacity);
    if(!o->bytes) { free(o); return NULL; }
    if(length) memcpy(o->bytes,bytes,length);
    o->bytes[length]=0; o->length=(int)length; o->refCount=1;
    return o;
}
tg_object *tg_object_retain(tg_object *o) {
    if(!o||o->refCount<0||o->refCount==INT_MAX) return NULL;
    o->refCount++; return o;
}
void tg_object_clear_type(tg_object *o) {
    if(!o||!o->typePtr) return;
    const Tcl_ObjType *type=o->typePtr;
    if(type->freeIntRepProc) type->freeIntRepProc(o);
    o->typePtr=NULL; memset(&o->internalRep,0,sizeof o->internalRep);
}
void tg_object_clear_caches(tg_object *o) {
    for(size_t k=0;k<o->list_count;k++) tg_object_release(o->list_items[k]);
    free(o->list_items); o->list_items=NULL; o->list_count=0; o->list_ready=0;
    free(o->binary); o->binary=NULL; o->binary_length=0;
    free(o->unicode); o->unicode=NULL; o->unicode_length=0;
}
void tg_object_destroy(tg_object *o) {
    if(!o) return;
    tg_object_clear_type(o); tg_object_clear_caches(o);
    free(o->bytes); free(o);
}
void tg_object_release(tg_object *o) {
    if(!o) return;
    if(o->refCount<=1) tg_object_destroy(o);
    else o->refCount--;
}
const char *tg_object_bytes(const tg_object *o,size_t *length) {
    if(length) *length=0;
    if(!o) return NULL;
    if(!o->bytes&&o->typePtr&&o->typePtr->updateStringProc)
        o->typePtr->updateStringProc((tg_object *)o);
    if(!o->bytes||o->length<0) return NULL;
    if(length) *length=(size_t)o->length;
    return o->bytes;
}
