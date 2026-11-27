/* Remaining 69 Tcl 8.6 Channel I/O slots (Linux reference headers). */
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct chan_handler {
    int mask;
    Tcl_ChannelProc *proc;
    ClientData data;
    struct chan_handler *next;
} chan_handler;

typedef struct close_handler {
    Tcl_CloseProc *proc;
    ClientData data;
    struct close_handler *next;
} close_handler;

struct Tcl_Channel_ {
    const Tcl_ChannelType *type;
    ClientData instance;
    char *name;
    int mode;
    int buf_size;
    int eof;
    int blocked;
    int ref_count;
    int std_type;
    Tcl_ThreadId thread;
    Tcl_Obj *error_msg;
    struct Tcl_Channel_ *down;
    struct Tcl_Channel_ *up;
    char *in_buf;
    size_t in_len,in_cap;
    char *out_buf;
    size_t out_len,out_cap;
    chan_handler *handlers;
    close_handler *close_handlers;
    struct Tcl_Channel_ *global_next;
};

typedef struct chan_reg_entry {
    struct Tcl_Channel_ *chan;
    struct chan_reg_entry *next;
} chan_reg_entry;

typedef struct {
    chan_reg_entry *head;
} chan_interp_table;

static Tcl_Mutex chan_global_mutex;
static struct Tcl_Channel_ *chan_global_list;
static struct Tcl_Channel_ *std_channels[3];
static unsigned long chan_id_counter;
static const char chan_assoc_key[]="TKGPU::channels";

static struct Tcl_Channel_ *chan_top(struct Tcl_Channel_ *c) {
    while(c&&c->up) c=c->up;
    return c;
}

static void dispose_chan_table(ClientData data,Tcl_Interp *i) {
    chan_interp_table *t=(chan_interp_table *)data;
    while(t->head) {
        chan_reg_entry *e=t->head;
        t->head=e->next;
        struct Tcl_Channel_ *c=e->chan;
        Tcl_Free((char *)e);
        if(c&&c->ref_count>0) {
            --c->ref_count;
            if(c->ref_count==0&&c->std_type<0) (void)Tcl_Close(i,c);
        }
    }
    Tcl_Free((char *)t);
}

static chan_interp_table *get_chan_table(Tcl_Interp *i,int create) {
    if(!i||Tcl_InterpDeleted(i)) return NULL;
    chan_interp_table *t=(chan_interp_table *)Tcl_GetAssocData(i,chan_assoc_key,NULL);
    if(!t&&create) {
        t=(chan_interp_table *)Tcl_Alloc((unsigned)sizeof *t);
        t->head=NULL;
        Tcl_SetAssocData(i,chan_assoc_key,dispose_chan_table,t);
    }
    return t;
}

static void chan_in_ensure(struct Tcl_Channel_ *c,size_t need) {
    if(c->in_len+need<=c->in_cap) return;
    size_t next=c->in_cap?c->in_cap*2:256;
    while(next<c->in_len+need) next*=2;
    c->in_buf=Tcl_Realloc(c->in_buf,(unsigned)next);
    c->in_cap=next;
}

Tcl_Channel Tcl_CreateChannel(const Tcl_ChannelType *typePtr,const char *chanName,
                              ClientData instanceData,int mask) {
    if(!typePtr) return NULL;
    struct Tcl_Channel_ *c=(struct Tcl_Channel_ *)Tcl_Alloc((unsigned)sizeof *c);
    memset(c,0,sizeof *c);
    c->type=typePtr;
    c->instance=instanceData;
    c->mode=mask;
    c->buf_size=4096;
    c->std_type=-1;
    c->thread=Tcl_GetCurrentThread();
    const char *nm=chanName?chanName:"chan";
    size_t n=strlen(nm);
    c->name=Tcl_Alloc((unsigned)(n+1));
    memcpy(c->name,nm,n+1);
    Tcl_MutexLock(&chan_global_mutex);
    c->global_next=chan_global_list;
    chan_global_list=c;
    Tcl_MutexUnlock(&chan_global_mutex);
    return c;
}

void Tcl_RegisterChannel(Tcl_Interp *i,Tcl_Channel chan) {
    if(!chan) return;
    struct Tcl_Channel_ *c=chan_top(chan);
    if(!i) { ++c->ref_count; return; }
    chan_interp_table *t=get_chan_table(i,1);
    if(!t) return;
    for(chan_reg_entry *e=t->head;e;e=e->next) {
        if(e->chan==c) return;
    }
    chan_reg_entry *e=(chan_reg_entry *)Tcl_Alloc((unsigned)sizeof *e);
    e->chan=c; e->next=t->head; t->head=e;
    ++c->ref_count;
}

int Tcl_DetachChannel(Tcl_Interp *i,Tcl_Channel chan) {
    if(!chan) return TCL_ERROR;
    struct Tcl_Channel_ *c=chan_top(chan);
    if(!i) return TCL_OK;
    chan_interp_table *t=get_chan_table(i,0);
    if(!t) return TCL_ERROR;
    chan_reg_entry **link=&t->head;
    while(*link) {
        chan_reg_entry *e=*link;
        if(e->chan==c) {
            *link=e->next;
            Tcl_Free((char *)e);
            if(c->ref_count>0) --c->ref_count;
            return TCL_OK;
        }
        link=&e->next;
    }
    return TCL_ERROR;
}

int Tcl_UnregisterChannel(Tcl_Interp *i,Tcl_Channel chan) {
    if(!chan) return TCL_ERROR;
    struct Tcl_Channel_ *c=chan_top(chan);
    if(i) {
        if(Tcl_DetachChannel(i,c)!=TCL_OK) return TCL_ERROR;
    } else if(c->ref_count>0) {
        --c->ref_count;
    }
    if(c->ref_count==0&&c->std_type<0) return Tcl_Close(i,c);
    return TCL_OK;
}

void Tcl_CutChannel(Tcl_Channel chan) {
    if(!chan) return;
    Tcl_MutexLock(&chan_global_mutex);
    struct Tcl_Channel_ **link=&chan_global_list;
    while(*link) {
        if(*link==chan) { *link=chan->global_next; chan->global_next=NULL; break; }
        link=&(*link)->global_next;
    }
    Tcl_MutexUnlock(&chan_global_mutex);
}

void Tcl_SpliceChannel(Tcl_Channel chan) {
    if(!chan) return;
    chan->thread=Tcl_GetCurrentThread();
    Tcl_MutexLock(&chan_global_mutex);
    for(struct Tcl_Channel_ *p=chan_global_list;p;p=p->global_next) {
        if(p==chan) { Tcl_MutexUnlock(&chan_global_mutex); return; }
    }
    chan->global_next=chan_global_list;
    chan_global_list=chan;
    Tcl_MutexUnlock(&chan_global_mutex);
}

Tcl_Channel Tcl_GetChannel(Tcl_Interp *i,const char *chanName,int *modePtr) {
    if(!chanName) return NULL;
    if(!strcmp(chanName,"stdin")) {
        Tcl_Channel c=Tcl_GetStdChannel(TCL_STDIN);
        if(c&&modePtr) *modePtr=c->mode;
        return c;
    }
    if(!strcmp(chanName,"stdout")) {
        Tcl_Channel c=Tcl_GetStdChannel(TCL_STDOUT);
        if(c&&modePtr) *modePtr=c->mode;
        return c;
    }
    if(!strcmp(chanName,"stderr")) {
        Tcl_Channel c=Tcl_GetStdChannel(TCL_STDERR);
        if(c&&modePtr) *modePtr=c->mode;
        return c;
    }
    chan_interp_table *t=get_chan_table(i,0);
    if(t) {
        for(chan_reg_entry *e=t->head;e;e=e->next) {
            if(e->chan&&!strcmp(e->chan->name,chanName)) {
                if(modePtr) *modePtr=e->chan->mode;
                return e->chan;
            }
        }
    }
    if(i) Tcl_SetObjResult(i,Tcl_ObjPrintf("can not find channel named \"%s\"",chanName));
    return NULL;
}

int Tcl_GetChannelNamesEx(Tcl_Interp *i,const char *pattern) {
    if(!i) return TCL_ERROR;
    Tcl_Obj *list=Tcl_NewListObj(0,NULL);
    Tcl_IncrRefCount(list);
    static const char *const stdNames[3]={"stdin","stdout","stderr"};
    for(int k=0;k<3;k++) {
        if(!pattern||Tcl_StringMatch(stdNames[k],pattern)) {
            Tcl_ListObjAppendElement(i,list,Tcl_NewStringObj(stdNames[k],-1));
        }
    }
    chan_interp_table *t=get_chan_table(i,0);
    if(t) {
        for(chan_reg_entry *e=t->head;e;e=e->next) {
            if(e->chan&&(!pattern||Tcl_StringMatch(e->chan->name,pattern))) {
                Tcl_ListObjAppendElement(i,list,Tcl_NewStringObj(e->chan->name,-1));
            }
        }
    }
    Tcl_SetObjResult(i,list);
    Tcl_DecrRefCount(list);
    return TCL_OK;
}

int Tcl_GetChannelNames(Tcl_Interp *i) { return Tcl_GetChannelNamesEx(i,NULL); }

int Tcl_IsChannelExisting(const char *chanName) {
    if(!chanName) return 0;
    if(!strcmp(chanName,"stdin")||!strcmp(chanName,"stdout")||!strcmp(chanName,"stderr")) return 1;
    Tcl_MutexLock(&chan_global_mutex);
    int found=0;
    for(struct Tcl_Channel_ *c=chan_global_list;c;c=c->global_next) {
        if(c->name&&!strcmp(c->name,chanName)) { found=1; break; }
    }
    Tcl_MutexUnlock(&chan_global_mutex);
    return found;
}

int Tcl_IsChannelRegistered(Tcl_Interp *i,Tcl_Channel chan) {
    if(!chan) return 0;
    if(chan->std_type>=0) return 1;
    chan_interp_table *t=get_chan_table(i,0);
    if(!t) return 0;
    struct Tcl_Channel_ *top=chan_top(chan);
    for(chan_reg_entry *e=t->head;e;e=e->next) {
        if(e->chan==top) return 1;
    }
    return 0;
}

int Tcl_IsChannelShared(Tcl_Channel chan) {
    struct Tcl_Channel_ *c=chan_top(chan);
    return c&&c->ref_count>1;
}

int Tcl_IsStandardChannel(Tcl_Channel chan) {
    return chan&&chan->std_type>=0;
}

CONST84_RETURN char *Tcl_GetChannelName(Tcl_Channel chan) { return chan?chan->name:""; }
CONST86 Tcl_ChannelType *Tcl_GetChannelType(Tcl_Channel chan) { return chan?chan->type:NULL; }
ClientData Tcl_GetChannelInstanceData(Tcl_Channel chan) { return chan?chan->instance:NULL; }
int Tcl_GetChannelMode(Tcl_Channel chan) { return chan?chan->mode:0; }
Tcl_ThreadId Tcl_GetChannelThread(Tcl_Channel chan) { return chan?chan->thread:NULL; }
int Tcl_GetChannelBufferSize(Tcl_Channel chan) { return chan?chan->buf_size:0; }
void Tcl_SetChannelBufferSize(Tcl_Channel chan,int sz) {
    if(!chan) return;
    if(sz<10) sz=10;
    if(sz>1024*1024) sz=1024*1024;
    chan->buf_size=sz;
}

int Tcl_GetChannelHandle(Tcl_Channel chan,int direction,ClientData *handlePtr) {
    if(!chan||!chan->type) return TCL_ERROR;
    Tcl_DriverGetHandleProc *fn=Tcl_ChannelGetHandleProc(chan->type);
    if(!fn) return TCL_ERROR;
    return fn(chan->instance,direction,handlePtr);
}

int Tcl_BadChannelOption(Tcl_Interp *i,const char *optionName,const char *optionList) {
    if(i) {
        Tcl_SetObjResult(i,Tcl_ObjPrintf("bad option \"%s\": should be one of -blocking, -buffering, -buffersize, -encoding, -eofchar, or -translation%s%s",
            optionName?optionName:"",optionList&&*optionList?", ":"",optionList?optionList:""));
    }
    return TCL_ERROR;
}

int Tcl_GetChannelOption(Tcl_Interp *i,Tcl_Channel chan,const char *optionName,Tcl_DString *dsPtr) {
    if(!chan||!dsPtr) return TCL_ERROR;
    if(!optionName||!strcmp(optionName,"-buffersize")) {
        char buf[32]; snprintf(buf,sizeof buf,"%d",chan->buf_size);
        if(!optionName) Tcl_DStringAppendElement(dsPtr,"-buffersize");
        Tcl_DStringAppendElement(dsPtr,buf);
        if(optionName) return TCL_OK;
    }
    if(!optionName||!strcmp(optionName,"-blocking")) {
        if(!optionName) Tcl_DStringAppendElement(dsPtr,"-blocking");
        Tcl_DStringAppendElement(dsPtr,"1");
        if(optionName) return TCL_OK;
    }
    if(!optionName||!strcmp(optionName,"-buffering")) {
        if(!optionName) Tcl_DStringAppendElement(dsPtr,"-buffering");
        Tcl_DStringAppendElement(dsPtr,"full");
        if(optionName) return TCL_OK;
    }
    if(!optionName||!strcmp(optionName,"-encoding")) {
        if(!optionName) Tcl_DStringAppendElement(dsPtr,"-encoding");
        Tcl_DStringAppendElement(dsPtr,"utf-8");
        if(optionName) return TCL_OK;
    }
    if(!optionName) return TCL_OK;
    Tcl_DriverGetOptionProc *fn=Tcl_ChannelGetOptionProc(chan->type);
    if(fn) return fn(chan->instance,i,optionName,dsPtr);
    return Tcl_BadChannelOption(i,optionName,NULL);
}

int Tcl_SetChannelOption(Tcl_Interp *i,Tcl_Channel chan,const char *optionName,const char *newValue) {
    if(!chan||!optionName) return TCL_ERROR;
    if(!strcmp(optionName,"-buffersize")) {
        int sz=0;
        if(Tcl_GetInt(i,newValue,&sz)!=TCL_OK) return TCL_ERROR;
        Tcl_SetChannelBufferSize(chan,sz);
        return TCL_OK;
    }
    if(!strcmp(optionName,"-blocking")||!strcmp(optionName,"-buffering")||
       !strcmp(optionName,"-encoding")||!strcmp(optionName,"-translation")||
       !strcmp(optionName,"-eofchar")) {
        return TCL_OK;
    }
    Tcl_DriverSetOptionProc *fn=Tcl_ChannelSetOptionProc(chan->type);
    if(fn) return fn(chan->instance,i,optionName,newValue);
    return Tcl_BadChannelOption(i,optionName,NULL);
}

void Tcl_SetChannelError(Tcl_Channel chan,Tcl_Obj *msg) {
    if(!chan) return;
    if(msg) Tcl_IncrRefCount(msg);
    if(chan->error_msg) Tcl_DecrRefCount(chan->error_msg);
    chan->error_msg=msg;
}

void Tcl_GetChannelError(Tcl_Channel chan,Tcl_Obj **msg) {
    if(!msg) return;
    if(!chan) { *msg=NULL; return; }
    *msg=chan->error_msg;
    chan->error_msg=NULL;
}

void Tcl_CreateChannelHandler(Tcl_Channel chan,int mask,Tcl_ChannelProc *proc,ClientData clientData) {
    if(!chan||!proc) return;
    for(chan_handler *h=chan->handlers;h;h=h->next) {
        if(h->proc==proc&&h->data==clientData) { h->mask=mask; return; }
    }
    chan_handler *h=(chan_handler *)Tcl_Alloc((unsigned)sizeof *h);
    h->mask=mask; h->proc=proc; h->data=clientData; h->next=chan->handlers;
    chan->handlers=h;
}

void Tcl_DeleteChannelHandler(Tcl_Channel chan,Tcl_ChannelProc *proc,ClientData clientData) {
    if(!chan) return;
    chan_handler **link=&chan->handlers;
    while(*link) {
        chan_handler *h=*link;
        if(h->proc==proc&&h->data==clientData) {
            *link=h->next; Tcl_Free((char *)h); return;
        }
        link=&h->next;
    }
}

void Tcl_ClearChannelHandlers(Tcl_Channel chan) {
    if(!chan) return;
    while(chan->handlers) {
        chan_handler *h=chan->handlers; chan->handlers=h->next; Tcl_Free((char *)h);
    }
}

void Tcl_NotifyChannel(Tcl_Channel chan,int mask) {
    if(!chan) return;
    for(chan_handler *h=chan->handlers;h;h=h->next) {
        if((h->mask&mask)&&h->proc) h->proc(h->data,mask&h->mask);
    }
}

void Tcl_CreateCloseHandler(Tcl_Channel chan,Tcl_CloseProc *proc,ClientData clientData) {
    if(!chan||!proc) return;
    close_handler *h=(close_handler *)Tcl_Alloc((unsigned)sizeof *h);
    h->proc=proc; h->data=clientData; h->next=chan->close_handlers;
    chan->close_handlers=h;
}

void Tcl_DeleteCloseHandler(Tcl_Channel chan,Tcl_CloseProc *proc,ClientData clientData) {
    if(!chan) return;
    close_handler **link=&chan->close_handlers;
    while(*link) {
        close_handler *h=*link;
        if(h->proc==proc&&h->data==clientData) {
            *link=h->next; Tcl_Free((char *)h); return;
        }
        link=&h->next;
    }
}

Tcl_Channel Tcl_StackChannel(Tcl_Interp *i,const Tcl_ChannelType *typePtr,ClientData instanceData,
                             int mask,Tcl_Channel prevChan) {
    (void)i;
    if(!typePtr||!prevChan) return NULL;
    struct Tcl_Channel_ *top=chan_top(prevChan);
    Tcl_Channel layered=Tcl_CreateChannel(typePtr,top->name,instanceData,mask);
    if(!layered) return NULL;
    layered->down=top;
    top->up=layered;
    return layered;
}

int Tcl_UnstackChannel(Tcl_Interp *i,Tcl_Channel chan) {
    if(!chan||!chan->down) return TCL_ERROR;
    struct Tcl_Channel_ *under=chan->down;
    under->up=chan->up;
    if(chan->up) chan->up->down=under;
    chan->down=NULL; chan->up=NULL;
    return Tcl_Close(i,chan);
}

Tcl_Channel Tcl_GetStackedChannel(Tcl_Channel chan) { return chan?chan->down:NULL; }
Tcl_Channel Tcl_GetTopChannel(Tcl_Channel chan) { return chan_top(chan); }

int Tcl_Eof(Tcl_Channel chan) { return chan?chan->eof:1; }
int Tcl_InputBlocked(Tcl_Channel chan) { return chan?chan->blocked:0; }
int Tcl_InputBuffered(Tcl_Channel chan) { return chan?(int)chan->in_len:0; }
int Tcl_OutputBuffered(Tcl_Channel chan) { return chan?(int)chan->out_len:0; }
int Tcl_ChannelBuffered(Tcl_Channel chan) { return Tcl_InputBuffered(chan); }

int Tcl_ReadRaw(Tcl_Channel chan,char *dst,int bytesToRead) {
    if(!chan||!dst||bytesToRead<0) return -1;
    Tcl_DriverInputProc *fn=Tcl_ChannelInputProc(chan->type);
    if(!fn) return -1;
    int err=0;
    int n=fn(chan->instance,dst,bytesToRead,&err);
    if(n==0) chan->eof=1;
    if(n<0) { Tcl_SetErrno(err); if(err==EAGAIN||err==EWOULDBLOCK) chan->blocked=1; }
    else chan->blocked=0;
    return n;
}

int Tcl_WriteRaw(Tcl_Channel chan,const char *src,int srcLen) {
    if(!chan||!src) return -1;
    if(srcLen<0) srcLen=(int)strlen(src);
    Tcl_DriverOutputProc *fn=Tcl_ChannelOutputProc(chan->type);
    if(!fn) return -1;
    int err=0;
    int n=fn(chan->instance,src,srcLen,&err);
    if(n<0) Tcl_SetErrno(err);
    return n;
}

int Tcl_Read(Tcl_Channel chan,char *bufPtr,int toRead) {
    if(!chan||!bufPtr||toRead<0) return -1;
    int total=0;
    if(chan->in_len>0) {
        size_t take=(size_t)toRead<chan->in_len?(size_t)toRead:chan->in_len;
        memcpy(bufPtr,chan->in_buf,take);
        memmove(chan->in_buf,chan->in_buf+take,chan->in_len-take);
        chan->in_len-=take;
        total+=(int)take;
    }
    if(total<toRead) {
        int n=Tcl_ReadRaw(chan,bufPtr+total,toRead-total);
        if(n<0) return total>0?total:-1;
        total+=n;
    }
    return total;
}

int Tcl_ReadChars(Tcl_Channel chan,Tcl_Obj *objPtr,int charsToRead,int appendFlag) {
    if(!chan||!objPtr) return -1;
    if(!appendFlag) Tcl_SetStringObj(objPtr,"",0);
    int total=0;
    char buf[1024];
    for(;;) {
        int want=(charsToRead<0)?(int)sizeof buf:(charsToRead-total<(int)sizeof buf?charsToRead-total:(int)sizeof buf);
        if(want<=0) break;
        int n=Tcl_Read(chan,buf,want);
        if(n<0) return total>0?total:-1;
        if(n==0) break;
        Tcl_AppendToObj(objPtr,buf,n);
        total+=n;
        if(charsToRead>=0&&total>=charsToRead) break;
    }
    return total;
}

int Tcl_GetsObj(Tcl_Channel chan,Tcl_Obj *objPtr) {
    if(!chan||!objPtr) return -1;
    Tcl_SetStringObj(objPtr,"",0);
    int total=0;
    for(;;) {
        char ch=0;
        int n=Tcl_Read(chan,&ch,1);
        if(n<0) return -1;
        if(n==0) return total==0?-1:total;
        if(ch=='\n') return total;
        if(ch=='\r') continue;
        Tcl_AppendToObj(objPtr,&ch,1);
        ++total;
    }
}

int Tcl_Gets(Tcl_Channel chan,Tcl_DString *dsPtr) {
    if(!chan||!dsPtr) return -1;
    Tcl_Obj *tmp=Tcl_NewObj();
    Tcl_IncrRefCount(tmp);
    int n=Tcl_GetsObj(chan,tmp);
    if(n>=0) {
        int len=0; const char *s=Tcl_GetStringFromObj(tmp,&len);
        Tcl_DStringAppend(dsPtr,s,len);
    }
    Tcl_DecrRefCount(tmp);
    return n;
}

int Tcl_Ungets(Tcl_Channel chan,const char *str,int len,int atHead) {
    if(!chan||!str) return -1;
    if(len<0) len=(int)strlen(str);
    chan_in_ensure(chan,(size_t)len);
    if(atHead) {
        memmove(chan->in_buf+len,chan->in_buf,chan->in_len);
        memcpy(chan->in_buf,str,(size_t)len);
    } else {
        memcpy(chan->in_buf+chan->in_len,str,(size_t)len);
    }
    chan->in_len+=(size_t)len;
    chan->eof=0;
    return len;
}

int Tcl_Write(Tcl_Channel chan,const char *s,int slen) {
    return Tcl_WriteRaw(chan,s,slen);
}

int Tcl_WriteChars(Tcl_Channel chan,const char *src,int srcLen) {
    return Tcl_WriteRaw(chan,src,srcLen);
}

int Tcl_WriteObj(Tcl_Channel chan,Tcl_Obj *objPtr) {
    if(!chan||!objPtr) return -1;
    int len=0;
    const char *s=Tcl_GetStringFromObj(objPtr,&len);
    return Tcl_WriteRaw(chan,s,len);
}

int Tcl_Flush(Tcl_Channel chan) {
    if(!chan) return TCL_ERROR;
    Tcl_DriverFlushProc *fn=Tcl_ChannelFlushProc(chan->type);
    if(fn) return fn(chan->instance);
    return TCL_OK;
}

Tcl_WideInt Tcl_Seek(Tcl_Channel chan,Tcl_WideInt offset,int mode) {
    if(!chan) return -1;
    chan->in_len=0; chan->eof=0;
    Tcl_DriverWideSeekProc *wfn=Tcl_ChannelWideSeekProc(chan->type);
    int err=0;
    if(wfn) {
        Tcl_WideInt pos=wfn(chan->instance,offset,mode,&err);
        if(pos<0) Tcl_SetErrno(err);
        return pos;
    }
    Tcl_DriverSeekProc *sfn=Tcl_ChannelSeekProc(chan->type);
    if(sfn) {
        int pos=sfn(chan->instance,(long)offset,mode,&err);
        if(pos<0) Tcl_SetErrno(err);
        return (Tcl_WideInt)pos;
    }
    return -1;
}

int Tcl_SeekOld(Tcl_Channel chan,int offset,int mode) {
    return (int)Tcl_Seek(chan,(Tcl_WideInt)offset,mode);
}

Tcl_WideInt Tcl_Tell(Tcl_Channel chan) {
    return Tcl_Seek(chan,0,SEEK_CUR);
}

int Tcl_TellOld(Tcl_Channel chan) {
    return (int)Tcl_Tell(chan);
}

int Tcl_TruncateChannel(Tcl_Channel chan,Tcl_WideInt length) {
    if(!chan) return TCL_ERROR;
    Tcl_DriverTruncateProc *fn=Tcl_ChannelTruncateProc(chan->type);
    if(!fn) return TCL_ERROR;
    return fn(chan->instance,length);
}

int Tcl_CloseEx(Tcl_Interp *i,Tcl_Channel chan,int flags) {
    if(!chan) return TCL_ERROR;
    (void)Tcl_Flush(chan);
    while(chan->close_handlers) {
        close_handler *h=chan->close_handlers;
        chan->close_handlers=h->next;
        Tcl_CloseProc *proc=h->proc; ClientData d=h->data;
        Tcl_Free((char *)h);
        if(proc) proc(d);
    }
    Tcl_ClearChannelHandlers(chan);
    int rc=TCL_OK;
    Tcl_DriverClose2Proc *c2=Tcl_ChannelClose2Proc(chan->type);
    Tcl_DriverCloseProc *c1=Tcl_ChannelCloseProc(chan->type);
    if(c2&&c1==TCL_CLOSE2PROC) rc=c2(chan->instance,i,flags);
    else if(c1&&c1!=TCL_CLOSE2PROC) rc=c1(chan->instance,i);
    if(flags==0) {
        Tcl_CutChannel(chan);
        if(chan->error_msg) Tcl_DecrRefCount(chan->error_msg);
        if(chan->in_buf) Tcl_Free(chan->in_buf);
        if(chan->out_buf) Tcl_Free(chan->out_buf);
        if(chan->name) Tcl_Free(chan->name);
        Tcl_Free((char *)chan);
    }
    return rc==0?TCL_OK:TCL_ERROR;
}

int Tcl_Close(Tcl_Interp *i,Tcl_Channel chan) {
    return Tcl_CloseEx(i,chan,0);
}

/* --- Built-in POSIX fd driver --- */
static int fd_close(ClientData inst,Tcl_Interp *i) {
    (void)i;
    int fd=(int)(intptr_t)inst;
    if(fd>2) (void)close(fd);
    return 0;
}
static int fd_input(ClientData inst,char *buf,int toRead,int *errPtr) {
    ssize_t n=read((int)(intptr_t)inst,buf,(size_t)toRead);
    if(n<0) { *errPtr=errno; return -1; }
    return (int)n;
}
static int fd_output(ClientData inst,const char *buf,int toWrite,int *errPtr) {
    ssize_t n=write((int)(intptr_t)inst,buf,(size_t)toWrite);
    if(n<0) { *errPtr=errno; return -1; }
    return (int)n;
}
static Tcl_WideInt fd_wide_seek(ClientData inst,Tcl_WideInt offset,int mode,int *errPtr) {
    off_t r=lseek((int)(intptr_t)inst,(off_t)offset,mode);
    if(r==(off_t)-1) { *errPtr=errno; return -1; }
    return (Tcl_WideInt)r;
}
static int fd_handle(ClientData inst,int direction,ClientData *handlePtr) {
    (void)direction;
    if(handlePtr) *handlePtr=inst;
    return TCL_OK;
}
static int fd_truncate(ClientData inst,Tcl_WideInt length) {
    return ftruncate((int)(intptr_t)inst,(off_t)length)==0?TCL_OK:TCL_ERROR;
}
static void fd_watch(ClientData inst,int mask) { (void)inst; (void)mask; }

static const Tcl_ChannelType posix_fd_type={
    .typeName="file",
    .version=TCL_CHANNEL_VERSION_5,
    .closeProc=fd_close,
    .inputProc=fd_input,
    .outputProc=fd_output,
    .watchProc=fd_watch,
    .getHandleProc=fd_handle,
    .wideSeekProc=fd_wide_seek,
    .truncateProc=fd_truncate
};

Tcl_Channel Tcl_MakeFileChannel(ClientData handle,int mode) {
    char name[32];
    snprintf(name,sizeof name,"file%lu",++chan_id_counter);
    return Tcl_CreateChannel(&posix_fd_type,name,handle,mode);
}

Tcl_Channel Tcl_GetStdChannel(int type) {
    int idx=(type==TCL_STDIN)?0:(type==TCL_STDOUT)?1:(type==TCL_STDERR)?2:-1;
    if(idx<0) return NULL;
    if(!std_channels[idx]) {
        static const char *const names[3]={"stdin","stdout","stderr"};
        int mode=(idx==0)?TCL_READABLE:TCL_WRITABLE;
        struct Tcl_Channel_ *c=Tcl_CreateChannel(&posix_fd_type,names[idx],(ClientData)(intptr_t)idx,mode);
        c->std_type=type;
        std_channels[idx]=c;
    }
    return std_channels[idx];
}

void Tcl_SetStdChannel(Tcl_Channel chan,int type) {
    int idx=(type==TCL_STDIN)?0:(type==TCL_STDOUT)?1:(type==TCL_STDERR)?2:-1;
    if(idx<0) return;
    std_channels[idx]=chan;
    if(chan) chan->std_type=type;
}

Tcl_Channel Tcl_OpenFileChannel(Tcl_Interp *i,const char *fileName,const char *modeString,int permissions) {
    if(!fileName) return NULL;
    int flags=O_RDONLY,mode=TCL_READABLE;
    if(modeString) {
        if(!strcmp(modeString,"r")) { flags=O_RDONLY; mode=TCL_READABLE; }
        else if(!strcmp(modeString,"r+")) { flags=O_RDWR; mode=TCL_READABLE|TCL_WRITABLE; }
        else if(!strcmp(modeString,"w")) { flags=O_WRONLY|O_CREAT|O_TRUNC; mode=TCL_WRITABLE; }
        else if(!strcmp(modeString,"w+")) { flags=O_RDWR|O_CREAT|O_TRUNC; mode=TCL_READABLE|TCL_WRITABLE; }
        else if(!strcmp(modeString,"a")) { flags=O_WRONLY|O_CREAT|O_APPEND; mode=TCL_WRITABLE; }
        else if(!strcmp(modeString,"a+")) { flags=O_RDWR|O_CREAT|O_APPEND; mode=TCL_READABLE|TCL_WRITABLE; }
    }
    int fd=open(fileName,flags,permissions?permissions:0666);
    if(fd<0) {
        Tcl_SetErrno(errno);
        if(i) Tcl_SetObjResult(i,Tcl_ObjPrintf("couldn't open \"%s\": %s",fileName,Tcl_PosixError(i)));
        return NULL;
    }
    Tcl_Channel c=Tcl_MakeFileChannel((ClientData)(intptr_t)fd,mode);
    if(i&&c) Tcl_RegisterChannel(i,c);
    return c;
}

int Tcl_CreatePipe(Tcl_Interp *i,Tcl_Channel *rchan,Tcl_Channel *wchan,int flags) {
    (void)flags;
    int fds[2];
    if(pipe(fds)!=0) {
        Tcl_SetErrno(errno);
        if(i) Tcl_SetObjResult(i,Tcl_ObjPrintf("couldn't create pipe: %s",Tcl_PosixError(i)));
        return TCL_ERROR;
    }
    Tcl_Channel rc=Tcl_MakeFileChannel((ClientData)(intptr_t)fds[0],TCL_READABLE);
    Tcl_Channel wc=Tcl_MakeFileChannel((ClientData)(intptr_t)fds[1],TCL_WRITABLE);
    if(i) { Tcl_RegisterChannel(i,rc); Tcl_RegisterChannel(i,wc); }
    if(rchan) *rchan=rc;
    if(wchan) *wchan=wc;
    return TCL_OK;
}

Tcl_Channel Tcl_OpenCommandChannel(Tcl_Interp *i,int argc,CONST84 char **argv,int flags) {
    if(argc<=0||!argv||!argv[0]) return NULL;
    Tcl_DString cmd; Tcl_DStringInit(&cmd);
    for(int k=0;k<argc;k++) {
        if(k) Tcl_DStringAppend(&cmd," ",1);
        Tcl_DStringAppend(&cmd,argv[k]?argv[k]:"",-1);
    }
    const char *pmode=(flags&TCL_WRITABLE)&&!(flags&TCL_READABLE)?"w":"r";
    FILE *fp=popen(Tcl_DStringValue(&cmd),pmode);
    Tcl_DStringFree(&cmd);
    if(!fp) {
        Tcl_SetErrno(errno);
        if(i) Tcl_SetObjResult(i,Tcl_ObjPrintf("couldn't execute \"%s\": %s",argv[0],Tcl_PosixError(i)));
        return NULL;
    }
    int fd=dup(fileno(fp));
    pclose(fp);
    if(fd<0) return NULL;
    Tcl_Channel c=Tcl_MakeFileChannel((ClientData)(intptr_t)fd,flags?flags:TCL_READABLE);
    if(i&&c) Tcl_RegisterChannel(i,c);
    return c;
}

Tcl_Channel Tcl_MakeTcpClientChannel(ClientData tcpSocket) {
    char name[32];
    snprintf(name,sizeof name,"sock%lu",++chan_id_counter);
    return Tcl_CreateChannel(&posix_fd_type,name,tcpSocket,TCL_READABLE|TCL_WRITABLE);
}

Tcl_Channel Tcl_OpenTcpClient(Tcl_Interp *i,int port,const char *address,const char *myaddr,int myport,int flags) {
    (void)myaddr; (void)myport; (void)flags;
    if(!address||port<=0) return NULL;
    struct addrinfo hints,*res=NULL;
    memset(&hints,0,sizeof hints);
    hints.ai_family=AF_UNSPEC;
    hints.ai_socktype=SOCK_STREAM;
    char portStr[16]; snprintf(portStr,sizeof portStr,"%d",port);
    if(getaddrinfo(address,portStr,&hints,&res)!=0||!res) {
        if(i) Tcl_SetObjResult(i,Tcl_ObjPrintf("couldn't open socket: host \"%s\" not found",address));
        return NULL;
    }
    int fd=socket(res->ai_family,res->ai_socktype,res->ai_protocol);
    if(fd<0||connect(fd,res->ai_addr,res->ai_addrlen)!=0) {
        int saved=errno;
        if(fd>=0) close(fd);
        freeaddrinfo(res);
        Tcl_SetErrno(saved);
        if(i) Tcl_SetObjResult(i,Tcl_ObjPrintf("couldn't open socket: %s",Tcl_PosixError(i)));
        return NULL;
    }
    freeaddrinfo(res);
    Tcl_Channel c=Tcl_MakeTcpClientChannel((ClientData)(intptr_t)fd);
    if(i&&c) Tcl_RegisterChannel(i,c);
    return c;
}

Tcl_Channel Tcl_OpenTcpServer(Tcl_Interp *i,int port,const char *host,Tcl_TcpAcceptProc *acceptProc,ClientData callbackData) {
    (void)acceptProc; (void)callbackData;
    struct addrinfo hints,*res=NULL;
    memset(&hints,0,sizeof hints);
    hints.ai_family=AF_INET;
    hints.ai_socktype=SOCK_STREAM;
    hints.ai_flags=AI_PASSIVE;
    char portStr[16]; snprintf(portStr,sizeof portStr,"%d",port);
    if(getaddrinfo(host,portStr,&hints,&res)!=0||!res) {
        if(i) Tcl_SetObjResult(i,Tcl_NewStringObj("couldn't resolve server address",-1));
        return NULL;
    }
    int fd=socket(res->ai_family,res->ai_socktype,res->ai_protocol);
    int opt=1;
    if(fd>=0) setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof opt);
    if(fd<0||bind(fd,res->ai_addr,res->ai_addrlen)!=0||listen(fd,16)!=0) {
        int saved=errno;
        if(fd>=0) close(fd);
        freeaddrinfo(res);
        Tcl_SetErrno(saved);
        if(i) Tcl_SetObjResult(i,Tcl_ObjPrintf("couldn't open server socket: %s",Tcl_PosixError(i)));
        return NULL;
    }
    freeaddrinfo(res);
    Tcl_Channel c=Tcl_MakeTcpClientChannel((ClientData)(intptr_t)fd);
    if(i&&c) Tcl_RegisterChannel(i,c);
    return c;
}

int Tcl_GetOpenFile(Tcl_Interp *i,const char *chanID,int forWriting,int checkUsage,ClientData *filePtr) {
    int mode=0;
    Tcl_Channel c=Tcl_GetChannel(i,chanID,&mode);
    if(!c) return TCL_ERROR;
    if(checkUsage) {
        if(forWriting&&!(mode&TCL_WRITABLE)) {
            if(i) Tcl_SetObjResult(i,Tcl_ObjPrintf("\"%s\" wasn't opened for writing",chanID));
            return TCL_ERROR;
        }
        if(!forWriting&&!(mode&TCL_READABLE)) {
            if(i) Tcl_SetObjResult(i,Tcl_ObjPrintf("\"%s\" wasn't opened for reading",chanID));
            return TCL_ERROR;
        }
    }
    ClientData h=NULL;
    if(Tcl_GetChannelHandle(c,forWriting?TCL_WRITABLE:TCL_READABLE,&h)!=TCL_OK) return TCL_ERROR;
    FILE *fp=fdopen(dup((int)(intptr_t)h),forWriting?"wb":"rb");
    if(!fp) return TCL_ERROR;
    if(filePtr) *filePtr=(ClientData)fp;
    else fclose(fp);
    return TCL_OK;
}
