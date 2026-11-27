#include "TKGPUCompat.H"
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
typedef struct { Tcl_Event base; int value; int required; } event;
static Tcl_ThreadId owner;
static int seen[16],count;
static int record(Tcl_Event *base,int flags) {
    event *e=(event *)base;
    if((flags&e->required)!=e->required) return 0;
    if(Tcl_GetCurrentThread()!=owner||count>=16) return 0;
    seen[count++]=e->value; return 1;
}
static Tcl_Event *make(int value,int required) {
    event *e=(event *)Tcl_AttemptAlloc(sizeof *e);
    if(!e) return NULL;
    e->base.proc=record; e->base.nextPtr=NULL;
    e->value=value; e->required=required; return &e->base;
}
#ifdef _WIN32
static DWORD WINAPI worker(LPVOID arg) {
#else
static void *worker(void *arg) {
#endif
    (void)arg;
    Tcl_ThreadQueueEvent(owner,make(5,0),TKGPU_QUEUE_TAIL);
    Tcl_ThreadAlert(owner);
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}
int main(void) {
    owner=Tcl_GetCurrentThread(); CHECK(owner);
    Tcl_QueueEvent(make(1,0),TKGPU_QUEUE_TAIL);
    Tcl_QueueEvent(make(2,0),TKGPU_QUEUE_MARK);
    Tcl_QueueEvent(make(3,0),TKGPU_QUEUE_MARK);
    Tcl_QueueEvent(make(4,TKGPU_FILE_EVENTS|TKGPU_DONT_WAIT),TKGPU_QUEUE_HEAD);
    CHECK(Tcl_DoOneEvent(TKGPU_TIMER_EVENTS|TKGPU_DONT_WAIT)==1&&seen[0]==2);
    CHECK(Tcl_DoOneEvent(TKGPU_DONT_WAIT)==1&&seen[1]==4);
    CHECK(Tcl_DoOneEvent(TKGPU_DONT_WAIT)==1&&seen[2]==3);
    CHECK(Tcl_DoOneEvent(TKGPU_DONT_WAIT)==1&&seen[3]==1);
    CHECK(!Tcl_DoOneEvent(TKGPU_DONT_WAIT));
#ifdef _WIN32
    HANDLE thread=CreateThread(NULL,0,worker,NULL,0,NULL); CHECK(thread);
#else
    pthread_t thread; CHECK(!pthread_create(&thread,NULL,worker,NULL));
#endif
    while(count<5) (void)Tcl_DoOneEvent(0);
#ifdef _WIN32
    CHECK(WaitForSingleObject(thread,5000)==WAIT_OBJECT_0); CloseHandle(thread);
#else
    CHECK(!pthread_join(thread,NULL));
#endif
    CHECK(seen[4]==5&&!Tcl_DoOneEvent(TKGPU_DONT_WAIT));
    puts("thread event order and delivery checks passed"); return 0;
}
