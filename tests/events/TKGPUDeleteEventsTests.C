#include "../abi/TKGPUFacadeTest.H"
#include <stdio.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
typedef struct { Tcl_Event base; int id; } event;
static int handled[16],count,saw_active,deleted;
static int dispatch(Tcl_Event *,int);
static void queue(int id,Tcl_QueuePosition position) {
    event *e=(event *)Tcl_Alloc(sizeof *e);
    e->base.proc=dispatch; e->base.nextPtr=NULL; e->id=id;
    Tcl_QueueEvent(&e->base,position);
}
static int all(Tcl_Event *base,void *data) {
    event *e=(event *)base; (void)data;
    if(e->id==1&&!e->base.proc) saw_active=1;
    ++deleted; return 1;
}
static int five(Tcl_Event *base,void *data) { (void)data; return ((event *)base)->id==5; }
static int nested(Tcl_Event *base,void *data) {
    (void)data;
    if(((event *)base)->id==4) Tcl_DeleteEvents(five,NULL);
    return 0;
}
static int dispatch(Tcl_Event *base,int flags) {
    (void)flags;
    int id=((event *)base)->id;
    handled[count++]=id;
    if(id==1) {
        Tcl_DeleteEvents(all,NULL);
        /* Reuse-sized allocation must not confuse the active event identity. */
        queue(9,TKGPU_QUEUE_TAIL);
    }
    return 1;
}
int main(void) {
    TKGPU_TEST_PREPARE();
    queue(1,TKGPU_QUEUE_TAIL); queue(2,TKGPU_QUEUE_TAIL); queue(3,TKGPU_QUEUE_MARK);
    /* Mark first: consume it before exercising deletion from active id 1. */
    CHECK(Tcl_DoOneEvent(TKGPU_DONT_WAIT)==1&&handled[0]==3);
    CHECK(Tcl_DoOneEvent(TKGPU_DONT_WAIT)==1&&saw_active&&deleted==2);
    CHECK(Tcl_DoOneEvent(TKGPU_DONT_WAIT)==1&&handled[2]==9);
    CHECK(Tcl_DoOneEvent(TKGPU_DONT_WAIT)==0&&count==3);
    queue(4,TKGPU_QUEUE_TAIL); queue(5,TKGPU_QUEUE_TAIL); queue(6,TKGPU_QUEUE_TAIL);
    Tcl_DeleteEvents(nested,NULL);
    CHECK(Tcl_DoOneEvent(TKGPU_DONT_WAIT)==1&&handled[3]==4);
    CHECK(Tcl_DoOneEvent(TKGPU_DONT_WAIT)==1&&handled[4]==6);
    CHECK(Tcl_DoOneEvent(TKGPU_DONT_WAIT)==0);
    queue(7,TKGPU_QUEUE_MARK); queue(8,TKGPU_QUEUE_MARK);
    Tcl_DeleteEvents(all,NULL);
    queue(10,TKGPU_QUEUE_MARK);
    CHECK(Tcl_DoOneEvent(TKGPU_DONT_WAIT)==1&&handled[5]==10);
    CHECK(Tcl_DoOneEvent(TKGPU_DONT_WAIT)==0);
    puts("Queued/active deletion, callback reentry, marker repair and allocation reuse passed");
    return 0;
}
