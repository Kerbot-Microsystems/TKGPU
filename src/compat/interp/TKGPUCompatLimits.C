#include "TKGPUCompat.H"
#include "../../core/TKGPUCoreInternal.H"
#include <stdint.h>
#include <stdlib.h>

/* Limits belong to the interpreter. A handler is detached before its delete
   callback; snapshots retain handlers across removal and callback reentry. */
typedef struct limit_handler {
    struct limit_handler *next;
    int type,removed;
    unsigned refs;
    Tcl_LimitHandlerProc *proc;
    Tcl_LimitHandlerDeleteProc *dispose;
    void *data;
} limit_handler;
typedef struct {
    unsigned enabled,exceeded,checking;
    uint64_t commands;
    int command_limit,granularity[2];
    Tcl_Time time;
    limit_handler *handlers;
} limits;
static const char key[]="TKGPU::evaluation-limits";
static void release_handler(limit_handler *h) {
    if(--h->refs) return;
    Tcl_LimitHandlerDeleteProc *dispose=h->dispose;
    void *data=h->data;
    free(h);
    if(dispose) dispose(data);
}
static void dispose_limits(void *data,Tcl_Interp *i) {
    limits *s=data;
    (void)i;
    while(s->handlers) {
        limit_handler *h=s->handlers;
        s->handlers=h->next; h->removed=1; release_handler(h);
    }
    free(s);
}
static limits *state(Tcl_Interp *i) {
    if(!i||i->disposing||i->destroying) return NULL;
    limits *s=Tcl_GetAssocData(i,key,NULL);
    if(!s) {
        s=calloc(1,sizeof *s);
        if(!s) Tcl_Panic("cannot allocate evaluation limits");
        s->granularity[0]=1; s->granularity[1]=10;
        Tcl_SetAssocData(i,key,dispose_limits,s);
    }
    return s;
}
static int index_of(int type) {
    if(type==TKGPU_LIMIT_COMMANDS) return 0;
    if(type==TKGPU_LIMIT_TIME) return 1;
    Tcl_Panic("invalid limit type %d",type);
}
void Tcl_LimitAddHandler(Tcl_Interp *i,int type,Tcl_LimitHandlerProc *proc,
                         void *data,Tcl_LimitHandlerDeleteProc *dispose) {
    (void)index_of(type);
    limits *s=state(i);
    if(!s||!proc) return;
    limit_handler *h=calloc(1,sizeof *h);
    if(!h) Tcl_Panic("cannot allocate limit handler");
    h->type=type; h->proc=proc; h->data=data; h->dispose=dispose;
    h->refs=1; h->next=s->handlers; s->handlers=h;
}
void Tcl_LimitRemoveHandler(Tcl_Interp *i,int type,Tcl_LimitHandlerProc *proc,void *data) {
    (void)index_of(type);
    limits *s=state(i);
    if(!s) return;
    limit_handler **link=&s->handlers;
    while(*link) {
        limit_handler *h=*link;
        if(h->type==type&&h->proc==proc&&h->data==data) {
            *link=h->next; h->removed=1; release_handler(h); return;
        }
        link=&h->next;
    }
}
int Tcl_LimitGetCommands(Tcl_Interp *i) { limits *s=state(i); return s?s->command_limit:0; }
void Tcl_LimitSetCommands(Tcl_Interp *i,int n) { limits *s=state(i); if(s) { s->command_limit=n; s->exceeded&=~1u; } }
void Tcl_LimitGetTime(Tcl_Interp *i,Tcl_Time *time) { limits *s=state(i); if(s&&time) *time=s->time; }
void Tcl_LimitSetTime(Tcl_Interp *i,Tcl_Time *time) { limits *s=state(i); if(s&&time) { s->time=*time; s->exceeded&=~2u; } }
int Tcl_LimitGetGranularity(Tcl_Interp *i,int type) {
    int k=index_of(type); limits *s=state(i); return s?s->granularity[k]:0;
}
void Tcl_LimitSetGranularity(Tcl_Interp *i,int type,int n) {
    int k=index_of(type);
    if(n<1) Tcl_Panic("limit granularity must be positive");
    limits *s=state(i); if(s) s->granularity[k]=n;
}
int Tcl_LimitTypeEnabled(Tcl_Interp *i,int type) {
    (void)index_of(type); limits *s=state(i); return s&&(s->enabled&(unsigned)type)!=0;
}
int Tcl_LimitTypeExceeded(Tcl_Interp *i,int type) {
    (void)index_of(type); limits *s=state(i); return s&&(s->exceeded&(unsigned)type)!=0;
}
void Tcl_LimitTypeSet(Tcl_Interp *i,int type) {
    (void)index_of(type); limits *s=state(i); if(s) s->enabled|=(unsigned)type;
}
void Tcl_LimitTypeReset(Tcl_Interp *i,int type) {
    (void)index_of(type); limits *s=state(i);
    if(s) { s->enabled&=~(unsigned)type; s->exceeded&=~(unsigned)type; }
}
int Tcl_LimitExceeded(Tcl_Interp *i) { limits *s=state(i); return s&&s->exceeded!=0; }
int Tcl_LimitReady(Tcl_Interp *i) {
    limits *s=state(i);
    if(!s) return 0;
    return ((s->enabled&1)&&s->commands%(unsigned)s->granularity[0]==0)||
           ((s->enabled&2)&&s->commands%(unsigned)s->granularity[1]==0);
}
static int over(limits *s,int type) {
    if(!(s->enabled&(unsigned)type)) return 0;
    if(type==1) return s->command_limit<0||s->commands>(uint64_t)s->command_limit;
    Tcl_Time now; Tcl_GetTime(&now);
    return now.sec>s->time.sec||(now.sec==s->time.sec&&now.usec>s->time.usec);
}
static void handlers(Tcl_Interp *i,limits *s,int type) {
    size_t count=0;
    for(limit_handler *h=s->handlers;h;h=h->next) if(h->type==type) ++count;
    if(!count) return;
    limit_handler **snapshot=malloc(count*sizeof *snapshot);
    if(!snapshot) Tcl_Panic("cannot snapshot limit handlers");
    size_t k=0;
    for(limit_handler *h=s->handlers;h;h=h->next) if(h->type==type) {
        ++h->refs; snapshot[k++]=h;
    }
    for(k=0;k<count;k++) {
        limit_handler *h=snapshot[k];
        if(!h->removed&&!i->deleted) h->proc(h->data,i);
        release_handler(h);
    }
    free(snapshot);
}
static int limit_check(Tcl_Interp *i) {
    limits *s=state(i);
    if(!s||s->checking) return TG_OK;
    s->checking=1;
    for(int type=1;type<=2;type++) if(over(s,type)) {
        handlers(i,s,type);
        if(i->deleted) { s->checking=0; return TG_ERROR; }
        if(over(s,type)) {
            s->exceeded|=(unsigned)type;
            tg_set_result(i,type==1?"command count limit exceeded":"time limit exceeded",TG_ERROR);
            Tcl_SetErrorCode(i,"TCL","LIMIT",type==1?"COMMANDS":"TIME",(char *)NULL);
            break;
        }
    }
    s->checking=0;
    return s->exceeded?TG_ERROR:TG_OK;
}
int Tcl_LimitCheck(Tcl_Interp *i) {
    if(!i) return TG_OK;
#ifdef TKGPU_REFERENCE_STUBS
    tg_compat_lifetime_enter(i);
#endif
    ++i->assoc_callback_depth;
    int rc=limit_check(i);
    --i->assoc_callback_depth;
#ifdef TKGPU_REFERENCE_STUBS
    tg_compat_lifetime_leave(i);
#endif
    return rc;
}
int tg_compat_cancel_check(Tcl_Interp *i);
int tg_compat_eval_check(Tcl_Interp *i) {
    if(tg_compat_cancel_check(i)!=TG_OK) return TG_ERROR;
    limits *s=state(i);
    if(!s) return TG_ERROR;
    if(i->deleted||s->exceeded) return TG_ERROR;
    if(s->commands!=UINT64_MAX) ++s->commands;
    return Tcl_LimitReady(i)?Tcl_LimitCheck(i):TG_OK;
}
