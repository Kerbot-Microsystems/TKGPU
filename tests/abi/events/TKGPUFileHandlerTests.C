#define _POSIX_C_SOURCE 200809L
#include "TKGPUCompat.H"
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
static int fd[2],count,mask,bytes,timers;
static void receive(void *ctx,int flags) {
    int *n=ctx; char c;
    ++*n; mask=flags;
    if(read(fd[0],&c,1)==1) bytes+=c;
    Tcl_DeleteFileHandler(fd[0]);
}
static void timer(void *ctx) { (*(int *)ctx)++; }
static void *writer(void *ctx) {
    (void)ctx; struct timespec t={0,20000000}; nanosleep(&t,NULL);
    return write(fd[1],"B",1)==1?NULL:(void *)1;
}
int main(void) {
    CHECK(pipe(fd)==0);
    Tcl_CreateFileHandler(fd[0],TKGPU_READABLE,receive,&count);
    CHECK(!Tcl_DoOneEvent(TKGPU_FILE_EVENTS|TKGPU_DONT_WAIT));
    CHECK(write(fd[1],"A",1)==1);
    CHECK(!Tcl_DoOneEvent(TKGPU_TIMER_EVENTS|TKGPU_DONT_WAIT));
    CHECK(Tcl_CreateTimerHandler(0,timer,&timers));
    CHECK(Tcl_DoOneEvent(TKGPU_TIMER_EVENTS|TKGPU_DONT_WAIT)==1&&timers==1);
    CHECK(Tcl_DoOneEvent(TKGPU_FILE_EVENTS|TKGPU_DONT_WAIT)==1);
    CHECK(count==1&&bytes=='A'&&mask==TKGPU_READABLE);
    CHECK(!Tcl_DoOneEvent(TKGPU_FILE_EVENTS|TKGPU_DONT_WAIT));
    Tcl_CreateFileHandler(fd[0],TKGPU_READABLE,receive,&count);
    pthread_t t; CHECK(!pthread_create(&t,NULL,writer,NULL));
    CHECK(Tcl_DoOneEvent(TKGPU_FILE_EVENTS)==1);
    void *rc; CHECK(!pthread_join(t,&rc)&&!rc);
    CHECK(count==2&&bytes=='A'+'B');
    close(fd[0]); close(fd[1]); puts("file readiness, masks, callback deletion and blocking delivery passed"); return 0;
}
