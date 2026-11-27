#include "TKGPUCompat.H"
#include "../object/TKGPUObjectInternal.H"
#include "../core/TKGPUList.H"
#include "../core/TKGPUCoreInternal.H"
#include "TKGPUChecked.H"
#include "types/TKGPUCompatTypes.H"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
Tcl_Obj *Tcl_NewListObj(int count,Tcl_Obj *const objects[]) {
    if(count<0||(count&&!objects)) return NULL;
    Tcl_DString text; Tcl_DStringInit(&text);
    for(int k=0;k<count;k++) {
        int length=0,flags=0;
        const char *value=Tcl_GetStringFromObj(objects[k],&length);
        if(!value) { Tcl_DStringFree(&text); return NULL; }
        int needed=Tcl_ScanCountedElement(value,length,&flags);
        if(needed<0||needed==INT_MAX) { Tcl_DStringFree(&text); return NULL; }
        char local[256],*quoted=needed<(int)sizeof local?local:malloc((size_t)needed+1);
        if(!quoted) { Tcl_DStringFree(&text); return NULL; }
        int written=Tcl_ConvertCountedElement(value,length,quoted,flags|(k?8:0));
        if(written<0) {
            if(quoted!=local) free(quoted);
            Tcl_DStringFree(&text); return NULL;
        }
        if(k) Tcl_DStringAppend(&text," ",1);
        Tcl_DStringAppend(&text,quoted,written);
        if(quoted!=local) free(quoted);
    }
    size_t bytes;
    if(tg_size_mul(count?(size_t)count:1,sizeof(Tcl_Obj *),&bytes)!=TG_OK) {
        Tcl_DStringFree(&text); return NULL;
    }
    Tcl_Obj *o=tg_object_string(text.string,(size_t)text.length);
    Tcl_DStringFree(&text);
    if(!o) return NULL;
    o->list_items=calloc(1,bytes);
    if(!o->list_items) { tg_object_release(o); return NULL; }
    /* Tcl lists own a reference to each element, including duplicate pointers.
       A list lookup returns the original borrowed object, not a parsed copy. */
    for(int k=0;k<count;k++) {
        Tcl_Obj *item=tg_object_retain(objects[k]);
        if(!item) { tg_object_release(o); return NULL; }
        o->list_items[o->list_count++]=item;
    }
    o->list_ready=1; o->typePtr=&tg_type_list;
    return tg_compat_new_object(o);
}
static int list_space(unsigned char c) {
    return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\v'||c=='\f';
}
static int list_item(const char *s,size_t length,size_t *cursor,Tcl_DString *out,const char **reason) {
    size_t at=*cursor;
    if(s[at]=='{') {
        size_t begin=++at,depth=1;
        while(at<length) {
            if(s[at]=='\\'&&at+1<length) { at+=2; continue; }
            if(s[at]=='{') ++depth;
            else if(s[at]=='}'&&!--depth) break;
            ++at;
        }
        if(depth) { *reason="unmatched open brace in list"; return TG_ERROR; }
        Tcl_DStringAppend(out,s+begin,(int)(at-begin));
        ++at;
        if(at<length&&!list_space((unsigned char)s[at])) {
            *reason="list element in braces followed by non-space"; return TG_ERROR;
        }
    } else {
        int quote=s[at]=='"';
        if(quote) ++at;
        while(at<length) {
            if(quote?s[at]=='"':list_space((unsigned char)s[at])) break;
            if(s[at]=='\\') {
                char escaped[8]; int read=0;
                int written=Tcl_UtfBackslash(s+at,&read,escaped);
                if(read<1||(size_t)read>length-at||written<1||written>(int)sizeof escaped) {
                    *reason="invalid list backslash escape"; return TG_ERROR;
                }
                Tcl_DStringAppend(out,escaped,written); at+=(size_t)read;
            } else {
                size_t begin=at++;
                while(at<length&&s[at]!='\\'&&!(quote?s[at]=='"':list_space((unsigned char)s[at]))) ++at;
                Tcl_DStringAppend(out,s+begin,(int)(at-begin));
            }
        }
        if(quote) {
            if(at==length) { *reason="unmatched open quote in list"; return TG_ERROR; }
            ++at;
            if(at<length&&!list_space((unsigned char)s[at])) {
                *reason="list element in quotes followed by non-space"; return TG_ERROR;
            }
        }
    }
    *cursor=at; return TG_OK;
}
static int prepare(Tcl_Interp *i,Tcl_Obj *o) {
    const char *reason="invalid list object";
    if(!o) goto fail;
    if(o->list_ready) { if(o->typePtr!=&tg_type_list) { tg_object_clear_type(o); o->typePtr=&tg_type_list; } return TG_OK; }
    size_t length=0;
    const char *text=tg_object_bytes(o,&length);
    if(!text) goto fail;
    size_t at=0,count=0,capacity=0;
    Tcl_Obj **items=NULL;
    while(at<length) {
        while(at<length&&list_space((unsigned char)text[at])) ++at;
        if(at==length) break;
        Tcl_DString item; Tcl_DStringInit(&item);
        int rc=list_item(text,length,&at,&item,&reason);
        if(rc!=TG_OK) { Tcl_DStringFree(&item); goto discard; }
        if(count==capacity) {
            size_t next=capacity?capacity*2:8;
            if(next<capacity||next>INT_MAX||next>SIZE_MAX/sizeof *items) {
                Tcl_DStringFree(&item); reason="list allocation overflow"; goto discard;
            }
            Tcl_Obj **grown=realloc(items,next*sizeof *items);
            if(!grown) { Tcl_DStringFree(&item); reason="list allocation failed"; goto discard; }
            items=grown; capacity=next;
        }
        items[count]=tg_object_string(item.string,(size_t)item.length);
        Tcl_DStringFree(&item);
        if(!items[count]) { reason="list allocation failed"; goto discard; }
        ++count;
    }
    tg_object_clear_type(o); tg_object_clear_caches(o);
    o->list_items=items; o->list_count=count; o->list_ready=1;
    o->typePtr=&tg_type_list;
    return TG_OK;
discard:
    for(size_t k=0;k<count;k++) tg_object_release(items[k]);
    free(items);
fail:
    if(i) tg_set_result(i,reason,TG_ERROR);
    return TG_ERROR;
}
int Tcl_ListObjGetElements(Tcl_Interp *i,Tcl_Obj *o,int *count,Tcl_Obj ***elements) {
    if(!count||!elements||prepare(i,o)!=TG_OK) return TG_ERROR;
    *count=(int)o->list_count; *elements=o->list_items; return TG_OK;
}
int Tcl_ListObjLength(Tcl_Interp *i,Tcl_Obj *o,int *count) {
    if(!count||prepare(i,o)!=TG_OK) return TG_ERROR;
    *count=(int)o->list_count; return TG_OK;
}
int Tcl_ListObjIndex(Tcl_Interp *i,Tcl_Obj *o,int index,Tcl_Obj **value) {
    if(!value||prepare(i,o)!=TG_OK) return TG_ERROR;
    *value=index<0||(size_t)index>=o->list_count?NULL:o->list_items[index];
    return TG_OK;
}

int Tcl_ListObjReplace(Tcl_Interp *i,Tcl_Obj *o,int first,int remove,int insert,Tcl_Obj *const elements[]) {
    if(!o||o->refCount>1||remove<0||insert<0||(insert&&!elements)||prepare(i,o)!=TG_OK) goto fail;
    if(first<0) first=0;
    size_t start=(size_t)first<o->list_count?(size_t)first:o->list_count;
    size_t deleted=(size_t)remove<o->list_count-start?(size_t)remove:o->list_count-start;
    for(int k=0;k<insert;k++) if(!elements[k]||elements[k]==o) goto fail;
    size_t count,bytes;
    if(tg_size_add(o->list_count-deleted,(size_t)insert,&count)!=TG_OK||count>INT_MAX||
       tg_size_mul(count?count:1,sizeof(Tcl_Obj *),&bytes)!=TG_OK) goto fail;
    Tcl_Obj **items=malloc(bytes); if(!items) goto fail;
    if(start) memcpy(items,o->list_items,start*sizeof *items);
    if(insert) memcpy(items+start,elements,(size_t)insert*sizeof *items);
    size_t tail=o->list_count-start-deleted;
    if(tail) memcpy(items+start+insert,o->list_items+start+deleted,tail*sizeof *items);
    /* Construct a complete replacement before releasing any old children. */
    Tcl_Obj *next=Tcl_NewListObj((int)count,items); free(items);
    if(!next) goto fail;
    tg_object_clear_type(o); tg_object_clear_caches(o); free(o->bytes);
    o->bytes=next->bytes; o->length=next->length;
    o->list_items=next->list_items; o->list_count=next->list_count; o->list_ready=1;
    o->typePtr=&tg_type_list;
    next->bytes=NULL; next->list_items=NULL; next->list_count=0;
    tg_object_release(next); return TG_OK;
fail:
    if(i) tg_set_result(i,"invalid or shared list mutation",TG_ERROR);
    return TG_ERROR;
}

int Tcl_ListObjAppendElement(Tcl_Interp *i,Tcl_Obj *o,Tcl_Obj *element) {
    return Tcl_ListObjReplace(i,o,INT_MAX,0,1,&element);
}
int Tcl_ListObjAppendList(Tcl_Interp *i,Tcl_Obj *o,Tcl_Obj *items) {
    if(!o||!items||o->refCount>1) return TG_ERROR;
    int count; Tcl_Obj **elements;
    if(Tcl_ListObjGetElements(i,items,&count,&elements)!=TG_OK) return TG_ERROR;
    return Tcl_ListObjReplace(i,o,INT_MAX,0,count,elements);
}
