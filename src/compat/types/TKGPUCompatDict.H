/* Ordered dictionary representation. Key comparisons are counted bytes. */
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#ifndef DICT_API
#define DICT_API(n) Tcl_##n
#endif
#ifndef DICT_REP
#define DICT_REP(o) ((o)->internalRep.pointers.first)
#endif
#ifndef DICT_CLEAR
#define DICT_CLEAR(o) do { tg_object_clear_type(o); tg_object_clear_caches(o); } while(0)
#endif
#ifndef DICT_TYPE
#define DICT_TYPE tg_type_dict
#endif
typedef struct { Tcl_Obj *key,*value; } dict_pair;
typedef struct { int count,capacity,refs; dict_pair *pairs; } dict_rep;
extern const Tcl_ObjType DICT_TYPE;
static int dict_error(Tcl_Interp *i,const char *s) {
    if(i) Tcl_SetObjResult(i,Tcl_NewStringObj(s,-1));
    return 1;
}
static dict_rep *dict_new(void) {
    dict_rep *r=calloc(1,sizeof *r); if(!r) Tcl_Panic("dictionary allocation failed"); r->refs=1;return r;
}
static void dict_release(dict_rep *r) {
    if(!r||--r->refs) return;
    for(int j=0;j<r->count;j++){Tcl_DecrRefCount(r->pairs[j].key);Tcl_DecrRefCount(r->pairs[j].value);}
    free(r->pairs);free(r);
}
static void dict_free(Tcl_Obj *o){dict_release(DICT_REP(o));DICT_REP(o)=NULL;}
static int dict_find(dict_rep *r,Tcl_Obj *key) {
    int n=0;const char *s=Tcl_GetStringFromObj(key,&n);if(!s)return -1;
    for(int j=0;j<r->count;j++){int m=0;const char *t=Tcl_GetStringFromObj(r->pairs[j].key,&m);if(t&&m==n&&!memcmp(s,t,(size_t)n))return j;}
    return -1;
}
static void dict_reserve(dict_rep *r) {
    if(r->count<r->capacity)return;
    if(r->capacity>INT_MAX/2 || (size_t)(r->capacity?r->capacity*2:8)>SIZE_MAX/sizeof(dict_pair))Tcl_Panic("dictionary capacity overflow");
    int cap=r->capacity?r->capacity*2:8;
    dict_pair *v=realloc(r->pairs,(size_t)cap*sizeof *v);if(!v)Tcl_Panic("dictionary allocation failed");r->pairs=v;r->capacity=cap;
}
static void dict_store(dict_rep *r,Tcl_Obj *key,Tcl_Obj *value) {
    int at=dict_find(r,key);Tcl_IncrRefCount(value);
    if(at>=0){Tcl_Obj *old=r->pairs[at].value;r->pairs[at].value=value;Tcl_DecrRefCount(old);return;}
    dict_reserve(r);Tcl_IncrRefCount(key);r->pairs[r->count++]=(dict_pair){key,value};
}
static void dict_dup(Tcl_Obj *src,Tcl_Obj *dst) {
    dict_rep *a=DICT_REP(src),*b=dict_new();
    for(int j=0;j<a->count;j++)dict_store(b,a->pairs[j].key,a->pairs[j].value);
    DICT_REP(dst)=b;dst->typePtr=&DICT_TYPE;
}
static void dict_update(Tcl_Obj *o) {
    dict_rep *r=DICT_REP(o);Tcl_DString d;Tcl_DStringInit(&d);
    for(int j=0;j<r->count;j++){
        Tcl_DStringAppendElement(&d,Tcl_GetString(r->pairs[j].key));
        Tcl_DStringAppendElement(&d,Tcl_GetString(r->pairs[j].value));
    }
    o->bytes=Tcl_Alloc((unsigned)d.length+1);memcpy(o->bytes,d.string,(size_t)d.length+1);o->length=d.length;Tcl_DStringFree(&d);
}
static int dict_convert(Tcl_Interp *i,Tcl_Obj *o) {
    if(!o)return dict_error(i,"invalid dictionary object");
    if(o->typePtr==&DICT_TYPE)return 0;
    int n;Tcl_Obj **v;
    if(Tcl_ListObjGetElements(i,o,&n,&v)!=0)return 1;
    if(n%2)return dict_error(i,"missing value to go with key");
    dict_rep *r=dict_new();for(int j=0;j<n;j+=2)dict_store(r,v[j],v[j+1]);
    DICT_CLEAR(o);DICT_REP(o)=r;o->typePtr=&DICT_TYPE;return 0;
}
const Tcl_ObjType DICT_TYPE={"dict",dict_free,dict_dup,dict_update,dict_convert};
Tcl_Obj *DICT_API(NewDictObj)(void){Tcl_Obj *o=Tcl_NewObj();if(!o)return NULL;DICT_REP(o)=dict_new();o->typePtr=&DICT_TYPE;return o;}
Tcl_Obj *DICT_API(DbNewDictObj)(const char *file,int line){(void)file;(void)line;return DICT_API(NewDictObj)();}
int DICT_API(DictObjSize)(Tcl_Interp *i,Tcl_Obj *o,int *size){if(!size||dict_convert(i,o))return 1;*size=((dict_rep *)DICT_REP(o))->count;return 0;}
int DICT_API(DictObjGet)(Tcl_Interp *i,Tcl_Obj *o,Tcl_Obj *key,Tcl_Obj **value){
    if(!key||!value||dict_convert(i,o))return 1;
    dict_rep *r=DICT_REP(o);int at=dict_find(r,key);*value=at<0?NULL:r->pairs[at].value;return 0;
}
static int dict_check_mutation(Tcl_Interp *i,Tcl_Obj *o){
    if(!o||o->refCount>1)return dict_error(i,"shared dictionary mutation");
    if(dict_convert(i,o))return 1;
    if(((dict_rep *)DICT_REP(o))->refs>1)return dict_error(i,"dictionary modified during search");
    return 0;
}
static int dict_valid_element(Tcl_Obj *o){int n=0;const char *s=Tcl_GetStringFromObj(o,&n);return s&&n>=0&&!memchr(s,0,(size_t)n);}
int DICT_API(DictObjPut)(Tcl_Interp *i,Tcl_Obj *o,Tcl_Obj *key,Tcl_Obj *value){
    if(!key||!value||o==key||o==value||!dict_valid_element(key)||!dict_valid_element(value))return dict_error(i,"unsupported dictionary element");
    if(dict_check_mutation(i,o))return 1;
    dict_store(DICT_REP(o),key,value);Tcl_InvalidateStringRep(o);return 0;
}
int DICT_API(DictObjRemove)(Tcl_Interp *i,Tcl_Obj *o,Tcl_Obj *key){
    if(!key||dict_check_mutation(i,o))return 1;
    dict_rep *r=DICT_REP(o);int at=dict_find(r,key);if(at<0)return 0;
    dict_pair old=r->pairs[at];memmove(r->pairs+at,r->pairs+at+1,(size_t)(--r->count-at)*sizeof *r->pairs);
    Tcl_InvalidateStringRep(o);Tcl_DecrRefCount(old.key);Tcl_DecrRefCount(old.value);return 0;
}
void DICT_API(DictObjDone)(Tcl_DictSearch *s){if(!s||s->epoch<0)return;dict_rep *r=(dict_rep *)s->dictionaryPtr;s->epoch=-1;s->next=NULL;s->dictionaryPtr=NULL;dict_release(r);}
void DICT_API(DictObjNext)(Tcl_DictSearch *s,Tcl_Obj **key,Tcl_Obj **value,int *done){
    dict_rep *r=s&&s->epoch>=0?(dict_rep *)s->dictionaryPtr:NULL;
    if(!r||s->epoch>=r->count){if(key)*key=NULL;if(value)*value=NULL;if(done)*done=1;DICT_API(DictObjDone)(s);return;}
    int at=s->epoch++;if(key)*key=r->pairs[at].key;if(value)*value=r->pairs[at].value;if(done)*done=0;
}
int DICT_API(DictObjFirst)(Tcl_Interp *i,Tcl_Obj *o,Tcl_DictSearch *s,Tcl_Obj **key,Tcl_Obj **value,int *done){
    if(!s)return 1;
    s->next=NULL;s->epoch=-1;s->dictionaryPtr=NULL;
    if(dict_convert(i,o))return 1;
    dict_rep *r=DICT_REP(o);if(r->refs==INT_MAX)Tcl_Panic("dictionary search reference overflow");++r->refs;s->dictionaryPtr=(void *)r;s->epoch=0;
    DICT_API(DictObjNext)(s,key,value,done);return 0;
}
/* Nested key paths: prepare a detached child before publishing it in its parent.
   Missing intermediate keys are created only by put; remove reports an error. */
static int dict_missing_key(Tcl_Interp *i,Tcl_Obj *key) {
    if(i) {
        Tcl_DString d;Tcl_DStringInit(&d);
        Tcl_DStringAppend(&d,"key \"",-1);Tcl_DStringAppend(&d,Tcl_GetString(key),-1);
        Tcl_DStringAppend(&d,"\" not known in dictionary",-1);
        Tcl_SetObjResult(i,Tcl_NewStringObj(d.string,d.length));Tcl_DStringFree(&d);
    }
    return 1;
}
static int dict_path_edit(Tcl_Interp *i,Tcl_Obj *o,int n,Tcl_Obj *const keys[],Tcl_Obj *value,int put) {
    if(n==1) {
        int rc=put?DICT_API(DictObjPut)(i,o,keys[0],value):DICT_API(DictObjRemove)(i,o,keys[0]);
        if(!rc) Tcl_InvalidateStringRep(o);
        return rc;
    }
    Tcl_Obj *child=NULL;
    if(DICT_API(DictObjGet)(i,o,keys[0],&child)) return 1;
    if(!child&&!put)return dict_missing_key(i,keys[0]);
    Tcl_Obj *copy=child?Tcl_DuplicateObj(child):DICT_API(NewDictObj)();
    if(!copy)return dict_error(i,"dictionary path allocation failed");
    Tcl_IncrRefCount(copy);
    int rc=dict_path_edit(i,copy,n-1,keys+1,value,put);
    if(!rc)rc=DICT_API(DictObjPut)(i,o,keys[0],copy);
    Tcl_DecrRefCount(copy);return rc;
}
static int dict_path(Tcl_Interp *i,Tcl_Obj *o,int n,Tcl_Obj *const keys[],Tcl_Obj *value,int put) {
    if(n<=0||n>256||!keys||(put&&(!value||value==o)))return dict_error(i,"unsupported dictionary key path");
    for(int j=0;j<n;j++)if(!keys[j]||keys[j]==o||!dict_valid_element(keys[j]))return dict_error(i,"unsupported dictionary key path");
    if(dict_check_mutation(i,o))return 1;
    return dict_path_edit(i,o,n,keys,value,put);
}
int DICT_API(DictObjPutKeyList)(Tcl_Interp *i,Tcl_Obj *o,int n,Tcl_Obj *const keys[],Tcl_Obj *value) {return dict_path(i,o,n,keys,value,1);}
int DICT_API(DictObjRemoveKeyList)(Tcl_Interp *i,Tcl_Obj *o,int n,Tcl_Obj *const keys[]) {return dict_path(i,o,n,keys,NULL,0);}
#undef DICT_API
