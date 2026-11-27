#include "TKGPUCompat.H"
#include "../../core/TKGPUCoreInternal.H"
#include <stdlib.h>
#include <string.h>
#define NS_GLOBAL(i) ((i)->compat_global_namespace)
#define NS_EXPORTS(i) ((i)->compat_namespace_exports)
#define Tcl_IncrRefCount(o) do {if(!tg_object_retain(o))Tcl_Panic("namespace reference overflow");}while(0)
#define Tcl_DecrRefCount(o) tg_object_release(o)
#include "TKGPUCompatNamespace.H"
#undef NS_GLOBAL
#undef NS_EXPORTS
#undef Tcl_IncrRefCount
#undef Tcl_DecrRefCount
struct TKGPUOpaqueCommand {
    Tcl_ObjCmdProc *proc; Tcl_CmdDeleteProc *dispose; void *obj_data,*string_data,*delete_data;
    Tcl_CmdProc *string_proc;
    unsigned active; int retired;
    Tcl_Namespace *namespace;
};
static int command_info_string(void *,Tcl_Interp *,int,const char *[]);
static int command_info_object(void *,Tcl_Interp *,int,Tcl_Obj *const []);
static void release_command(struct TKGPUOpaqueCommand *c) {
    if(c->dispose) c->dispose(c->delete_data);
    free(c);
}
static int invoke(void *p,tg_interp *i,size_t n,tg_object *const a[]) {
    struct TKGPUOpaqueCommand *c=p;
    c->active++; i->compat_callback_depth++;
    int rc=c->proc?c->proc(c->obj_data,i,(int)n,a):tg_set_result(i,"missing object callback",TG_ERROR);
    i->compat_callback_depth--; c->active--;
    if(c->retired&&!c->active) {
        int previous=i->disposing; i->disposing=1;
        release_command(c); i->disposing=previous;
    }
    return rc;
}
static int strings(void *p,tg_interp *i,size_t n,const char *const a[]) {
    tg_object *v[MAX_ARGS]={0}; int rc=TG_ERROR;
    for(size_t k=0;k<n;k++) {
        v[k]=tg_object_string(a[k],strlen(a[k]));
        if(!v[k]) { tg_set_result(i,"out of memory",TG_ERROR); goto done; }
    }
    rc=invoke(p,i,n,v);
done:
    for(size_t k=0;k<n;k++) tg_object_release(v[k]);
    return rc;
}
static void dispose(void *p) {
    struct TKGPUOpaqueCommand *c=p;
    c->retired=1;
    if(!c->active) release_command(c);
}
static Tcl_Command create_command(Tcl_Interp *i,const char *name,Tcl_ObjCmdProc *fn,Tcl_CmdProc *string_fn,void *data,Tcl_CmdDeleteProc *del) {
    if(!i||(i->depth&&!i->compat_callback_depth)||i->disposing||!name||!*name||strlen(name)>=MAX_WORD||(!fn&&!string_fn)||
       strstr(name,"::")) return NULL;
    struct TKGPUOpaqueCommand *c=malloc(sizeof *c);
    if(!c) return NULL;
    *c=(struct TKGPUOpaqueCommand){
        .proc=fn?fn:command_info_object,.obj_data=fn?data:c,
        .string_proc=string_fn?string_fn:command_info_string,.string_data=string_fn?data:c,
        .dispose=del,.delete_data=data,.namespace=Tcl_GetGlobalNamespace(i)
    };
    size_t k=tg_command_lookup(i,name);
    if(k<i->command_count) {
        tg_registered_command *slot=&i->commands[k],old=*slot;
        slot->fn=strings; slot->context=c; slot->dispose=dispose; slot->object_fn=invoke;
        i->disposing=1;
        if(old.dispose) old.dispose(old.context);
        i->disposing=0; return c;
    }
    if(tg_command_register(i,name,strings,c,dispose)!=TG_OK) { free(c); return NULL; }
    i->commands[i->command_count-1].object_fn=invoke;
    return c;
}
Tcl_Command Tcl_CreateObjCommand(Tcl_Interp *i,const char *name,Tcl_ObjCmdProc *fn,void *data,Tcl_CmdDeleteProc *del) {
    return create_command(i,name,fn,NULL,data,del);
}
Tcl_Command Tcl_CreateCommand(Tcl_Interp *i,const char *name,Tcl_CmdProc *fn,void *data,Tcl_CmdDeleteProc *del) {
    return create_command(i,name,NULL,fn,data,del);
}
int Tcl_DeleteCommand(Tcl_Interp *i,const char *name) {
    return tg_command_unregister(i,name)==TG_OK?0:-1;
}

/* Only facade-owned native commands have Tcl_Command tokens. Do not cast
   private host commands, widget IDs or arbitrary callback data to tokens. */
static size_t token_index(Tcl_Interp *i,Tcl_Command token) {
    if(!i) return 0;
    if(token) for(size_t k=0;k<i->command_count;k++)
        if(i->commands[k].object_fn==invoke&&i->commands[k].dispose==dispose&&
           i->commands[k].context==token) return k;
    return i->command_count;
}
const char *Tcl_GetCommandName(Tcl_Interp *i,Tcl_Command token) {
    size_t k=token_index(i,token);
    return i&&k<i->command_count?i->commands[k].name:"";
}
Tcl_Command Tcl_GetCommandFromObj(Tcl_Interp *i,Tcl_Obj *o) {
    if(!i||i->disposing) return NULL;
    size_t n; const char *name=tg_object_bytes(o,&n);
    if(!name||memchr(name,0,n)) return NULL;
    if(name[0]==':'&&name[1]==':') name+=2;
    size_t k=tg_command_lookup(i,name);
    if(k<i->command_count&&i->commands[k].object_fn==invoke&&i->commands[k].dispose==dispose)
        return i->commands[k].context;
    return NULL;
}
int Tcl_DeleteCommandFromToken(Tcl_Interp *i,Tcl_Command token) {
    size_t k=token_index(i,token);
    if(!i||k==i->command_count) return -1;
    return Tcl_DeleteCommand(i,i->commands[k].name);
}

void Tcl_GetCommandFullName(Tcl_Interp *i,Tcl_Command token,Tcl_Obj *out) {
    size_t k=token_index(i,token);
    if(!out||!i||k==i->command_count) return;
    /* All currently registered facade commands live in the global namespace. */
    Tcl_AppendToObj(out,"::",2);
    Tcl_AppendToObj(out,i->commands[k].name,-1);
}

/* Global-namespace native command inspection. */
Tcl_Command Tcl_FindCommand(Tcl_Interp *i,const char *name,Tcl_Namespace *ns,int flags) {
    if(!i||!name||i->disposing||i->deleted) return NULL;
    const char *key=name;
    int absolute=key[0]==':'&&key[1]==':';
    if(absolute) while(*key==':') ++key;
    Tcl_Command token=NULL;
    if((!ns||ns==Tcl_GetGlobalNamespace(i)||absolute||(flags&TKGPU_GLOBAL_ONLY))&&*key&&!strstr(key,"::")) {
        size_t k=tg_command_lookup(i,key);
        if(k<i->command_count&&i->commands[k].object_fn==invoke&&i->commands[k].dispose==dispose)
            token=i->commands[k].context;
    }
    if(!token&&(flags&TKGPU_LEAVE_ERR_MSG)) {
        Tcl_ResetResult(i); Tcl_AppendResult(i,"unknown command \"",name,"\"",(char *)NULL);
    }
    return token;
}
static int command_info_string(void *p,Tcl_Interp *i,int n,const char *a[]) {
    if(n<0||n>MAX_ARGS) return TG_ERROR;
    return strings(p,i,(size_t)n,a);
}
static int command_info_object(void *p,Tcl_Interp *i,int n,Tcl_Obj *const a[]) {
    struct TKGPUOpaqueCommand *c=p;
    if(!c||!i||n<0||n>MAX_ARGS||!c->string_proc||c->string_proc==command_info_string)
        return TG_ERROR;
    const char *args[MAX_ARGS+1];
    for(int k=0;k<n;k++) {
        args[k]=Tcl_GetString(a[k]);
        if(!args[k]) return tg_set_result(i,"invalid string command argument",TG_ERROR);
    }
    args[n]=NULL;
    c->active++; i->compat_callback_depth++;
    int rc=c->string_proc(c->string_data,i,n,args);
    i->compat_callback_depth--; c->active--;
    if(c->retired&&!c->active) {
        int previous=i->disposing; i->disposing=1;
        release_command(c); i->disposing=previous;
    }
    return rc;
}
#include "TKGPUCommandInfo.H"
int Tcl_GetCommandInfo(Tcl_Interp *i,const char *name,Tcl_CmdInfo *info) {
    return Tcl_GetCommandInfoFromToken(Tcl_FindCommand(i,name,NULL,0),info);
}
int Tcl_SetCommandInfo(Tcl_Interp *i,const char *name,const Tcl_CmdInfo *info) {
    return Tcl_SetCommandInfoFromToken(Tcl_FindCommand(i,name,NULL,0),info);
}
