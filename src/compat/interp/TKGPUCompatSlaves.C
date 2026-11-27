#include "TKGPUCompat.H"
#include "../../core/TKGPUCoreInternal.H"
#include <stdlib.h>
#include <string.h>

typedef struct slave_link {
    char *name;
    Tcl_Interp *child;
    struct slave_link *next;
} slave_link;

typedef struct hidden_cmd {
    char *token;
    Tcl_CmdInfo info;
    struct hidden_cmd *next;
} hidden_cmd;

typedef struct alias_entry {
    char *child_cmd;
    Tcl_Interp *target;
    char *target_cmd;
    int objc;
    Tcl_Obj **objv;
    const char **argv;
    struct alias_entry *next;
} alias_entry;

struct Tcl_Trace_ {
    int level;
    int flags;
    int is_obj;
    Tcl_CmdTraceProc *str_proc;
    Tcl_CmdObjTraceProc *obj_proc;
    Tcl_CmdObjTraceDeleteProc *del_proc;
    void *data;
    struct Tcl_Trace_ *next;
};

typedef struct {
    Tcl_Interp *master;
    char *name_in_master;
    int safe;
    slave_link *slaves;
    hidden_cmd *hidden;
    alias_entry *aliases;
    struct Tcl_Trace_ *traces;
} interp_ext;

static const char ext_key[]="TKGPU::interp-ext";

static char *dup_cstr(const char *s) {
    if(!s) return NULL;
    size_t n=strlen(s);
    char *p=malloc(n+1);
    if(!p) Tcl_Panic("cannot copy string");
    memcpy(p,s,n+1);
    return p;
}

static interp_ext *get_ext(Tcl_Interp *i,int create);

static void free_alias(alias_entry *a) {
    if(!a) return;
    for(int k=0;k<a->objc;k++) {
        if(a->objv[k]) TKGPU_ReleaseObject(a->objv[k]);
    }
    free(a->objv);
    free(a->argv);
    free(a->child_cmd);
    free(a->target_cmd);
    free(a);
}

static void dispose_ext(void *data,Tcl_Interp *i) {
    interp_ext *e=data;
    if(e->master) {
        interp_ext *me=get_ext(e->master,0);
        if(me) {
            slave_link **link=&me->slaves;
            while(*link) {
                if((*link)->child==i) {
                    slave_link *dead=*link;
                    *link=dead->next;
                    free(dead->name);
                    free(dead);
                    break;
                }
                link=&(*link)->next;
            }
        }
    }
    while(e->slaves) {
        slave_link *s=e->slaves;
        e->slaves=s->next;
        Tcl_Interp *child=s->child;
        free(s->name);
        free(s);
        if(child&&!Tcl_InterpDeleted(child)) {
            interp_ext *ce=get_ext(child,0);
            if(ce) ce->master=NULL;
            Tcl_DeleteInterp(child);
        }
    }
    while(e->hidden) {
        hidden_cmd *h=e->hidden;
        e->hidden=h->next;
        free(h->token);
        free(h);
    }
    while(e->aliases) {
        alias_entry *a=e->aliases;
        e->aliases=a->next;
        free_alias(a);
    }
    while(e->traces) {
        struct Tcl_Trace_ *t=e->traces;
        e->traces=t->next;
        if(t->del_proc) t->del_proc(t->data);
        free(t);
    }
    free(e->name_in_master);
    free(e);
}

static interp_ext *get_ext(Tcl_Interp *i,int create) {
    if(!i||i->disposing||i->destroying) return NULL;
    interp_ext *e=Tcl_GetAssocData(i,ext_key,NULL);
    if(!e&&create) {
        e=calloc(1,sizeof *e);
        if(!e) Tcl_Panic("cannot allocate interp ext");
        Tcl_SetAssocData(i,ext_key,dispose_ext,e);
    }
    return e;
}

Tcl_Interp *Tcl_CreateSlave(Tcl_Interp *i,const char *name,int isSafe) {
    if(!i||i->disposing||!name||!*name) return NULL;
    interp_ext *e=get_ext(i,1);
    for(slave_link *s=e->slaves;s;s=s->next) {
        if(!strcmp(s->name,name)) {
            Tcl_SetObjResult(i,Tcl_ObjPrintf("interpreter named \"%s\" already exists, cannot create",name));
            return NULL;
        }
    }
    Tcl_Interp *child=Tcl_CreateInterp();
    if(!child) return NULL;
    interp_ext *ce=get_ext(child,1);
    ce->master=i;
    ce->name_in_master=dup_cstr(name);
    if(isSafe||e->safe) ce->safe=1;
    slave_link *link=calloc(1,sizeof *link);
    if(!link) { Tcl_DeleteInterp(child); Tcl_Panic("cannot allocate slave link"); }
    link->name=dup_cstr(name);
    link->child=child;
    link->next=e->slaves;
    e->slaves=link;
    return child;
}

Tcl_Interp *Tcl_GetSlave(Tcl_Interp *i,const char *name) {
    interp_ext *e=get_ext(i,0);
    if(!e||!name) return NULL;
    for(slave_link *s=e->slaves;s;s=s->next) {
        if(!strcmp(s->name,name)) return s->child;
    }
    return NULL;
}

Tcl_Interp *Tcl_GetMaster(Tcl_Interp *i) {
    interp_ext *e=get_ext(i,0);
    return e?e->master:NULL;
}

int Tcl_GetInterpPath(Tcl_Interp *ask,Tcl_Interp *child) {
    if(!ask||!child) return TG_ERROR;
    if(ask==child) { Tcl_ResetResult(ask); return TG_OK; }
    const char *parts[64];
    int count=0;
    Tcl_Interp *cur=child;
    while(cur&&cur!=ask&&count<64) {
        interp_ext *ce=get_ext(cur,0);
        if(!ce||!ce->master) break;
        parts[count++]=ce->name_in_master;
        cur=ce->master;
    }
    if(cur!=ask) {
        tg_set_result(ask,"interpreter is not a descendant",TG_ERROR);
        return TG_ERROR;
    }
    Tcl_Obj *list=Tcl_NewListObj(0,NULL);
    TKGPU_RetainObject(list);
    for(int k=count-1;k>=0;k--) {
        Tcl_ListObjAppendElement(ask,list,Tcl_NewStringObj(parts[k],-1));
    }
    Tcl_SetObjResult(ask,list);
    TKGPU_ReleaseObject(list);
    return TG_OK;
}

int Tcl_IsSafe(Tcl_Interp *i) {
    interp_ext *e=get_ext(i,0);
    return e?e->safe:0;
}

int Tcl_MakeSafe(Tcl_Interp *i) {
    interp_ext *e=get_ext(i,1);
    if(!e) return TG_ERROR;
    e->safe=1;
    return TG_OK;
}

int Tcl_HideCommand(Tcl_Interp *i,const char *cmdName,const char *hiddenToken) {
    if(!i||i->disposing||!cmdName||!hiddenToken) return TG_ERROR;
    Tcl_CmdInfo info;
    if(!Tcl_GetCommandInfo(i,cmdName,&info)) {
        Tcl_SetObjResult(i,Tcl_ObjPrintf("unknown command \"%s\"",cmdName));
        return TG_ERROR;
    }
    interp_ext *e=get_ext(i,1);
    for(hidden_cmd *h=e->hidden;h;h=h->next) {
        if(!strcmp(h->token,hiddenToken)) {
            Tcl_SetObjResult(i,Tcl_ObjPrintf("hidden command named \"%s\" already exists",hiddenToken));
            return TG_ERROR;
        }
    }
    hidden_cmd *h=calloc(1,sizeof *h);
    if(!h) Tcl_Panic("cannot allocate hidden command");
    h->token=dup_cstr(hiddenToken);
    h->info=info;
    h->next=e->hidden;
    e->hidden=h;
    /* Detach delete proc before deleting exposed command so clientData stays alive. */
    Tcl_CmdInfo stripped=info;
    stripped.deleteProc=NULL;
    (void)Tcl_SetCommandInfo(i,cmdName,&stripped);
    (void)Tcl_DeleteCommand(i,cmdName);
    return TG_OK;
}

int Tcl_ExposeCommand(Tcl_Interp *i,const char *hiddenToken,const char *cmdName) {
    if(!i||i->disposing||!hiddenToken||!cmdName) return TG_ERROR;
    interp_ext *e=get_ext(i,0);
    if(!e) return TG_ERROR;
    hidden_cmd **link=&e->hidden;
    while(*link) {
        hidden_cmd *h=*link;
        if(!strcmp(h->token,hiddenToken)) {
            *link=h->next;
            if(h->info.isNativeObjectProc) {
                (void)Tcl_CreateObjCommand(i,cmdName,h->info.objProc,h->info.objClientData,h->info.deleteProc);
            } else {
                (void)Tcl_CreateCommand(i,cmdName,h->info.proc,h->info.clientData,h->info.deleteProc);
            }
            free(h->token);
            free(h);
            return TG_OK;
        }
        link=&h->next;
    }
    Tcl_SetObjResult(i,Tcl_ObjPrintf("unknown hidden command \"%s\"",hiddenToken));
    return TG_ERROR;
}

static int alias_invoke(void *data,Tcl_Interp *child,int objc,Tcl_Obj *const objv[]) {
    alias_entry *a=data;
    if(!a||!a->target||Tcl_InterpDeleted(a->target)) return TG_ERROR;
    int total=1+a->objc+(objc>1?objc-1:0);
    Tcl_Obj **call=malloc((size_t)total*sizeof *call);
    if(!call) return TG_ERROR;
    Tcl_Obj *head=Tcl_NewStringObj(a->target_cmd,-1);
    TKGPU_RetainObject(head);
    call[0]=head;
    for(int k=0;k<a->objc;k++) call[1+k]=a->objv[k];
    for(int k=1;k<objc;k++) call[1+a->objc+k-1]=objv[k];
    int rc=Tcl_EvalObjv(a->target,total,call,0);
    TKGPU_ReleaseObject(head);
    free(call);
    if(a->target!=child) Tcl_TransferResult(a->target,rc,child);
    return rc;
}

int Tcl_CreateAliasObj(Tcl_Interp *child,const char *childCmd,Tcl_Interp *target,
                       const char *targetCmd,int objc,Tcl_Obj *const objv[]) {
    if(!child||child->disposing||!childCmd) return TG_ERROR;
    interp_ext *e=get_ext(child,1);
    /* Remove any existing alias with the same name. */
    alias_entry **link=&e->aliases;
    while(*link) {
        if(!strcmp((*link)->child_cmd,childCmd)) {
            alias_entry *old=*link;
            *link=old->next;
            free_alias(old);
            break;
        }
        link=&(*link)->next;
    }
    if(!targetCmd) {
        (void)Tcl_DeleteCommand(child,childCmd);
        return TG_OK;
    }
    if(!target) return TG_ERROR;
    alias_entry *a=calloc(1,sizeof *a);
    if(!a) Tcl_Panic("cannot allocate alias");
    a->child_cmd=dup_cstr(childCmd);
    a->target=target;
    a->target_cmd=dup_cstr(targetCmd);
    a->objc=objc>0?objc:0;
    if(a->objc>0) {
        a->objv=calloc((size_t)a->objc,sizeof *a->objv);
        a->argv=calloc((size_t)a->objc,sizeof *a->argv);
        if(!a->objv||!a->argv) Tcl_Panic("cannot allocate alias args");
        for(int k=0;k<a->objc;k++) {
            a->objv[k]=objv[k];
            TKGPU_RetainObject(a->objv[k]);
            a->argv[k]=Tcl_GetString(a->objv[k]);
        }
    }
    a->next=e->aliases;
    e->aliases=a;
    if(!Tcl_CreateObjCommand(child,childCmd,alias_invoke,a,NULL)) return TG_ERROR;
    return TG_OK;
}

int Tcl_CreateAlias(Tcl_Interp *child,const char *childCmd,Tcl_Interp *target,
                    const char *targetCmd,int argc,const char *const *argv) {
    Tcl_Obj **objs=NULL;
    if(argc>0&&argv) {
        objs=calloc((size_t)argc,sizeof *objs);
        if(!objs) Tcl_Panic("cannot allocate alias arg objects");
        for(int k=0;k<argc;k++) {
            objs[k]=Tcl_NewStringObj(argv[k]?argv[k]:"",-1);
            TKGPU_RetainObject(objs[k]);
        }
    }
    int rc=Tcl_CreateAliasObj(child,childCmd,target,targetCmd,argc,objs);
    if(objs) {
        for(int k=0;k<argc;k++) TKGPU_ReleaseObject(objs[k]);
        free(objs);
    }
    return rc;
}

int Tcl_GetAliasObj(Tcl_Interp *i,const char *childCmd,Tcl_Interp **targetPtr,
                    const char **targetCmdPtr,int *objcPtr,Tcl_Obj ***objvPtr) {
    interp_ext *e=get_ext(i,0);
    if(!e||!childCmd) return TG_ERROR;
    for(alias_entry *a=e->aliases;a;a=a->next) {
        if(!strcmp(a->child_cmd,childCmd)) {
            if(targetPtr) *targetPtr=a->target;
            if(targetCmdPtr) *targetCmdPtr=a->target_cmd;
            if(objcPtr) *objcPtr=a->objc;
            if(objvPtr) *objvPtr=a->objv;
            return TG_OK;
        }
    }
    Tcl_SetObjResult(i,Tcl_ObjPrintf("alias \"%s\" not found",childCmd));
    return TG_ERROR;
}

int Tcl_GetAlias(Tcl_Interp *i,const char *childCmd,Tcl_Interp **targetPtr,
                 const char **targetCmdPtr,int *argcPtr,const char ***argvPtr) {
    interp_ext *e=get_ext(i,0);
    if(!e||!childCmd) return TG_ERROR;
    for(alias_entry *a=e->aliases;a;a=a->next) {
        if(!strcmp(a->child_cmd,childCmd)) {
            if(targetPtr) *targetPtr=a->target;
            if(targetCmdPtr) *targetCmdPtr=a->target_cmd;
            if(argcPtr) *argcPtr=a->objc;
            if(argvPtr) *argvPtr=a->argv;
            return TG_OK;
        }
    }
    Tcl_SetObjResult(i,Tcl_ObjPrintf("alias \"%s\" not found",childCmd));
    return TG_ERROR;
}

Tcl_Trace Tcl_CreateTrace(Tcl_Interp *i,int level,Tcl_CmdTraceProc *proc,void *data) {
    if(!i||i->disposing||!proc) return NULL;
    interp_ext *e=get_ext(i,1);
    struct Tcl_Trace_ *t=calloc(1,sizeof *t);
    if(!t) Tcl_Panic("cannot allocate trace");
    t->level=level; t->is_obj=0; t->str_proc=proc; t->data=data;
    t->next=e->traces; e->traces=t;
    return t;
}

Tcl_Trace Tcl_CreateObjTrace(Tcl_Interp *i,int level,int flags,Tcl_CmdObjTraceProc *proc,
                             void *data,Tcl_CmdObjTraceDeleteProc *delProc) {
    if(!i||i->disposing||!proc) return NULL;
    interp_ext *e=get_ext(i,1);
    struct Tcl_Trace_ *t=calloc(1,sizeof *t);
    if(!t) Tcl_Panic("cannot allocate obj trace");
    t->level=level; t->flags=flags; t->is_obj=1;
    t->obj_proc=proc; t->del_proc=delProc; t->data=data;
    t->next=e->traces; e->traces=t;
    return t;
}

void Tcl_DeleteTrace(Tcl_Interp *i,Tcl_Trace trace) {
    interp_ext *e=get_ext(i,0);
    if(!e||!trace) return;
    struct Tcl_Trace_ **link=&e->traces;
    while(*link) {
        if(*link==trace) {
            *link=trace->next;
            if(trace->del_proc) trace->del_proc(trace->data);
            free(trace);
            return;
        }
        link=&(*link)->next;
    }
}
