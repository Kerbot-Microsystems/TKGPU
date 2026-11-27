/* A pending channel error owns one reference. Get transfers it to caller. */
#ifndef CHANNEL_ERROR_API
#define CHANNEL_ERROR_API(n) Tcl_##n
#endif
void CHANNEL_ERROR_API(SetChannelErrorInterp)(Tcl_Interp *i,Tcl_Obj *msg) {
    if(!i) return;
    if(msg) Tcl_IncrRefCount(msg);
    Tcl_Obj *old=CHANNEL_ERROR_FIELD(i); CHANNEL_ERROR_FIELD(i)=msg;
    if(old) Tcl_DecrRefCount(old);
}
void CHANNEL_ERROR_API(GetChannelErrorInterp)(Tcl_Interp *i,Tcl_Obj **msg) {
    if(!msg) return;
    *msg=i?CHANNEL_ERROR_FIELD(i):NULL;
    if(i) CHANNEL_ERROR_FIELD(i)=NULL;
}
#undef CHANNEL_ERROR_API
