#include "TKGPUCompat.H"
#include "../../core/TKGPUCoreInternal.H"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    TRACE_READS=0x10,
    TRACE_WRITES=0x20,
    TRACE_UNSETS=0x40,
    TRACE_DESTROYED=0x80,
    INTERP_DESTROYED=0x100,
    TRACE_RENAME=0x2000,
    TRACE_DELETE=0x4000
};

enum {
    LINK_INT=1, LINK_DOUBLE=2, LINK_BOOLEAN=3, LINK_STRING=4,
    LINK_WIDE_INT=5, LINK_CHAR=6, LINK_UCHAR=7, LINK_SHORT=8,
    LINK_USHORT=9, LINK_UINT=10, LINK_LONG=11, LINK_ULONG=12,
    LINK_FLOAT=13, LINK_WIDE_UINT=14, LINK_READ_ONLY=0x80
};

typedef struct var_trace {
    char *part1;
    char *part2;
    int flags;
    Tcl_VarTraceProc *proc;
    void *data;
    struct var_trace *next;
} var_trace;

typedef struct cmd_trace {
    char *name;
    int flags;
    Tcl_CommandTraceProc *proc;
    void *data;
    struct cmd_trace *next;
} cmd_trace;

typedef struct link_entry {
    char *name;
    char *addr;
    int type;
    int updating;
    struct link_entry *next;
} link_entry;

typedef struct {
    var_trace *var_traces;
    cmd_trace *cmd_traces;
    link_entry *links;
} trace_registry;

static const char trace_reg_key[]="TKGPU::traces";

static char *copy_cstr(const char *s) {
    if(!s) return NULL;
    size_t n=strlen(s);
    char *p=malloc(n+1);
    if(!p) Tcl_Panic("cannot copy trace string");
    memcpy(p,s,n+1);
    return p;
}

static int str_eq(const char *a,const char *b) {
    if(!a&&!b) return 1;
    if(!a||!b) return 0;
    return strcmp(a,b)==0;
}

static void dispose_traces(void *data,Tcl_Interp *i) {
    trace_registry *r=data;
    while(r->links) {
        link_entry *l=r->links; r->links=l->next;
        free(l->name); free(l);
    }
    while(r->var_traces) {
        var_trace *t=r->var_traces; r->var_traces=t->next;
        if((t->flags&TRACE_UNSETS)&&t->proc) {
            (void)t->proc(t->data,i,t->part1,t->part2,TRACE_UNSETS|TRACE_DESTROYED|INTERP_DESTROYED);
        }
        free(t->part1); free(t->part2); free(t);
    }
    while(r->cmd_traces) {
        cmd_trace *c=r->cmd_traces; r->cmd_traces=c->next;
        free(c->name); free(c);
    }
    free(r);
}

static trace_registry *get_traces(Tcl_Interp *i,int create) {
    if(!i||i->disposing||i->destroying) return NULL;
    trace_registry *r=Tcl_GetAssocData(i,trace_reg_key,NULL);
    if(!r&&create) {
        r=calloc(1,sizeof *r);
        if(!r) Tcl_Panic("cannot allocate trace registry");
        Tcl_SetAssocData(i,trace_reg_key,dispose_traces,r);
    }
    return r;
}

static void split_part1(const char *varName,char **p1,char **p2) {
    const char *lp=varName?strchr(varName,'('):NULL;
    size_t n=varName?strlen(varName):0;
    if(lp&&n>0&&varName[n-1]==')') {
        size_t n1=(size_t)(lp-varName),n2=(size_t)(varName+n-1-(lp+1));
        *p1=malloc(n1+1); *p2=malloc(n2+1);
        if(!*p1||!*p2) Tcl_Panic("cannot split var name");
        memcpy(*p1,varName,n1); (*p1)[n1]=0;
        memcpy(*p2,lp+1,n2); (*p2)[n2]=0;
    } else {
        *p1=copy_cstr(varName); *p2=NULL;
    }
}

int Tcl_TraceVar2(Tcl_Interp *i,const char *part1,const char *part2,int flags,
                  Tcl_VarTraceProc *proc,void *data) {
    if(!i||i->disposing||!part1||!proc) return TG_ERROR;
    trace_registry *r=get_traces(i,1);
    var_trace *t=calloc(1,sizeof *t);
    if(!t) Tcl_Panic("cannot allocate var trace");
    t->part1=copy_cstr(part1);
    t->part2=copy_cstr(part2);
    t->flags=flags;
    t->proc=proc;
    t->data=data;
    t->next=r->var_traces;
    r->var_traces=t;
    return TG_OK;
}

int Tcl_TraceVar(Tcl_Interp *i,const char *varName,int flags,Tcl_VarTraceProc *proc,void *data) {
    char *p1=NULL,*p2=NULL;
    split_part1(varName,&p1,&p2);
    int rc=Tcl_TraceVar2(i,p1,p2,flags,proc,data);
    free(p1); free(p2);
    return rc;
}

void Tcl_UntraceVar2(Tcl_Interp *i,const char *part1,const char *part2,int flags,
                     Tcl_VarTraceProc *proc,void *data) {
    trace_registry *r=get_traces(i,0);
    if(!r||!part1) return;
    int mask=flags&(TRACE_READS|TRACE_WRITES|TRACE_UNSETS);
    var_trace **link=&r->var_traces;
    while(*link) {
        var_trace *t=*link;
        if(str_eq(t->part1,part1)&&str_eq(t->part2,part2)&&t->proc==proc&&t->data==data&&
           (!mask||(t->flags&mask)==mask)) {
            *link=t->next;
            free(t->part1); free(t->part2); free(t);
            return;
        }
        link=&t->next;
    }
}

void Tcl_UntraceVar(Tcl_Interp *i,const char *varName,int flags,Tcl_VarTraceProc *proc,void *data) {
    char *p1=NULL,*p2=NULL;
    split_part1(varName,&p1,&p2);
    Tcl_UntraceVar2(i,p1,p2,flags,proc,data);
    free(p1); free(p2);
}

void *Tcl_VarTraceInfo2(Tcl_Interp *i,const char *part1,const char *part2,int flags,
                        Tcl_VarTraceProc *proc,void *prevData) {
    trace_registry *r=get_traces(i,0);
    if(!r||!part1) return NULL;
    (void)flags;
    int seen=(prevData==NULL);
    for(var_trace *t=r->var_traces;t;t=t->next) {
        if(!str_eq(t->part1,part1)||!str_eq(t->part2,part2)) continue;
        if(proc&&t->proc!=proc) continue;
        if(seen) return t->data;
        if(t->data==prevData) seen=1;
    }
    return NULL;
}

void *Tcl_VarTraceInfo(Tcl_Interp *i,const char *varName,int flags,
                       Tcl_VarTraceProc *proc,void *prevData) {
    char *p1=NULL,*p2=NULL;
    split_part1(varName,&p1,&p2);
    void *res=Tcl_VarTraceInfo2(i,p1,p2,flags,proc,prevData);
    free(p1); free(p2);
    return res;
}

int Tcl_TraceCommand(Tcl_Interp *i,const char *name,int flags,
                     Tcl_CommandTraceProc *proc,void *data) {
    if(!i||i->disposing||!name||!proc) return TG_ERROR;
    if(!Tcl_FindCommand(i,name,NULL,0)) {
        Tcl_SetObjResult(i,Tcl_ObjPrintf("unknown command \"%s\"",name));
        return TG_ERROR;
    }
    trace_registry *r=get_traces(i,1);
    cmd_trace *c=calloc(1,sizeof *c);
    if(!c) Tcl_Panic("cannot allocate command trace");
    c->name=copy_cstr(name);
    c->flags=flags;
    c->proc=proc;
    c->data=data;
    c->next=r->cmd_traces;
    r->cmd_traces=c;
    return TG_OK;
}

void Tcl_UntraceCommand(Tcl_Interp *i,const char *name,int flags,
                        Tcl_CommandTraceProc *proc,void *data) {
    trace_registry *r=get_traces(i,0);
    if(!r||!name) return;
    int mask=flags&(TRACE_RENAME|TRACE_DELETE);
    cmd_trace **link=&r->cmd_traces;
    while(*link) {
        cmd_trace *c=*link;
        if(!strcmp(c->name,name)&&c->proc==proc&&c->data==data&&(!mask||(c->flags&mask)==mask)) {
            *link=c->next;
            free(c->name); free(c);
            return;
        }
        link=&c->next;
    }
}

void *Tcl_CommandTraceInfo(Tcl_Interp *i,const char *name,int flags,
                           Tcl_CommandTraceProc *proc,void *prevData) {
    trace_registry *r=get_traces(i,0);
    if(!r||!name) return NULL;
    (void)flags;
    int seen=(prevData==NULL);
    for(cmd_trace *c=r->cmd_traces;c;c=c->next) {
        if(strcmp(c->name,name)!=0) continue;
        if(proc&&c->proc!=proc) continue;
        if(seen) return c->data;
        if(c->data==prevData) seen=1;
    }
    return NULL;
}

/* --- LinkVar --- */

static Tcl_Obj *read_linked_value(link_entry *l) {
    int base=l->type&~LINK_READ_ONLY;
    switch(base) {
    case LINK_INT: return Tcl_NewIntObj(*(int *)l->addr);
    case LINK_UINT: return Tcl_ObjPrintf("%u",*(unsigned int *)l->addr);
    case LINK_CHAR: return Tcl_NewIntObj((int)*(char *)l->addr);
    case LINK_UCHAR: return Tcl_NewIntObj((int)*(unsigned char *)l->addr);
    case LINK_SHORT: return Tcl_NewIntObj((int)*(short *)l->addr);
    case LINK_USHORT: return Tcl_NewIntObj((int)*(unsigned short *)l->addr);
    case LINK_LONG: return Tcl_NewLongObj(*(long *)l->addr);
    case LINK_ULONG: return Tcl_ObjPrintf("%lu",*(unsigned long *)l->addr);
    case LINK_WIDE_INT: return Tcl_NewWideIntObj(*(long long *)l->addr);
    case LINK_WIDE_UINT: return Tcl_ObjPrintf("%llu",*(unsigned long long *)l->addr);
    case LINK_BOOLEAN: return Tcl_NewIntObj(*(int *)l->addr?1:0);
    case LINK_DOUBLE: return Tcl_NewDoubleObj(*(double *)l->addr);
    case LINK_FLOAT: return Tcl_NewDoubleObj((double)*(float *)l->addr);
    case LINK_STRING: {
        char *s=*(char **)l->addr;
        return Tcl_NewStringObj(s?s:"NULL",-1);
    }
    default: return Tcl_NewStringObj("",0);
    }
}

static void sync_link_to_tcl(Tcl_Interp *i,link_entry *l) {
    if(!i||!l||l->updating) return;
    l->updating=1;
    Tcl_Obj *v=read_linked_value(l);
    if(v) {
        TKGPU_RetainObject(v);
        (void)Tcl_SetVar2Ex(i,l->name,NULL,v,TKGPU_GLOBAL_ONLY);
        TKGPU_ReleaseObject(v);
    }
    l->updating=0;
}

static char *link_trace_proc(void *data,Tcl_Interp *i,const char *p1,const char *p2,int flags) {
    link_entry *l=data;
    (void)p1; (void)p2;
    if(flags&TRACE_UNSETS) return NULL;
    if(l->updating) return NULL;
    if(flags&TRACE_READS) {
        sync_link_to_tcl(i,l);
        return NULL;
    }
    if(flags&TRACE_WRITES) {
        if(l->type&LINK_READ_ONLY) {
            sync_link_to_tcl(i,l);
            return "linked variable is read-only";
        }
        Tcl_Obj *v=Tcl_GetVar2Ex(i,l->name,NULL,TKGPU_GLOBAL_ONLY);
        if(!v) return NULL;
        int base=l->type&~LINK_READ_ONLY;
        if(base==LINK_INT) {
            int x=0;
            if(Tcl_GetIntFromObj(NULL,v,&x)!=TG_OK) return "variable must have integer value";
            *(int *)l->addr=x;
        } else if(base==LINK_BOOLEAN) {
            int x=0;
            if(Tcl_GetBooleanFromObj(NULL,v,&x)!=TG_OK) return "variable must have boolean value";
            *(int *)l->addr=x;
        } else if(base==LINK_DOUBLE) {
            double x=0;
            if(Tcl_GetDoubleFromObj(NULL,v,&x)!=TG_OK) return "variable must have real value";
            *(double *)l->addr=x;
        } else if(base==LINK_FLOAT) {
            double x=0;
            if(Tcl_GetDoubleFromObj(NULL,v,&x)!=TG_OK) return "variable must have real value";
            *(float *)l->addr=(float)x;
        } else if(base==LINK_WIDE_INT) {
            long long x=0;
            if(Tcl_GetWideIntFromObj(NULL,v,&x)!=TG_OK) return "variable must have integer value";
            *(long long *)l->addr=x;
        }
    }
    return NULL;
}

int Tcl_LinkVar(Tcl_Interp *i,const char *varName,char *addr,int type) {
    if(!i||i->disposing||!varName||!addr) return TG_ERROR;
    trace_registry *r=get_traces(i,1);
    for(link_entry *l=r->links;l;l=l->next) {
        if(!strcmp(l->name,varName)) {
            Tcl_SetObjResult(i,Tcl_ObjPrintf("variable \"%s\" is already linked",varName));
            return TG_ERROR;
        }
    }
    link_entry *l=calloc(1,sizeof *l);
    if(!l) Tcl_Panic("cannot allocate link entry");
    l->name=copy_cstr(varName);
    l->addr=addr;
    l->type=type;
    l->next=r->links;
    r->links=l;
    sync_link_to_tcl(i,l);
    (void)Tcl_TraceVar2(i,varName,NULL,TRACE_READS|TRACE_WRITES|TRACE_UNSETS,link_trace_proc,l);
    return TG_OK;
}

void Tcl_UnlinkVar(Tcl_Interp *i,const char *varName) {
    trace_registry *r=get_traces(i,0);
    if(!r||!varName) return;
    link_entry **link=&r->links;
    while(*link) {
        link_entry *l=*link;
        if(!strcmp(l->name,varName)) {
            *link=l->next;
            Tcl_UntraceVar2(i,varName,NULL,TRACE_READS|TRACE_WRITES|TRACE_UNSETS,link_trace_proc,l);
            free(l->name);
            free(l);
            return;
        }
        link=&l->next;
    }
}

void Tcl_UpdateLinkedVar(Tcl_Interp *i,const char *varName) {
    trace_registry *r=get_traces(i,0);
    if(!r||!varName) return;
    for(link_entry *l=r->links;l;l=l->next) {
        if(!strcmp(l->name,varName)) {
            sync_link_to_tcl(i,l);
            return;
        }
    }
}

/* --- UpVar & ProcObjCmd --- */

int Tcl_UpVar2(Tcl_Interp *i,const char *frameName,const char *part1,const char *part2,
               const char *localName,int flags) {
    (void)frameName; (void)flags;
    if(!i||i->disposing||!part1||!localName) return TG_ERROR;
    Tcl_Obj *val=Tcl_GetVar2Ex(i,part1,part2,TKGPU_GLOBAL_ONLY);
    if(val) (void)Tcl_SetVar2Ex(i,localName,NULL,val,0);
    return TG_OK;
}

int Tcl_UpVar(Tcl_Interp *i,const char *frameName,const char *varName,const char *localName,int flags) {
    char *p1=NULL,*p2=NULL;
    split_part1(varName,&p1,&p2);
    int rc=Tcl_UpVar2(i,frameName,p1,p2,localName,flags);
    free(p1); free(p2);
    return rc;
}

int Tcl_ProcObjCmd(void *data,Tcl_Interp *i,int objc,Tcl_Obj *const objv[]) {
    (void)data;
    if(!i||i->disposing) return TG_ERROR;
    if(objc!=4) {
        Tcl_WrongNumArgs(i,1,objv,"name args body");
        return TG_ERROR;
    }
    return Tcl_EvalObjv(i,objc,objv,0);
}
