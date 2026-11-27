/* The provider triple is published atomically under the runtime guard.
   Invoke extension callbacks only after releasing that guard. */
#ifndef TIME_API
#define TIME_API(n) Tcl_##n
#endif
static void native_scale(Tcl_Time *t,void *data) { (void)t;(void)data; }
static Tcl_GetTimeProc *time_get=native_get;
static Tcl_ScaleTimeProc *time_scale=native_scale;
static void *time_data;
void TIME_API(SetTimeProc)(Tcl_GetTimeProc *get,Tcl_ScaleTimeProc *scale,void *data) {
    tg_native_guard_lock();
    time_get=get?get:native_get;time_scale=scale?scale:native_scale;time_data=data;
    tg_native_guard_unlock();
}
void TIME_API(QueryTimeProc)(Tcl_GetTimeProc **get,Tcl_ScaleTimeProc **scale,void **data) {
    tg_native_guard_lock();
    if(get)*get=time_get;
    if(scale)*scale=time_scale;
    if(data)*data=time_data;
    tg_native_guard_unlock();
}
void TIME_API(GetTime)(Tcl_Time *out) {
    if(!out)return;
    Tcl_GetTimeProc *get;void *data;
    TIME_API(QueryTimeProc)(&get,NULL,&data);get(out,data);
}
/* Convert a virtual timeout into a native wait, rounded up to milliseconds.
   Clamp malformed/negative provider output instead of overflowing. */
static uint64_t scaled_wait(uint64_t ms) {
    if(ms/1000>(uint64_t)LONG_MAX)return UINT64_MAX;
    Tcl_Time t={(long)(ms/1000),(long)(ms%1000)*1000};
    Tcl_ScaleTimeProc *scale;void *data;
    TIME_API(QueryTimeProc)(NULL,&scale,&data);scale(&t,data);
    if(t.sec<0||t.usec<0||t.usec>=1000000)return 0;
    if((uint64_t)t.sec>(UINT64_MAX-1000)/1000)return UINT64_MAX;
    return (uint64_t)t.sec*1000+((uint64_t)t.usec+999)/1000;
}
#undef TIME_API
