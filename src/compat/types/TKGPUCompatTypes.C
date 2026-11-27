#include "TKGPUCompat.H"
#include "TKGPUCompatTypes.H"
#include "../../object/TKGPUObjectInternal.H"
#include "../../core/TKGPUCoreInternal.H"
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include "TKGPUChecked.H"
#include "../../platform/threads/TKGPUThreadNative.H"
/* Immutable-string preview. Registered external descriptors are borrowed
   for the process lifetime; modules must remain loaded while registered. */
static int as_integer(Tcl_Interp *i,Tcl_Obj *o) {
    long long n; return Tcl_GetWideIntFromObj(i,o,&n);
}
static int as_double(Tcl_Interp *i,Tcl_Obj *o) {
    double n; return Tcl_GetDoubleFromObj(i,o,&n);
}
static int as_list(Tcl_Interp *i,Tcl_Obj *o) {
    int n; return Tcl_ListObjLength(i,o,&n);
}
static int as_bytes(Tcl_Interp *i,Tcl_Obj *o) {
    if(Tcl_GetByteArrayFromObj(o,NULL)) return TG_OK;
    if(i) tg_set_result(i,"invalid bytearray conversion",TG_ERROR);
    return TG_ERROR;
}
static int as_bignum(Tcl_Interp *i,Tcl_Obj *o) {
    mp_int value;
    int rc=Tcl_GetBignumFromObj(i,o,&value);
    if(rc==TG_OK) { TclBN_mp_clear(&value); tg_object_clear_type(o); o->typePtr=&tg_type_bignum; }
    return rc;
}
static void update_scalar(Tcl_Obj *o) {
    char b[96]; int n;
    if(o->typePtr==&tg_type_int) n=snprintf(b,sizeof b,"%ld",o->internalRep.integer);
    else if(o->typePtr==&tg_type_wide) n=snprintf(b,sizeof b,"%" PRId64,o->internalRep.wide);
    else {
        n=snprintf(b,sizeof b,"%.17g",o->internalRep.real);
        if(n>=0&&n<(int)sizeof b-2&&!strpbrk(b,".eE")) { b[n++]='.'; b[n++]='0'; b[n]=0; }
    }
    if(n<0||n>=(int)sizeof b) return;
    char *next=malloc((size_t)n+1); if(!next) return;
    memcpy(next,b,(size_t)n+1); o->bytes=next; o->length=n;
}
const Tcl_ObjType tg_type_int={"int",NULL,NULL,update_scalar,as_integer};
const Tcl_ObjType tg_type_wide={"wideInt",NULL,NULL,update_scalar,as_integer};
const Tcl_ObjType tg_type_double={"double",NULL,NULL,update_scalar,as_double};
const Tcl_ObjType tg_type_list={"list",NULL,NULL,NULL,as_list};
const Tcl_ObjType tg_type_bignum={"bignum",NULL,NULL,NULL,as_bignum};
const Tcl_ObjType tg_type_bytes={"bytearray",NULL,NULL,tg_bytearray_update_string,as_bytes};
static const Tcl_ObjType *builtin(const char *name) {
    if(!name) return NULL;
    const Tcl_ObjType *types[]={&tg_type_int,&tg_type_double,&tg_type_list,&tg_type_bytes,&tg_type_bignum,&tg_type_dict};
    for(size_t k=0;k<sizeof(types)/sizeof(types[0]);k++)
        if(!strcmp(name,types[k]->name)) return types[k];
#if LONG_MAX < INT64_MAX
    if(!strcmp(name,"wideInt")) return &tg_type_wide;
#endif
    return NULL;
}

enum { CUSTOM_TYPES=256 };
static const Tcl_ObjType *registered[CUSTOM_TYPES];
static size_t registered_count;
const Tcl_ObjType *Tcl_GetObjType(const char *name) {
    const Tcl_ObjType *found=builtin(name);
    if(found||!name) return found;
    tg_native_guard_lock();
    for(size_t k=0;k<registered_count;k++) {
        if(!strcmp(registered[k]->name,name)) { found=registered[k]; break; }
    }
    tg_native_guard_unlock(); return found;
}
void Tcl_RegisterObjType(const Tcl_ObjType *type) {
    if(!type||!type->name||!*type->name||strlen(type->name)>127||
       builtin(type->name)||!type->setFromAnyProc) return;
    tg_native_guard_lock();
    size_t k=0;
    while(k<registered_count&&strcmp(registered[k]->name,type->name)) k++;
    if(k==registered_count&&k<CUSTOM_TYPES) registered[registered_count++]=type;
    tg_native_guard_unlock();
}
int Tcl_ConvertToType(Tcl_Interp *i,Tcl_Obj *o,const Tcl_ObjType *type) {
    if(!o||!type||!type->name||Tcl_GetObjType(type->name)!=type||!type->setFromAnyProc)
        return i?tg_set_result(i,"unregistered or unsupported object type",TG_ERROR):TG_ERROR;
    if(o->typePtr==type) return TG_OK;
    /* An external conversion hook is trusted native code. It must preserve
       the existing string and release the old representation on success,
       as required by the documented Tcl setFromAnyProc contract. */
    return type->setFromAnyProc(i,o);
}
Tcl_Obj *Tcl_DuplicateObj(Tcl_Obj *o) {
    size_t n; const char *s=tg_object_bytes(o,&n);
    if(!s) return NULL;
    Tcl_Obj *copy=tg_object_string(s,n);
    if(!copy) return NULL;
    const Tcl_ObjType *type=o->typePtr;
    if(type&&type->dupIntRepProc) type->dupIntRepProc(o,copy);
    else if(type==&tg_type_int||type==&tg_type_wide||type==&tg_type_double) {
        copy->typePtr=type; copy->internalRep=o->internalRep;
    } else if(type==&tg_type_list&&o->list_ready) {
        size_t bytes;
        if(tg_size_mul(o->list_count?o->list_count:1,sizeof *copy->list_items,&bytes)!=TG_OK) goto fail;
        copy->list_items=calloc(1,bytes); if(!copy->list_items) goto fail;
        for(size_t k=0;k<o->list_count;k++) {
            tg_object *item=tg_object_retain(o->list_items[k]); if(!item) goto fail;
            copy->list_items[copy->list_count++]=item;
        }
        copy->list_ready=1; copy->typePtr=type;
    } else if(type==&tg_type_bytes&&o->binary) {
        copy->binary=malloc(o->binary_length?o->binary_length:1);
        if(!copy->binary) goto fail;
        memcpy(copy->binary,o->binary,o->binary_length);
        copy->binary_length=o->binary_length; copy->typePtr=type;
    } else if(type==&tg_type_bignum) copy->typePtr=type;
    return tg_compat_new_object(copy);
fail:
    tg_object_release(copy); return NULL;
}

/* Snapshot borrowed type descriptors under the registry lock; perform
   allocation and list mutation outside it, where custom hooks may run. */
int Tcl_AppendAllObjTypes(Tcl_Interp *i,Tcl_Obj *list) {
    const Tcl_ObjType *types[CUSTOM_TYPES+7]={&tg_type_int,&tg_type_double,
        &tg_type_list,&tg_type_bytes,&tg_type_bignum,&tg_type_dict};
    size_t count=6;
#if LONG_MAX < INT64_MAX
    types[count++]=&tg_type_wide;
#endif
    tg_native_guard_lock();
    for(size_t k=0;k<registered_count;k++) types[count++]=registered[k];
    tg_native_guard_unlock();
    Tcl_Obj *names[CUSTOM_TYPES+7]={0};
    int rc=TG_ERROR;
    if(!list||list->refCount>1) goto done;
    for(size_t k=0;k<count;k++) {
        names[k]=Tcl_NewStringObj(types[k]->name,-1);
        if(!names[k]||!TKGPU_RetainObject(names[k])) goto done;
    }
    rc=Tcl_ListObjReplace(i,list,INT_MAX,0,(int)count,names);
done:
    for(size_t k=0;k<count;k++) TKGPU_ReleaseObject(names[k]);
    if(rc!=TG_OK&&i) tg_set_result(i,"cannot append registered object types",TG_ERROR);
    return rc;
}
