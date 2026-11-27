#include "TKGPUCompat.H"
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
static Tcl_Mutex mutex;
static Tcl_Condition cond;
static Tcl_ThreadDataKey key;
static Tcl_ThreadId worker_id;
static int ready,done,failed;
static void worker(void) {
    int *local=Tcl_GetThreadData(&key,sizeof(int));
    if(*local!=0) failed=1;
    *local=9;
    Tcl_MutexLock(&mutex);
    worker_id=Tcl_GetCurrentThread(); ready=1;
    Tcl_ConditionNotify(&cond);
    while(!done) Tcl_ConditionWait(&cond,&mutex,NULL);
    Tcl_MutexUnlock(&mutex);
}
#ifdef _WIN32
static DWORD WINAPI run(LPVOID arg) {
    (void)arg; worker();
#ifdef TKGPU_LEGACY_WINDOWS
    if(TKGPU_FinalizeLegacyThread()!=0) return 1;
#endif
    return 0;
}
#else
static void *run(void *arg) { (void)arg; worker(); return NULL; }
#endif
int main(void) {
    int *local=Tcl_GetThreadData(&key,sizeof(int)); CHECK(*local==0); *local=42;
    Tcl_ThreadId self=Tcl_GetCurrentThread(); CHECK(self);
    Tcl_MutexLock(&mutex);
#ifdef _WIN32
    HANDLE thread=CreateThread(NULL,0,run,NULL,0,NULL); CHECK(thread);
#else
    pthread_t thread; CHECK(pthread_create(&thread,NULL,run,NULL)==0);
#endif
    while(!ready) Tcl_ConditionWait(&cond,&mutex,NULL);
    CHECK(worker_id&&worker_id!=self&&*local==42);
    done=1; Tcl_ConditionNotify(&cond); Tcl_MutexUnlock(&mutex);
#ifdef _WIN32
    CHECK(WaitForSingleObject(thread,5000)==WAIT_OBJECT_0);
    DWORD exit_code; CHECK(GetExitCodeThread(thread,&exit_code)&&exit_code==0);
    CloseHandle(thread);
#else
    CHECK(pthread_join(thread,NULL)==0);
#endif
    CHECK(!failed&&*local==42&&Tcl_GetThreadData(&key,sizeof(int))==local);
    Tcl_MutexLock(&mutex);
    Tcl_Time wait={0,1000}; Tcl_ConditionWait(&cond,&mutex,&wait);
    Tcl_MutexUnlock(&mutex);
    Tcl_ConditionFinalize(&cond); Tcl_MutexFinalize(&mutex);
    CHECK(!cond&&!mutex);
#ifdef TKGPU_LEGACY_WINDOWS
    CHECK(TKGPU_FinalizeLegacyThread()==0);
    CHECK(*(int *)Tcl_GetThreadData(&key,sizeof(int))==0);
    CHECK(TKGPU_FinalizeLegacyThread()==0);
#endif
    puts("mutex, condition and TLS checks passed"); return 0;
}
