#include "TKGPUCompat.H"
#include "../../core/TKGPUCoreInternal.H"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void Tcl_AllowExceptions(Tcl_Interp *i) { (void)i; }

typedef struct {
    Tcl_Interp *interp;
    char *message;
    int code;
} bg_error_job;

static void run_bg_error(void *data) {
    bg_error_job *job=data;
    Tcl_Interp *i=job->interp;
    if(i&&!Tcl_InterpDeleted(i)) {
        Tcl_CmdInfo info;
        if(Tcl_GetCommandInfo(i,"bgerror",&info)) {
            Tcl_Obj *argv[2]={
                Tcl_NewStringObj("bgerror",-1),
                Tcl_NewStringObj(job->message?job->message:"",-1)
            };
            TKGPU_RetainObject(argv[0]);
            TKGPU_RetainObject(argv[1]);
            (void)Tcl_EvalObjv(i,2,argv,0);
            TKGPU_ReleaseObject(argv[1]);
            TKGPU_ReleaseObject(argv[0]);
        }
    }
    free(job->message);
    free(job);
}

void Tcl_BackgroundException(Tcl_Interp *i,int code) {
    if(!i||i->disposing||i->destroying) return;
    const char *msg=Tcl_GetStringResult(i);
    bg_error_job *job=calloc(1,sizeof *job);
    if(!job) Tcl_Panic("cannot allocate background exception job");
    job->interp=i;
    job->code=code;
    if(msg) {
        size_t n=strlen(msg);
        job->message=malloc(n+1);
        if(!job->message) { free(job); Tcl_Panic("cannot copy background exception message"); }
        memcpy(job->message,msg,n+1);
    }
    Tcl_DoWhenIdle(run_bg_error,job);
}

void Tcl_BackgroundError(Tcl_Interp *i) {
    Tcl_BackgroundException(i,TG_ERROR);
}

void Tcl_LogCommandInfo(Tcl_Interp *i,const char *script,const char *command,int length) {
    if(!i||i->disposing||!command) return;
    int line=1;
    if(script&&command>=script) {
        for(const char *p=script;p<command;p++) if(*p=='\n') ++line;
    }
    Tcl_SetErrorLine(i,line);
    size_t len=length<0?strlen(command):(size_t)length;
    int truncated=0;
    if(len>150) { len=150; truncated=1; }
    Tcl_Obj *info=Tcl_ObjPrintf("\n    while executing\n\"%.*s%s\"",(int)len,command,truncated?"...":"");
    if(info) {
        TKGPU_RetainObject(info);
        Tcl_AppendObjToErrorInfo(i,info);
        TKGPU_ReleaseObject(info);
    }
}

int Tcl_PutEnv(const char *assignment) {
    if(!assignment||!*assignment) return TG_ERROR;
    const char *eq=strchr(assignment,'=');
    if(!eq||eq==assignment) return TG_ERROR;
#ifdef _WIN32
    return _putenv(assignment)==0?TG_OK:TG_ERROR;
#else
    size_t klen=(size_t)(eq-assignment);
    char *key=malloc(klen+1);
    if(!key) return TG_ERROR;
    memcpy(key,assignment,klen);
    key[klen]=0;
    int rc=eq[1]?setenv(key,eq+1,1):unsetenv(key);
    free(key);
    return rc==0?TG_OK:TG_ERROR;
#endif
}

typedef struct config_pair {
    char *key;
    char *value;
} config_pair;

typedef struct {
    config_pair *items;
    size_t count;
} config_table;

static void delete_config_table(void *data) {
    config_table *t=data;
    if(!t) return;
    for(size_t k=0;k<t->count;k++) {
        free(t->items[k].key);
        free(t->items[k].value);
    }
    free(t->items);
    free(t);
}

static int pkgconfig_str_cmd(void *data,tg_interp *i,size_t argc,const char *const argv[]) {
    config_table *t=data;
    if(argc<2) {
        tg_set_result(i,"wrong # args: should be \"pkgconfig subcommand ?arg ...?\"",TG_ERROR);
        return TG_ERROR;
    }
    if(!strcmp(argv[1],"list")) {
        if(argc!=2) {
            tg_set_result(i,"wrong # args: should be \"pkgconfig list\"",TG_ERROR);
            return TG_ERROR;
        }
        Tcl_Obj *list=Tcl_NewListObj(0,NULL);
        TKGPU_RetainObject(list);
        for(size_t k=0;k<t->count;k++) {
            Tcl_ListObjAppendElement(i,list,Tcl_NewStringObj(t->items[k].key,-1));
        }
        Tcl_SetObjResult(i,list);
        TKGPU_ReleaseObject(list);
        return TG_OK;
    }
    if(!strcmp(argv[1],"get")) {
        if(argc!=3) {
            tg_set_result(i,"wrong # args: should be \"pkgconfig get key\"",TG_ERROR);
            return TG_ERROR;
        }
        for(size_t k=0;k<t->count;k++) {
            if(!strcmp(t->items[k].key,argv[2])) {
                Tcl_SetObjResult(i,Tcl_NewStringObj(t->items[k].value,-1));
                return TG_OK;
            }
        }
        Tcl_SetObjResult(i,Tcl_ObjPrintf("key not known: \"%s\"",argv[2]));
        return TG_ERROR;
    }
    Tcl_SetObjResult(i,Tcl_ObjPrintf("bad subcommand \"%s\": must be get or list",argv[1]));
    return TG_ERROR;
}

static char *dup_str(const char *s) {
    size_t n=s?strlen(s):0;
    char *copy=malloc(n+1);
    if(!copy) Tcl_Panic("cannot copy config string");
    if(n) memcpy(copy,s,n);
    copy[n]=0;
    return copy;
}

void Tcl_RegisterConfig(Tcl_Interp *i,const char *pkgName,const Tcl_Config *cfg,const char *valEncoding) {
    (void)valEncoding;
    if(!i||i->disposing||!pkgName||!cfg) return;
    while(pkgName[0]==':'&&pkgName[1]==':') pkgName+=2;
    if(!*pkgName) return;
    size_t count=0;
    while(cfg[count].key) ++count;
    config_table *t=calloc(1,sizeof *t);
    if(!t) Tcl_Panic("cannot allocate config table");
    if(count) {
        t->items=calloc(count,sizeof *t->items);
        if(!t->items) Tcl_Panic("cannot allocate config items");
        for(size_t k=0;k<count;k++) {
            t->items[k].key=dup_str(cfg[k].key);
            t->items[k].value=dup_str(cfg[k].value);
        }
        t->count=count;
    }
    Tcl_Obj *cmdName=Tcl_ObjPrintf("::%s::pkgconfig",pkgName);
    TKGPU_RetainObject(cmdName);
    if(tg_command_register(i,Tcl_GetString(cmdName),pkgconfig_str_cmd,t,delete_config_table)!=TG_OK) {
        delete_config_table(t);
    }
    TKGPU_ReleaseObject(cmdName);
}

void Tcl_SourceRCFile(Tcl_Interp *i) {
    if(!i||i->disposing) return;
    const char *rc=Tcl_GetVar(i,"tcl_rcFileName",TKGPU_GLOBAL_ONLY);
    if(!rc||!*rc) return;
    FILE *f=fopen(rc,"rb");
    if(!f) return;
    fclose(f);
    (void)Tcl_EvalFile(i,rc);
}

typedef struct static_pkg {
    char *prefix;
    Tcl_PackageInitProc *initProc;
    Tcl_PackageInitProc *safeInitProc;
    struct static_pkg *next;
} static_pkg;

static Tcl_Mutex static_pkg_mutex;
static static_pkg *static_pkgs;

void Tcl_StaticPackage(Tcl_Interp *i,const char *prefix,
                       Tcl_PackageInitProc *initProc,Tcl_PackageInitProc *safeInitProc) {
    (void)i;
    if(!prefix||!*prefix) return;
    Tcl_MutexLock(&static_pkg_mutex);
    for(static_pkg *p=static_pkgs;p;p=p->next) {
        if(!strcmp(p->prefix,prefix)) {
            p->initProc=initProc;
            p->safeInitProc=safeInitProc;
            Tcl_MutexUnlock(&static_pkg_mutex);
            return;
        }
    }
    static_pkg *node=calloc(1,sizeof *node);
    if(!node) { Tcl_MutexUnlock(&static_pkg_mutex); Tcl_Panic("cannot allocate static package"); }
    node->prefix=dup_str(prefix);
    node->initProc=initProc;
    node->safeInitProc=safeInitProc;
    node->next=static_pkgs;
    static_pkgs=node;
    Tcl_MutexUnlock(&static_pkg_mutex);
}
