/* Test descriptor introspection only; synthetic callback values are never invoked. */
#include <tcl.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define TKGPU_CHANNEL_API(name) candidate_##name
#include "TKGPUReferenceChannel.H"
static uint32_t state=421;
static uint32_t next(void) {state^=state<<13;state^=state>>17;state^=state<<5;return state;}
#define CHECK(name) do {if(candidate_##name(descriptor)!=Tcl_##name(descriptor)){fprintf(stderr,"mismatch %s at %u\n",#name,i);return 1;}} while(0)
#define SET(field) do {uintptr_t bits=next()&1?next():0; if(sizeof t.field!=sizeof bits)return 77; memcpy(&t.field,&bits,sizeof bits);} while(0)
int main(void) {
    for(unsigned i=0;i<20000;i++) {
        Tcl_ChannelType t; memset(&t,0,sizeof t); t.typeName="test";
        uintptr_t v=i%8; if(v==7)v=next()|8u;
        t.version=(Tcl_ChannelTypeVersion)v;
        const Tcl_ChannelType *descriptor=&t;
        SET(closeProc);
        SET(inputProc);
        SET(outputProc);
        SET(seekProc);
        SET(setOptionProc);
        SET(getOptionProc);
        SET(watchProc);
        SET(getHandleProc);
        SET(close2Proc);
        SET(blockModeProc);
        SET(flushProc);
        SET(handlerProc);
        SET(wideSeekProc);
        SET(threadActionProc);
        SET(truncateProc);
        CHECK(ChannelName);
        CHECK(ChannelVersion);
        CHECK(ChannelBlockModeProc);
        CHECK(ChannelCloseProc);
        CHECK(ChannelClose2Proc);
        CHECK(ChannelInputProc);
        CHECK(ChannelOutputProc);
        CHECK(ChannelSeekProc);
        CHECK(ChannelSetOptionProc);
        CHECK(ChannelGetOptionProc);
        CHECK(ChannelWatchProc);
        CHECK(ChannelGetHandleProc);
        CHECK(ChannelFlushProc);
        CHECK(ChannelHandlerProc);
        CHECK(ChannelWideSeekProc);
        CHECK(ChannelThreadActionProc);
        CHECK(ChannelTruncateProc);
    }
    puts("340000 channel descriptor comparisons passed");
    /* Old clients can supply physically shorter descriptors. */
    const size_t sizes[]={offsetof(Tcl_ChannelType,blockModeProc),
        offsetof(Tcl_ChannelType,wideSeekProc),offsetof(Tcl_ChannelType,threadActionProc),
        offsetof(Tcl_ChannelType,truncateProc),sizeof(Tcl_ChannelType)};
    for(unsigned i=0;i<10000;i++) {
        unsigned v=i%5+1; Tcl_ChannelType t; memset(&t,0,sizeof t);
        t.typeName="test"; t.version=(Tcl_ChannelTypeVersion)(uintptr_t)v;
        Tcl_ChannelType *p=malloc(sizes[v-1]); if(!p) return 2;
        memcpy(p,&t,sizes[v-1]);
        const Tcl_ChannelType *descriptor=p;
        CHECK(ChannelName);
        CHECK(ChannelVersion);
        CHECK(ChannelCloseProc);
        CHECK(ChannelInputProc);
        CHECK(ChannelOutputProc);
        CHECK(ChannelSeekProc);
        CHECK(ChannelSetOptionProc);
        CHECK(ChannelGetOptionProc);
        CHECK(ChannelWatchProc);
        CHECK(ChannelGetHandleProc);
        CHECK(ChannelClose2Proc);
        CHECK(ChannelBlockModeProc);
        CHECK(ChannelFlushProc);
        CHECK(ChannelHandlerProc);
        CHECK(ChannelWideSeekProc);
        CHECK(ChannelThreadActionProc);
        CHECK(ChannelTruncateProc);
        free(p);
    }
    puts("170000 legacy descriptor comparisons passed");
    return 0;
}
