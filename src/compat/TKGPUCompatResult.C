#include "TKGPUCompat.H"
#include "TKGPUObject.H"
#include "../core/TKGPUCoreInternal.H"
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
/* Borrowed result snapshot, valid until the next result lookup/change or
   interpreter destruction. Preview objects use TKGPU ownership rules. */
Tcl_Obj *Tcl_GetObjResult(Tcl_Interp *i) {
    /* CPython also asks for the error result when a finalizer attempts a
       rejected reentrant call during command/interpreter teardown. */
    if(!i) return NULL;
    const char *s=tg_result(i);
    if(i->compat_legacy_lookup) {
        size_t old_length=0;
        const char *old=tg_object_bytes(i->compat_result,&old_length);
        size_t current_length=strlen(s);
        if(old&&old_length==current_length&&!memcmp(old,s,current_length)) {
            i->compat_legacy_lookup=0;
            return i->compat_result;
        }
        /* A legacy C-string lookup exposes strlen semantics on conversion
           back to an object, including embedded NUL truncation in Tcl 8.6. */
        Tcl_Obj *next=tg_object_string(s,strlen(s));
        if(!next) return NULL;
        Tcl_SetObjResult(i,next); tg_object_release(next);
        return i->compat_result;
    }
    if(i->compat_result&&s==Tcl_GetString(i->compat_result)) return i->compat_result;
    size_t length=strlen(s),old_length=0;
    const char *old=tg_object_bytes(i->compat_result,&old_length);
    if(i->legacy_result==i->result&&old&&length==old_length&&!memcmp(s,old,length)) return i->compat_result;
    Tcl_Obj *next=tg_object_string(s,length);
    if(!next) return NULL;
    if(i->legacy_result!=i->result) {
        Tcl_SetObjResult(i,next);
        tg_object_release(next);
        return i->compat_result;
    }
    /* Materialize explicit core types without guessing numeric-looking text. */
    if(i->result_kind==TG_RESULT_INTEGER) {
        long long value; (void)Tcl_GetWideIntFromObj(NULL,next,&value);
    } else if(i->result_kind==TG_RESULT_DOUBLE) {
        double value; (void)Tcl_GetDoubleFromObj(NULL,next,&value);
    } else if(i->result_kind==TG_RESULT_LIST||i->result_kind==TG_RESULT_INT_LIST||i->result_kind==TG_RESULT_DOUBLE_LIST) {
        int count=0;
        Tcl_Obj **elems=NULL;
        if(Tcl_ListObjGetElements(NULL,next,&count,&elems)==TG_OK&&
           i->result_kind==TG_RESULT_LIST&&count==10&&
           !strcmp(Tcl_GetString(elems[0]),"-text")&&
           !strcmp(Tcl_GetString(elems[2]),"-values")&&
           !strcmp(Tcl_GetString(elems[4]),"-open")&&
           !strcmp(Tcl_GetString(elems[6]),"-tags")) {
            int sub=0;
            if(Tcl_GetString(elems[3])[0]) (void)Tcl_ListObjLength(NULL,elems[3],&sub);
            long long open_val=0; (void)Tcl_GetWideIntFromObj(NULL,elems[5],&open_val);
            if(Tcl_GetString(elems[7])[0]) (void)Tcl_ListObjLength(NULL,elems[7],&sub);
        }
    }
    tg_object_release(i->compat_result);
    i->compat_result=next;
    return next;
}
void Tcl_SetObjResult(Tcl_Interp *i,Tcl_Obj *o) {
    if(!i||i->disposing) return;
    /* Own the incoming object before replacing the previous result,
       including the case where both pointers are equal. */
    if(!tg_object_retain(o)) {
        tg_set_result(i,"invalid result object",TG_ERROR);
        return;
    }
    size_t length=0;
    const char *text=tg_object_bytes(o,&length);
    int extended=text&&(length>=MAX_WORD||memchr(text,0,length));
    if(extended) tg_set_result(i,"",TG_OK);
    else if(TKGPU_SetResultObject(i,o)!=TG_OK) {
        tg_object_release(o);
        tg_set_result(i,"unsupported result object",TG_ERROR);
        return;
    }
    tg_object_release(i->compat_result);
    i->compat_result=o;
    if(extended) i->legacy_result=(char *)text;
}

void Tcl_AppendResultVA(Tcl_Interp *i,va_list source) {
    if(!i||i->disposing) return;
    /* Copy arguments first: a suffix can alias an owned legacy result that
       is released when GetObjResult materializes its object representation. */
    Tcl_DString suffix; Tcl_DStringInit(&suffix);
    va_list args; va_copy(args,source);
    const char *part;
    while((part=va_arg(args,const char *))!=NULL) Tcl_DStringAppend(&suffix,part,-1);
    va_end(args);
    Tcl_Obj *old=Tcl_GetObjResult(i);
    if(!old) { Tcl_DStringFree(&suffix); return; }
    if(!suffix.length) { Tcl_DStringFree(&suffix); return; }
    int length=0;
    const char *prefix=Tcl_GetStringFromObj(old,&length);
    Tcl_DString joined; Tcl_DStringInit(&joined);
    Tcl_DStringAppend(&joined,prefix,length);
    Tcl_DStringAppend(&joined,suffix.string,suffix.length);
    Tcl_DStringFree(&suffix);
    Tcl_DStringResult(i,&joined);
}

void Tcl_AppendResult(Tcl_Interp *i,...) {
    va_list args; va_start(args,i);
    Tcl_AppendResultVA(i,args); va_end(args);
}

void Tcl_AppendElement(Tcl_Interp *i,const char *element) {
    if(!i||i->disposing||!element) return;
    char *copy=tg_copy_string(element);
    if(!copy) Tcl_Panic("cannot preserve aliased result element");
    Tcl_Obj *old=Tcl_GetObjResult(i);
    if(!old) { free(copy); return; }
    int length=0;
    const char *prefix=Tcl_GetStringFromObj(old,&length);
    Tcl_DString joined; Tcl_DStringInit(&joined);
    Tcl_DStringAppend(&joined,prefix,length);
    Tcl_DStringAppendElement(&joined,copy);
    free(copy);
    Tcl_DStringResult(i,&joined);
}
/* The facade publishes diagnostic variables immediately. Snapshot them
   without variable-read callbacks. Restore is best effort, respecting
   existing variable write/unset semantics. */
static Tcl_Obj *state_error(Tcl_Interp *i,const char *key) {
    size_t k=tg_var_find(i,key);
    if(k==i->count) return NULL;
    return i->vars[k].object?i->vars[k].object:Tcl_NewStringObj(i->vars[k].value,-1);
}
static void state_restore_error(Tcl_Interp *i,const char *key,Tcl_Obj *o) {
    if(i->deleted||i->disposing) return;
    if(o) (void)Tcl_SetVar2Ex(i,key,NULL,o,TKGPU_GLOBAL_ONLY);
    else if(tg_var_find(i,key)<i->count) (void)Tcl_UnsetVar2(i,key,NULL,TKGPU_GLOBAL_ONLY);
}
#define Tcl_IncrRefCount(o) do { if(!tg_object_retain(o)) Tcl_Panic("state reference overflow"); } while(0)
#define Tcl_DecrRefCount(o) tg_object_release(o)
#define CHANNEL_ERROR_FIELD(i) ((i)->compat_channel_error)
#include "interp/TKGPUCompatChannelError.H"
#undef CHANNEL_ERROR_FIELD
#define STATE_API(n) state_impl_##n
#define STATE_GET_ERROR(i,k) state_error(i,k)
#define STATE_RESTORE_ERROR(i,k,v) state_restore_error(i,k,v)
#include "interp/TKGPUCompatState.H"
#undef STATE_GET_ERROR
#undef STATE_RESTORE_ERROR
#undef Tcl_IncrRefCount
#undef Tcl_DecrRefCount
Tcl_InterpState Tcl_SaveInterpState(Tcl_Interp *i,int status) { return state_impl_SaveInterpState(i,status); }
void Tcl_DiscardInterpState(Tcl_InterpState s) { state_impl_DiscardInterpState(s); }
int Tcl_RestoreInterpState(Tcl_Interp *i,Tcl_InterpState s) {
    if(!i||i->deleted||i->disposing) { state_impl_DiscardInterpState(s); return TG_ERROR; }
#ifdef TKGPU_REFERENCE_STUBS
    tg_compat_lifetime_enter(i);
#endif
    int rc=state_impl_RestoreInterpState(i,s);
#ifdef TKGPU_REFERENCE_STUBS
    tg_compat_lifetime_leave(i);
#endif
    return rc;
}

/* Legacy string ownership remains lazy until object conversion/reset. */
void Tcl_SetResult(Tcl_Interp *i,char *text,Tcl_FreeProc *dispose) {
    if(!i||i->disposing) return;
    if(!text) { Tcl_ResetResult(i); return; }
    if((uintptr_t)dispose==1u) {
        Tcl_Obj *copy=Tcl_NewStringObj(text,-1);
        if(!copy) Tcl_Panic("cannot copy volatile result");
        Tcl_SetObjResult(i,copy);
        return;
    }
    if(text==i->legacy_result&&dispose==i->legacy_free) return;
    if(text==i->result) {
        Tcl_Obj *copy=Tcl_NewStringObj(text,-1);
        if(!copy) Tcl_Panic("cannot preserve internal result buffer");
        Tcl_SetObjResult(i,copy); return;
    }
    if(text==i->legacy_result&&(!i->compat_result||text!=Tcl_GetString(i->compat_result)))
        i->legacy_free=NULL; /* Transfer ownership of the same legacy buffer. */
    Tcl_ResetResult(i);
    i->legacy_result=text; i->legacy_free=dispose;
}
void Tcl_FreeResult(Tcl_Interp *i) {
    if(!i||!i->legacy_free) return;
    if(i->compat_result&&i->legacy_result==Tcl_GetString(i->compat_result)) return;
    /* Tcl_FreeResult releases the allocation, but does not reset the public
       legacy result pointer. Clients must replace/reset it before reuse. */
    char *legacy=i->legacy_result;
    tg_legacy_result_release(i);
    i->legacy_result=legacy;
}
void Tcl_TransferResult(Tcl_Interp *source,int code,Tcl_Interp *target) {
    if(!source||!target||source==target) return;
    Tcl_Obj *result=Tcl_GetObjResult(source);
    if(!tg_object_retain(result)) return;
    if(code==TG_ERROR) {
        const char *keys[]={"errorInfo","errorCode"};
        for(int k=0;k<2;k++) {
            size_t at=tg_var_find(source,keys[k]);
            if(at<source->count) Tcl_SetVar2(target,keys[k],NULL,source->vars[at].value,0);
        }
        target->legacy_error_line=source->legacy_error_line;
    }
    Tcl_SetObjResult(target,result);
    Tcl_ResetResult(source);
    tg_object_release(result);
}
