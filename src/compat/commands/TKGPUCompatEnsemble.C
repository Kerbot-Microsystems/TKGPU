#include "TKGPUCompat.H"
#include "../../core/TKGPUCoreInternal.H"
#include <stdlib.h>
#include <string.h>

typedef struct ns_entry {
    Tcl_Namespace pub;
    Tcl_Interp *interp;
    Tcl_Obj *unknown_handler;
    Tcl_Obj *imports;
    struct ns_entry *next;
} ns_entry;

typedef struct {
    ns_entry *children;
    Tcl_Obj *global_unknown;
    Tcl_Obj *global_imports;
} ns_registry;

static const char ns_reg_key[]="TKGPU::namespaces";

static void free_ns_entry(ns_entry *e) {
    if(!e) return;
    if(e->pub.deleteProc) e->pub.deleteProc(e->pub.clientData);
    if(e->unknown_handler) TKGPU_ReleaseObject(e->unknown_handler);
    if(e->imports) TKGPU_ReleaseObject(e->imports);
    free(e->pub.fullName);
    free(e);
}

static void dispose_ns_registry(void *data,Tcl_Interp *i) {
    ns_registry *r=data;
    (void)i;
    while(r->children) {
        ns_entry *e=r->children;
        r->children=e->next;
        free_ns_entry(e);
    }
    if(r->global_unknown) TKGPU_ReleaseObject(r->global_unknown);
    if(r->global_imports) TKGPU_ReleaseObject(r->global_imports);
    free(r);
}

static ns_registry *get_ns_registry(Tcl_Interp *i,int create) {
    if(!i||i->disposing||i->destroying) return NULL;
    ns_registry *r=Tcl_GetAssocData(i,ns_reg_key,NULL);
    if(!r&&create) {
        r=calloc(1,sizeof *r);
        if(!r) Tcl_Panic("cannot allocate namespace registry");
        Tcl_SetAssocData(i,ns_reg_key,dispose_ns_registry,r);
    }
    return r;
}

static char *canonical_ns_name(const char *name) {
    if(!name) return NULL;
    while(name[0]==':'&&name[1]==':') name+=2;
    size_t n=strlen(name);
    while(n>=2&&name[n-1]==':'&&name[n-2]==':') n-=2;
    char *out=malloc(n+3);
    if(!out) Tcl_Panic("cannot allocate namespace name");
    out[0]=':'; out[1]=':';
    if(n) memcpy(out+2,name,n);
    out[n+2]=0;
    return out;
}

static ns_entry *find_ns_entry(ns_registry *r,const char *full) {
    if(!r||!full) return NULL;
    for(ns_entry *e=r->children;e;e=e->next) {
        if(!strcmp(e->pub.fullName,full)) return e;
    }
    return NULL;
}

Tcl_Namespace *tg_compat_find_child_namespace(Tcl_Interp *i,const char *name,Tcl_Namespace *context) {
    ns_registry *r=get_ns_registry(i,0);
    if(!r||!name||!*name) return NULL;
    char *full=NULL;
    if(name[0]==':'&&name[1]==':') {
        full=canonical_ns_name(name);
    } else if(context&&context->fullName&&strcmp(context->fullName,"::")) {
        Tcl_Obj *joined=Tcl_ObjPrintf("%s::%s",context->fullName,name);
        TKGPU_RetainObject(joined);
        full=canonical_ns_name(Tcl_GetString(joined));
        TKGPU_ReleaseObject(joined);
    } else {
        full=canonical_ns_name(name);
    }
    ns_entry *e=find_ns_entry(r,full);
    free(full);
    return e?&e->pub:NULL;
}

Tcl_Namespace *Tcl_CreateNamespace(Tcl_Interp *i,const char *name,void *clientData,Tcl_NamespaceDeleteProc *deleteProc) {
    if(!i||i->disposing||!name) return NULL;
    char *full=canonical_ns_name(name);
    if(!strcmp(full,"::")) {
        Tcl_SetObjResult(i,Tcl_ObjPrintf("can't create namespace \"%s\": already exists",name));
        free(full);
        return NULL;
    }
    ns_registry *r=get_ns_registry(i,1);
    if(find_ns_entry(r,full)) {
        Tcl_SetObjResult(i,Tcl_ObjPrintf("can't create namespace \"%s\": already exists",name));
        free(full);
        return NULL;
    }
    ns_entry *e=calloc(1,sizeof *e);
    if(!e) { free(full); Tcl_Panic("cannot allocate namespace"); }
    e->interp=i;
    e->pub.fullName=full;
    const char *tail=full+2;
    for(const char *p=tail;*p;p++) {
        if(p[0]==':'&&p[1]==':') tail=p+2;
    }
    e->pub.name=(char *)tail;
    e->pub.clientData=clientData;
    e->pub.deleteProc=deleteProc;
    e->pub.parentPtr=Tcl_GetGlobalNamespace(i);
    if(tail>full+2) {
        size_t plen=(size_t)((tail-2)-full);
        char *pbuf=malloc(plen+1);
        if(pbuf) {
            memcpy(pbuf,full,plen); pbuf[plen]=0;
            Tcl_Namespace *parent=Tcl_FindNamespace(i,pbuf,NULL,0);
            if(!parent) parent=Tcl_CreateNamespace(i,pbuf,NULL,NULL);
            if(parent) e->pub.parentPtr=parent;
            free(pbuf);
        }
    }
    e->next=r->children;
    r->children=e;
    return &e->pub;
}

void Tcl_DeleteNamespace(Tcl_Namespace *nsPtr) {
    if(!nsPtr||!nsPtr->fullName||!strcmp(nsPtr->fullName,"::")) return;
    ns_entry *e=(ns_entry *)nsPtr;
    ns_registry *r=get_ns_registry(e->interp,0);
    if(!r) return;
    ns_entry **link=&r->children;
    while(*link) {
        if(*link==e) {
            *link=e->next;
            free_ns_entry(e);
            return;
        }
        link=&(*link)->next;
    }
}

static Tcl_Obj **ns_unknown_slot(Tcl_Interp *i,Tcl_Namespace *nsPtr) {
    ns_registry *r=get_ns_registry(i,1);
    if(!r) return NULL;
    Tcl_Namespace *global=Tcl_GetGlobalNamespace(i);
    if(!nsPtr||nsPtr==global) return &r->global_unknown;
    for(ns_entry *e=r->children;e;e=e->next) {
        if(&e->pub==nsPtr) return &e->unknown_handler;
    }
    return NULL;
}

Tcl_Obj *Tcl_GetNamespaceUnknownHandler(Tcl_Interp *i,Tcl_Namespace *nsPtr) {
    if(!i||i->disposing) return NULL;
    ns_registry *r=get_ns_registry(i,0);
    if(!r) return NULL;
    Tcl_Namespace *global=Tcl_GetGlobalNamespace(i);
    if(!nsPtr||nsPtr==global) return r->global_unknown;
    for(ns_entry *e=r->children;e;e=e->next) {
        if(&e->pub==nsPtr) return e->unknown_handler;
    }
    return NULL;
}

int Tcl_SetNamespaceUnknownHandler(Tcl_Interp *i,Tcl_Namespace *nsPtr,Tcl_Obj *handlerPtr) {
    if(!i||i->disposing) return TG_ERROR;
    Tcl_Obj **slot=ns_unknown_slot(i,nsPtr);
    if(!slot) return TG_ERROR;
    int len=0;
    if(handlerPtr) {
        if(Tcl_ListObjLength(i,handlerPtr,&len)!=TG_OK) return TG_ERROR;
    }
    if(*slot) { TKGPU_ReleaseObject(*slot); *slot=NULL; }
    if(handlerPtr&&len>0) {
        TKGPU_RetainObject(handlerPtr);
        *slot=handlerPtr;
    }
    return TG_OK;
}

int Tcl_Import(Tcl_Interp *i,Tcl_Namespace *nsPtr,const char *pattern,int allowOverwrite) {
    (void)allowOverwrite;
    if(!i||i->disposing||!pattern||!*pattern) return TG_ERROR;
    (void)nsPtr;
    if(!strstr(pattern,"::")) {
        Tcl_SetObjResult(i,Tcl_ObjPrintf("unknown namespace in import pattern \"%s\"",pattern));
        return TG_ERROR;
    }
    return TG_OK;
}

int Tcl_ForgetImport(Tcl_Interp *i,Tcl_Namespace *nsPtr,const char *pattern) {
    if(!i||i->disposing||!pattern) return TG_ERROR;
    (void)nsPtr;
    return TG_OK;
}

/* --- Ensembles --- */

typedef struct ensemble_state {
    Tcl_Interp *interp;
    Tcl_Command token;
    Tcl_Namespace *ns;
    int flags;
    Tcl_Obj *map_dict;
    Tcl_Obj *subcmds;
    Tcl_Obj *params;
    Tcl_Obj *unknown;
    struct ensemble_state *next;
} ensemble_state;

typedef struct {
    ensemble_state *head;
} ensemble_registry;

static const char ens_reg_key[]="TKGPU::ensembles";

static void dispose_ens_registry(void *data,Tcl_Interp *i) {
    (void)i;
    free(data);
}

static ensemble_registry *get_ens_registry(Tcl_Interp *i,int create) {
    if(!i||i->disposing||i->destroying) return NULL;
    ensemble_registry *r=Tcl_GetAssocData(i,ens_reg_key,NULL);
    if(!r&&create) {
        r=calloc(1,sizeof *r);
        if(!r) Tcl_Panic("cannot allocate ensemble registry");
        Tcl_SetAssocData(i,ens_reg_key,dispose_ens_registry,r);
    }
    return r;
}

static Tcl_Mutex ens_global_mutex;
static ensemble_state *all_ensembles;

static ensemble_state *find_ensemble_by_token(Tcl_Command token) {
    if(!token) return NULL;
    Tcl_MutexLock(&ens_global_mutex);
    ensemble_state *found=NULL;
    for(ensemble_state *e=all_ensembles;e;e=e->next) {
        if(e->token==token) { found=e; break; }
    }
    Tcl_MutexUnlock(&ens_global_mutex);
    return found;
}

static void delete_ensemble(void *data) {
    ensemble_state *e=data;
    if(!e) return;
    Tcl_MutexLock(&ens_global_mutex);
    ensemble_state **link=&all_ensembles;
    while(*link) {
        if(*link==e) { *link=e->next; break; }
        link=&(*link)->next;
    }
    Tcl_MutexUnlock(&ens_global_mutex);
    if(e->map_dict) TKGPU_ReleaseObject(e->map_dict);
    if(e->subcmds) TKGPU_ReleaseObject(e->subcmds);
    if(e->params) TKGPU_ReleaseObject(e->params);
    if(e->unknown) TKGPU_ReleaseObject(e->unknown);
    free(e);
}

static int ensemble_dispatch(void *data,Tcl_Interp *i,int objc,Tcl_Obj *const objv[]) {
    ensemble_state *e=data;
    if(objc<2) {
        Tcl_WrongNumArgs(i,1,objv,"subcommand ?arg ...?");
        return TG_ERROR;
    }
    Tcl_Obj *target=NULL;
    if(e->map_dict) {
        if(Tcl_DictObjGet(i,e->map_dict,objv[1],&target)==TG_OK&&target) {
            int tlen=0; Tcl_Obj **tvals=NULL;
            if(Tcl_ListObjGetElements(i,target,&tlen,&tvals)==TG_OK&&tlen>0) {
                int total=tlen+(objc-2);
                Tcl_Obj **call=malloc((size_t)total*sizeof *call);
                if(!call) return TG_ERROR;
                for(int k=0;k<tlen;k++) call[k]=tvals[k];
                for(int k=2;k<objc;k++) call[tlen+k-2]=objv[k];
                int rc=Tcl_EvalObjv(i,total,call,0);
                free(call);
                return rc;
            }
        }
    }
    Tcl_SetObjResult(i,Tcl_ObjPrintf("unknown or ambiguous subcommand \"%s\"",Tcl_GetString(objv[1])));
    return TG_ERROR;
}

Tcl_Command Tcl_CreateEnsemble(Tcl_Interp *i,const char *name,Tcl_Namespace *nsPtr,int flags) {
    if(!i||i->disposing||!name||!*name) return NULL;
    while(name[0]==':'&&name[1]==':') name+=2;
    ensemble_state *e=calloc(1,sizeof *e);
    if(!e) Tcl_Panic("cannot allocate ensemble");
    e->interp=i;
    e->ns=nsPtr?nsPtr:Tcl_GetCurrentNamespace(i);
    e->flags=flags;
    Tcl_Command tok=Tcl_CreateObjCommand(i,name,ensemble_dispatch,e,delete_ensemble);
    if(!tok) { free(e); return NULL; }
    e->token=tok;
    (void)get_ens_registry(i,1);
    Tcl_MutexLock(&ens_global_mutex);
    e->next=all_ensembles;
    all_ensembles=e;
    Tcl_MutexUnlock(&ens_global_mutex);
    return tok;
}

int Tcl_IsEnsemble(Tcl_Command token) {
    return find_ensemble_by_token(token)!=NULL;
}

Tcl_Command Tcl_FindEnsemble(Tcl_Interp *i,Tcl_Obj *cmdNameObj,int flags) {
    if(!i||i->disposing||!cmdNameObj) return NULL;
    Tcl_Command tok=Tcl_GetCommandFromObj(i,cmdNameObj);
    if(tok&&Tcl_IsEnsemble(tok)) return tok;
    if(flags&TKGPU_LEAVE_ERR_MSG) {
        Tcl_SetObjResult(i,Tcl_ObjPrintf("\"%s\" is not an ensemble command",Tcl_GetString(cmdNameObj)));
    }
    return NULL;
}

static ensemble_state *require_ensemble(Tcl_Interp *i,Tcl_Command token) {
    ensemble_state *e=find_ensemble_by_token(token);
    if(!e&&i) tg_set_result(i,"command is not an ensemble",TG_ERROR);
    return e;
}

int Tcl_GetEnsembleFlags(Tcl_Interp *i,Tcl_Command token,int *flagsPtr) {
    ensemble_state *e=require_ensemble(i,token);
    if(!e) return TG_ERROR;
    if(flagsPtr) *flagsPtr=e->flags;
    return TG_OK;
}

int Tcl_SetEnsembleFlags(Tcl_Interp *i,Tcl_Command token,int flags) {
    ensemble_state *e=require_ensemble(i,token);
    if(!e) return TG_ERROR;
    e->flags=flags;
    return TG_OK;
}

int Tcl_GetEnsembleNamespace(Tcl_Interp *i,Tcl_Command token,Tcl_Namespace **nsPtrPtr) {
    ensemble_state *e=require_ensemble(i,token);
    if(!e) return TG_ERROR;
    if(nsPtrPtr) *nsPtrPtr=e->ns;
    return TG_OK;
}

int Tcl_GetEnsembleMappingDict(Tcl_Interp *i,Tcl_Command token,Tcl_Obj **mapDictPtr) {
    ensemble_state *e=require_ensemble(i,token);
    if(!e) return TG_ERROR;
    if(mapDictPtr) *mapDictPtr=e->map_dict;
    return TG_OK;
}

int Tcl_SetEnsembleMappingDict(Tcl_Interp *i,Tcl_Command token,Tcl_Obj *mapDict) {
    ensemble_state *e=require_ensemble(i,token);
    if(!e) return TG_ERROR;
    int size=0;
    if(mapDict&&Tcl_DictObjSize(i,mapDict,&size)!=TG_OK) return TG_ERROR;
    if(e->map_dict) { TKGPU_ReleaseObject(e->map_dict); e->map_dict=NULL; }
    if(mapDict&&size>0) { TKGPU_RetainObject(mapDict); e->map_dict=mapDict; }
    return TG_OK;
}

static int set_ens_list(Tcl_Interp *i,Tcl_Obj **slot,Tcl_Obj *listObj) {
    int len=0;
    if(listObj&&Tcl_ListObjLength(i,listObj,&len)!=TG_OK) return TG_ERROR;
    if(*slot) { TKGPU_ReleaseObject(*slot); *slot=NULL; }
    if(listObj&&len>0) { TKGPU_RetainObject(listObj); *slot=listObj; }
    return TG_OK;
}

int Tcl_GetEnsembleSubcommandList(Tcl_Interp *i,Tcl_Command token,Tcl_Obj **subcmdListPtr) {
    ensemble_state *e=require_ensemble(i,token);
    if(!e) return TG_ERROR;
    if(subcmdListPtr) *subcmdListPtr=e->subcmds;
    return TG_OK;
}

int Tcl_SetEnsembleSubcommandList(Tcl_Interp *i,Tcl_Command token,Tcl_Obj *subcmdList) {
    ensemble_state *e=require_ensemble(i,token);
    if(!e) return TG_ERROR;
    return set_ens_list(i,&e->subcmds,subcmdList);
}

int Tcl_GetEnsembleParameterList(Tcl_Interp *i,Tcl_Command token,Tcl_Obj **paramListPtr) {
    ensemble_state *e=require_ensemble(i,token);
    if(!e) return TG_ERROR;
    if(paramListPtr) *paramListPtr=e->params;
    return TG_OK;
}

int Tcl_SetEnsembleParameterList(Tcl_Interp *i,Tcl_Command token,Tcl_Obj *paramList) {
    ensemble_state *e=require_ensemble(i,token);
    if(!e) return TG_ERROR;
    return set_ens_list(i,&e->params,paramList);
}

int Tcl_GetEnsembleUnknownHandler(Tcl_Interp *i,Tcl_Command token,Tcl_Obj **unknownListPtr) {
    ensemble_state *e=require_ensemble(i,token);
    if(!e) return TG_ERROR;
    if(unknownListPtr) *unknownListPtr=e->unknown;
    return TG_OK;
}

int Tcl_SetEnsembleUnknownHandler(Tcl_Interp *i,Tcl_Command token,Tcl_Obj *unknownList) {
    ensemble_state *e=require_ensemble(i,token);
    if(!e) return TG_ERROR;
    return set_ens_list(i,&e->unknown,unknownList);
}
