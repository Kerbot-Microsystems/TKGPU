#include "TKGPUCompat.H"
#include "TKGPUCompatVars.H"
#include "../core/TKGPUCoreInternal.H"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define VARIABLE_FLAGS (TKGPU_GLOBAL_ONLY | TKGPU_NAMESPACE_ONLY | TKGPU_APPEND_VALUE | TKGPU_LIST_ELEMENT | TKGPU_PARSE_PART1 | TKGPU_LEAVE_ERR_MSG)
/* Preserve both the bounded core result and the public owned/string/object
   views while variable traces execute. Detach legacy ownership temporarily. */
typedef struct {
    char *core;
    char *legacy;
    void (*dispose)(char *);
    tg_object *object;
    int kind,legacy_lookup;
} variable_result;
static void save_result(Tcl_Interp *i,variable_result *saved) {
    saved->core=tg_copy_string(i->result);
    saved->legacy=i->legacy_result; saved->dispose=i->legacy_free;
    saved->object=tg_object_retain(i->compat_result);
    saved->kind=i->result_kind; saved->legacy_lookup=i->compat_legacy_lookup;
    i->legacy_free=NULL;
}
static void finish_result(Tcl_Interp *i,variable_result *saved,int restore) {
    if(restore) {
        tg_set_result(i,saved->core?saved->core:"",TG_OK);
        i->result_kind=saved->kind;
        i->compat_result=saved->object; saved->object=NULL;
        i->legacy_result=saved->legacy; i->legacy_free=saved->dispose;
        i->compat_legacy_lookup=saved->legacy_lookup;
    } else if(i->legacy_result==saved->legacy&&!i->legacy_free) {
        i->legacy_free=saved->dispose;
    } else if(saved->legacy&&saved->dispose) {
        ++i->assoc_callback_depth;
        if((uintptr_t)saved->dispose==3u) Tcl_Free(saved->legacy);
        else if((uintptr_t)saved->dispose!=1u) saved->dispose(saved->legacy);
        --i->assoc_callback_depth;
    }
    free(saved->core); saved->core=NULL;
    tg_object_release(saved->object);
}

/* Flat global namespace plus bounded literal array element keys.
   The core enforces scalar/array collisions for the same base name. */
int tg_compat_var_key(Tcl_Interp *i,const char *name,const char *part2,int flags,char *key) {
    if(!i||i->disposing) return 0;
    while(name&&name[0]==':'&&name[1]==':') name+=2;
    if(!name||!*name||(flags&~VARIABLE_FLAGS)||
       (part2&&(strchr(name,'(')||strchr(name,')')))) goto fail;
    size_t len=strlen(name);
    if(len>=MAX_WORD) goto fail;
    if(!part2) { memcpy(key,name,len+1); return 1; }
    size_t part=strlen(part2);
    if(len>MAX_WORD-3||part>MAX_WORD-3-len) goto fail;
    memcpy(key,name,len); key[len]='(';
    memcpy(key+len+1,part2,part); key[len+1+part]=')'; key[len+2+part]=0;
    return 1;
fail:
    if(flags&TKGPU_LEAVE_ERR_MSG) tg_set_result(i,"unsupported variable name or flags",TG_ERROR);
    return 0;
}
const char *Tcl_GetVar2(Tcl_Interp *i,const char *name,const char *part2,int flags) {
    char key[MAX_WORD]; if(!tg_compat_var_key(i,name,part2,flags,key)) return NULL;
    size_t k=tg_var_find(i,key);
    if(k<i->count&&i->vars[k].kind!=TG_VAR_ARRAY) return i->vars[k].object?Tcl_GetString(i->vars[k].object):i->vars[k].value;
    if(flags&TKGPU_LEAVE_ERR_MSG) tg_set_result(i,"unknown variable",TG_ERROR);
    return NULL;
}
static Tcl_Obj *append_value(Tcl_Interp *i,const char *key,const char *value,Tcl_Obj *object,int flags) {
    Tcl_Obj *incoming=object?object:Tcl_NewStringObj(value,-1);
    if(!incoming||!TKGPU_RetainObject(incoming)) return NULL;
    size_t at=tg_var_find(i,key);
    Tcl_Obj *old=at<i->count?Tcl_GetVar2Ex(i,key,NULL,0):NULL;
    Tcl_Obj *next=NULL;
    if(flags&TKGPU_LIST_ELEMENT) {
        next=old?Tcl_DuplicateObj(old):Tcl_NewListObj(0,NULL);
        if(next&&Tcl_ListObjAppendElement(i,next,incoming)!=TG_OK) {
            TKGPU_ReleaseObject(next); next=NULL;
        }
    } else if(old) {
        next=Tcl_DuplicateObj(old);
        if(next) {
            int length=0; const char *text=Tcl_GetStringFromObj(incoming,&length);
            Tcl_AppendToObj(next,text,length);
        }
    } else {
        /* Transfer this temporary reference; a new variable can own the
           original object, preserving its type and pointer identity. */
        return incoming;
    }
    if(next) TKGPU_RetainObject(next);
    TKGPU_ReleaseObject(incoming);
    return next;
}
int tg_compat_var_store(Tcl_Interp *i,const char *key,const char *value,tg_object *object,int flags,var **out) {
    *out=NULL;
    if(!value) return TG_ERROR;
    variable_result saved; save_result(i,&saved);
    Tcl_Obj *prepared=NULL;
    int rc=TG_ERROR;
    if((flags&TKGPU_LIST_ELEMENT)&&!(flags&TKGPU_APPEND_VALUE)) {
        if(flags&TKGPU_LEAVE_ERR_MSG) tg_set_result(i,"list element requires append value",TG_ERROR);
    } else if(flags&TKGPU_APPEND_VALUE) {
        prepared=append_value(i,key,value,object,flags);
        if(prepared) rc=tg_var_assign_object(i,key,Tcl_GetString(prepared),prepared);
    } else rc=tg_var_assign_object(i,key,value,object);
    finish_result(i,&saved,rc==TG_OK||!(flags&TKGPU_LEAVE_ERR_MSG));
    TKGPU_ReleaseObject(prepared);
    /* Result/temporary destructors can reenter variable operations too.
       Resolve the borrowed slot only after every callback has completed. */
    size_t k=tg_var_find(i,key);
    if(rc==TG_OK&&k<i->count) *out=&i->vars[k];
    return rc;
}
const char *Tcl_SetVar2(Tcl_Interp *i,const char *name,const char *part2,const char *value,int flags) {
    char key[MAX_WORD];
    if(!tg_compat_var_key(i,name,part2,flags,key)) return NULL;
    var *stored=NULL;
    if(tg_compat_var_store(i,key,value,NULL,flags,&stored)!=TG_OK) return NULL;
    return stored?(stored->object?Tcl_GetString(stored->object):stored->value):"";
}
int Tcl_UnsetVar2(Tcl_Interp *i,const char *name,const char *part2,int flags) {
    char key[MAX_WORD]; if(!tg_compat_var_key(i,name,part2,flags,key)) return TG_ERROR;
    variable_result saved; save_result(i,&saved);
    int rc=tg_var_unset(i,key);
    finish_result(i,&saved,rc==TG_OK||!(flags&TKGPU_LEAVE_ERR_MSG));
    return rc;
}

const char *Tcl_GetVar(Tcl_Interp *i,const char *name,int flags) { return Tcl_GetVar2(i,name,NULL,flags); }
const char *Tcl_SetVar(Tcl_Interp *i,const char *name,const char *value,int flags) { return Tcl_SetVar2(i,name,NULL,value,flags); }
int Tcl_UnsetVar(Tcl_Interp *i,const char *name,int flags) { return Tcl_UnsetVar2(i,name,NULL,flags); }
