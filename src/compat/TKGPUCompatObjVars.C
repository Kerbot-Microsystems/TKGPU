#include "TKGPUCompatVars.H"
#include <string.h>
/* Each variable owns its object. Plain string writes invalidate that cache;
   unrelated accesses do not. Borrowed objects must be retained across writes. */
static Tcl_Obj *variable_object(var *v) {
    if(!v->object) v->object=tg_object_string(v->value,strlen(v->value));
    return v->object;
}
Tcl_Obj *Tcl_GetVar2Ex(Tcl_Interp *i,const char *name,const char *part2,int flags) {
    char key[MAX_WORD];
    if(!tg_compat_var_key(i,name,part2,flags,key)) return NULL;
    size_t k=tg_var_find(i,key);
    if(k==i->count||i->vars[k].kind==TG_VAR_ARRAY) {
        if(flags&TKGPU_LEAVE_ERR_MSG) tg_set_result(i,"unknown variable",TG_ERROR);
        return NULL;
    }
    return variable_object(&i->vars[k]);
}
Tcl_Obj *Tcl_SetVar2Ex(Tcl_Interp *i,const char *name,const char *part2,Tcl_Obj *value,int flags) {
    char key[MAX_WORD];
    if(!tg_compat_var_key(i,name,part2,flags,key)) return NULL;
    const char *s=tg_object_bytes(value,NULL);
    if(!s) {
        if(flags&TKGPU_LEAVE_ERR_MSG) tg_set_result(i,"invalid variable object",TG_ERROR);
        return NULL;
    }
    var *stored=NULL;
    if(tg_compat_var_store(i,key,s,value,flags,&stored)!=TG_OK) return NULL;
    if(stored) return variable_object(stored);
    /* A write trace deleted the variable. Return an interpreter-owned empty
       object, not the released input or a pointer to a removed variable. */
    Tcl_Obj *empty=tg_object_string("",0);
    if(!empty) return NULL;
    tg_object_release(i->compat_variable); i->compat_variable=empty;
    return empty;
}

static int name_bytes(Tcl_Interp *i,Tcl_Obj *o,int flags,const char **out) {
    size_t n; const char *s=tg_object_bytes(o,&n);
    if(!s||n>=MAX_WORD||memchr(s,0,n)) {
        if(i&&(flags&TKGPU_LEAVE_ERR_MSG)) tg_set_result(i,"unsupported variable name object",TG_ERROR);
        return 0;
    }
    *out=s; return 1;
}
Tcl_Obj *Tcl_ObjGetVar2(Tcl_Interp *i,Tcl_Obj *part1,Tcl_Obj *part2,int flags) {
    const char *a,*b=NULL;
    if(!name_bytes(i,part1,flags,&a)||(part2&&!name_bytes(i,part2,flags,&b))) return NULL;
    return Tcl_GetVar2Ex(i,a,b,flags);
}
Tcl_Obj *Tcl_ObjSetVar2(Tcl_Interp *i,Tcl_Obj *part1,Tcl_Obj *part2,Tcl_Obj *value,int flags) {
    const char *a,*b=NULL;
    if(!name_bytes(i,part1,flags,&a)||(part2&&!name_bytes(i,part2,flags,&b))) return NULL;
    return Tcl_SetVar2Ex(i,a,b,value,flags);
}
