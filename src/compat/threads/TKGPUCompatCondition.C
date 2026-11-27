#include "TKGPUCompatMutex.H"
#include <stdlib.h>
static tg_native_condition *condition(Tcl_Condition *key,int create) {
    if(!key) abort();
    tg_native_guard_lock();
    if(!*key&&create) *key=(Tcl_Condition)tg_native_condition_create();
    tg_native_condition *c=(tg_native_condition *)*key;
    tg_native_guard_unlock(); return c;
}
void Tcl_ConditionNotify(Tcl_Condition *key) {
    tg_native_condition *c=condition(key,0);
    if(c) tg_native_condition_notify(c);
}
void Tcl_ConditionWait(Tcl_Condition *key,Tcl_Mutex *mutex,const Tcl_Time *time) {
    int64_t ms=-1;
    if(time) {
        if(time->sec<0||time->usec<0) ms=0;
        else if(time->sec>86400||time->usec>86400000000LL) ms=86400000;
        else {
            ms=(int64_t)time->sec*1000+((int64_t)time->usec+999)/1000;
            if(ms>86400000) ms=86400000;
        }
    }
    tg_native_condition_wait(condition(key,1),tg_compat_mutex(mutex),ms);
}
void Tcl_ConditionFinalize(Tcl_Condition *key) {
    if(!key) return;
    tg_native_guard_lock();
    tg_native_condition *c=(tg_native_condition *)*key; *key=NULL;
    tg_native_guard_unlock();
    tg_native_condition_destroy(c);
}
