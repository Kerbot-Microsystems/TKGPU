/* Descriptor access only: no channel creation or I/O. */
#ifndef TKGPU_CHANNEL_API
#define TKGPU_CHANNEL_API(name) Tcl_##name
#endif
static unsigned channel_level(const Tcl_ChannelType *t) {
    if(t->version==TCL_CHANNEL_VERSION_2) return 2;
    if(t->version==TCL_CHANNEL_VERSION_3) return 3;
    if(t->version==TCL_CHANNEL_VERSION_4) return 4;
    if(t->version==TCL_CHANNEL_VERSION_5) return 5;
    return 1;
}
const char *TKGPU_CHANNEL_API(ChannelName)(const Tcl_ChannelType *t) { return t->typeName; }
Tcl_ChannelTypeVersion TKGPU_CHANNEL_API(ChannelVersion)(const Tcl_ChannelType *t) {
    return (Tcl_ChannelTypeVersion)(uintptr_t)channel_level(t);
}
Tcl_DriverBlockModeProc *TKGPU_CHANNEL_API(ChannelBlockModeProc)(const Tcl_ChannelType *t) {
    if(channel_level(t)>=2) return t->blockModeProc;
    Tcl_DriverBlockModeProc *p=NULL;
    /* Linux reference ABI: old descriptors put this callback in version. */
    if(sizeof p!=sizeof t->version) return NULL;
    memcpy(&p,&t->version,sizeof p); return p;
}
#define CHANNEL_FIELD(type,name,field) \
    type *TKGPU_CHANNEL_API(Channel##name)(const Tcl_ChannelType *t) { return t->field; }
CHANNEL_FIELD(Tcl_DriverCloseProc,CloseProc,closeProc)
CHANNEL_FIELD(Tcl_DriverClose2Proc,Close2Proc,close2Proc)
CHANNEL_FIELD(Tcl_DriverInputProc,InputProc,inputProc)
CHANNEL_FIELD(Tcl_DriverOutputProc,OutputProc,outputProc)
CHANNEL_FIELD(Tcl_DriverSeekProc,SeekProc,seekProc)
CHANNEL_FIELD(Tcl_DriverSetOptionProc,SetOptionProc,setOptionProc)
CHANNEL_FIELD(Tcl_DriverGetOptionProc,GetOptionProc,getOptionProc)
CHANNEL_FIELD(Tcl_DriverWatchProc,WatchProc,watchProc)
CHANNEL_FIELD(Tcl_DriverGetHandleProc,GetHandleProc,getHandleProc)
#undef CHANNEL_FIELD
#define CHANNEL_VERSIONED(type,name,field,min) \
    type *TKGPU_CHANNEL_API(Channel##name)(const Tcl_ChannelType *t) { return channel_level(t)>=min?t->field:NULL; }
CHANNEL_VERSIONED(Tcl_DriverFlushProc,FlushProc,flushProc,2)
CHANNEL_VERSIONED(Tcl_DriverHandlerProc,HandlerProc,handlerProc,2)
CHANNEL_VERSIONED(Tcl_DriverWideSeekProc,WideSeekProc,wideSeekProc,3)
CHANNEL_VERSIONED(Tcl_DriverThreadActionProc,ThreadActionProc,threadActionProc,4)
CHANNEL_VERSIONED(Tcl_DriverTruncateProc,TruncateProc,truncateProc,5)
#undef CHANNEL_VERSIONED
#undef TKGPU_CHANNEL_API
