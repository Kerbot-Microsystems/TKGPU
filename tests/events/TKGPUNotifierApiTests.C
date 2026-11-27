#include "../abi/TKGPUFacadeTest.H"
#include <stdio.h>
#include <stdint.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
static int fired;
static void timer(void *data) { ++*(int *)data; }
#ifdef _WIN32
static DWORD WINAPI wake(void *token) {
    Tcl_Sleep(20); Tcl_AlertNotifier(token); return 0;
}
#else
static void *wake(void *token) {
    Tcl_Sleep(20); Tcl_AlertNotifier(token); return NULL;
}
#endif
int main(void) {
    TKGPU_TEST_PREPARE();
    void *token=Tcl_InitNotifier();
    CHECK(token&&Tcl_InitNotifier()==token);
    Tcl_Time zero={0,0},delay={0,20000};
    CHECK(Tcl_WaitForEvent(&zero)==0);
    Tcl_SetTimer(&delay);
    CHECK(Tcl_WaitForEvent(NULL)==0);
    Tcl_SetTimer(NULL);
    CHECK(Tcl_CreateTimerHandler(0,timer,&fired));
    CHECK(Tcl_WaitForEvent(&zero)==0&&!fired);
    CHECK(Tcl_DoOneEvent(TKGPU_TIMER_EVENTS|TKGPU_DONT_WAIT)==1&&fired==1);
#ifdef _WIN32
    HANDLE worker=CreateThread(NULL,0,wake,token,0,NULL); CHECK(worker);
#else
    pthread_t worker; CHECK(pthread_create(&worker,NULL,wake,token)==0);
#endif
    Tcl_Time before,after,maximum={2,0};
    Tcl_GetTime(&before);
    CHECK(Tcl_WaitForEvent(&maximum)==0);
    Tcl_GetTime(&after);
    int64_t us=((int64_t)after.sec-before.sec)*1000000+after.usec-before.usec;
    CHECK(us>=0&&us<1500000);
#ifdef _WIN32
    CHECK(WaitForSingleObject(worker,2000)==WAIT_OBJECT_0); CloseHandle(worker);
#else
    CHECK(pthread_join(worker,NULL)==0);
#endif
    for(int k=0;k<100;k++) {
        Tcl_FinalizeNotifier(token);
        token=Tcl_InitNotifier(); CHECK(token);
        CHECK(Tcl_WaitForEvent(&zero)==0);
    }
    Tcl_FinalizeNotifier(token);
    puts("Notifier lifecycle, timer wait, callback isolation and cross-thread alert passed");
    return 0;
}
