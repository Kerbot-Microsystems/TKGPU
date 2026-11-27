#include "TKGPUCompat.H"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>
#else
#include <pthread.h>
#include <sys/wait.h>
#endif

typedef struct thread_rec {
    Tcl_ThreadId id;
#ifdef _WIN32
    HANDLE handle;
#else
    pthread_t handle;
#endif
    int joinable;
    struct thread_rec *next;
} thread_rec;

typedef struct {
    Tcl_ThreadCreateProc *proc;
    void *data;
} thread_start;

static Tcl_Mutex thread_list_mutex;
static thread_rec *thread_list;

#ifdef _WIN32
static unsigned __stdcall thread_trampoline(void *arg) {
    thread_start *s=arg;
    Tcl_ThreadCreateProc *proc=s->proc;
    void *data=s->data;
    free(s);
    unsigned rc=proc(data);
    Tcl_FinalizeThread();
    return rc;
}
#else
static void *thread_trampoline(void *arg) {
    thread_start *s=arg;
    Tcl_ThreadCreateProc *proc=s->proc;
    void *data=s->data;
    free(s);
    proc(data);
    Tcl_FinalizeThread();
    return NULL;
}
#endif

int Tcl_CreateThread(Tcl_ThreadId *idPtr,Tcl_ThreadCreateProc *proc,void *clientData,int stackSize,int flags) {
    if(!proc) return 1;
    thread_start *s=calloc(1,sizeof *s);
    if(!s) return 1;
    s->proc=proc; s->data=clientData;
    int joinable=(flags&1)!=0;
#ifdef _WIN32
    unsigned tid=0;
    HANDLE h=(HANDLE)_beginthreadex(NULL,(unsigned)(stackSize>0?stackSize:0),thread_trampoline,s,0,&tid);
    if(!h) { free(s); return 1; }
    Tcl_ThreadId id=(Tcl_ThreadId)(uintptr_t)tid;
    if(idPtr) *idPtr=id;
    if(joinable) {
        thread_rec *r=calloc(1,sizeof *r);
        if(!r) { CloseHandle(h); return 1; }
        r->id=id; r->handle=h; r->joinable=1;
        Tcl_MutexLock(&thread_list_mutex);
        r->next=thread_list; thread_list=r;
        Tcl_MutexUnlock(&thread_list_mutex);
    } else {
        CloseHandle(h);
    }
    return 0;
#else
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    if(stackSize>0) pthread_attr_setstacksize(&attr,(size_t)stackSize);
    if(!joinable) pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
    pthread_t pt;
    int rc=pthread_create(&pt,&attr,thread_trampoline,s);
    pthread_attr_destroy(&attr);
    if(rc!=0) { free(s); return 1; }
    Tcl_ThreadId id=(Tcl_ThreadId)(uintptr_t)pt;
    if(idPtr) *idPtr=id;
    if(joinable) {
        thread_rec *r=calloc(1,sizeof *r);
        if(!r) return 1;
        r->id=id; r->handle=pt; r->joinable=1;
        Tcl_MutexLock(&thread_list_mutex);
        r->next=thread_list; thread_list=r;
        Tcl_MutexUnlock(&thread_list_mutex);
    }
    return 0;
#endif
}

int Tcl_JoinThread(Tcl_ThreadId id,int *result) {
    Tcl_MutexLock(&thread_list_mutex);
    thread_rec **link=&thread_list,*found=NULL;
    while(*link) {
        if((*link)->id==id) { found=*link; *link=found->next; break; }
        link=&(*link)->next;
    }
    Tcl_MutexUnlock(&thread_list_mutex);
    if(!found) return 1;
#ifdef _WIN32
    DWORD code=0;
    WaitForSingleObject(found->handle,INFINITE);
    GetExitCodeThread(found->handle,&code);
    CloseHandle(found->handle);
    free(found);
    if(result) *result=(int)code;
    return 0;
#else
    void *ret=NULL;
    int rc=pthread_join(found->handle,&ret);
    free(found);
    if(rc!=0) return 1;
    if(result) *result=(int)(intptr_t)ret;
    return 0;
#endif
}

static Tcl_NotifierProcs custom_notifier;
static int has_custom_notifier;

void Tcl_SetNotifier(Tcl_NotifierProcs *procs) {
    if(procs) { custom_notifier=*procs; has_custom_notifier=1; }
    else { memset(&custom_notifier,0,sizeof custom_notifier); has_custom_notifier=0; }
}

typedef struct pid_node {
    Tcl_Pid pid;
    struct pid_node *next;
} pid_node;

static Tcl_Mutex pid_mutex;
static pid_node *detached_pids;

void Tcl_DetachPids(int numPids,Tcl_Pid *pidPtr) {
    if(numPids<=0||!pidPtr) return;
    Tcl_MutexLock(&pid_mutex);
    for(int k=0;k<numPids;k++) {
        pid_node *n=calloc(1,sizeof *n);
        if(n) { n->pid=pidPtr[k]; n->next=detached_pids; detached_pids=n; }
    }
    Tcl_MutexUnlock(&pid_mutex);
}

Tcl_Pid Tcl_WaitPid(Tcl_Pid pid,int *statPtr,int options) {
    if(statPtr) *statPtr=0;
#ifdef _WIN32
    HANDLE h=(HANDLE)pid;
    if(!h) return (Tcl_Pid)(intptr_t)-1;
    DWORD timeout=(options&1)?0:INFINITE;
    DWORD w=WaitForSingleObject(h,timeout);
    if(w==WAIT_TIMEOUT) return (Tcl_Pid)0;
    if(w!=WAIT_OBJECT_0) return (Tcl_Pid)(intptr_t)-1;
    DWORD code=0;
    if(GetExitCodeProcess(h,&code)&&statPtr) *statPtr=(int)code;
    return pid;
#else
    int status=0;
    pid_t r=waitpid((pid_t)(intptr_t)pid,&status,options);
    if(statPtr) *statPtr=status;
    return (Tcl_Pid)(intptr_t)r;
#endif
}

void Tcl_ReapDetachedProcs(void) {
    Tcl_MutexLock(&pid_mutex);
    pid_node **link=&detached_pids;
    while(*link) {
        pid_node *n=*link;
        int st=0;
        Tcl_Pid r=Tcl_WaitPid(n->pid,&st,1);
        if(r!=(Tcl_Pid)0) {
            *link=n->next;
            free(n);
        } else {
            link=&n->next;
        }
    }
    Tcl_MutexUnlock(&pid_mutex);
}
