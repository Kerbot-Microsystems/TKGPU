#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif
#include "TKGPUClock.H"
#include "TKGPUCompat.H"
#include <limits.h>
#ifdef TKGPU_REFERENCE_STUBS
#include "../threads/TKGPUThreadNative.H"
#endif
#ifdef _WIN32
#include <windows.h>
#include <string.h>
#include "../threads/TKGPUThreadNative.H"
#include "legacy/TKGPUTickExtend.H"
typedef ULONGLONG (WINAPI *tg_tick64_fn)(void);
static tg_tick64_fn tick64;
static int resolved;
static tg_tick_state legacy;
int tg_clock_now(uint64_t *ms) {
    if(!ms) return 0;
    tg_native_guard_lock();
    if(!resolved) {
        HMODULE kernel=GetModuleHandleA("kernel32.dll");
        FARPROC proc=kernel?GetProcAddress(kernel,"GetTickCount64"):NULL;
        if(proc&&sizeof proc==sizeof tick64) memcpy(&tick64,&proc,sizeof tick64);
        resolved=1;
    }
    *ms=tick64?(uint64_t)tick64():tg_tick_extend(&legacy,(uint32_t)GetTickCount());
    tg_native_guard_unlock(); return 1;
}
void tg_clock_sleep(uint64_t ms) { Sleep((DWORD)(ms>1000?1000:ms)); }
#else
#include <time.h>
#include <errno.h>
#include <sys/time.h>
int tg_clock_now(uint64_t *ms) {
    struct timespec t;
    if(!ms||clock_gettime(CLOCK_MONOTONIC,&t)!=0||t.tv_sec<0||
       (uint64_t)t.tv_sec>(UINT64_MAX-999)/1000) return 0;
    *ms=(uint64_t)t.tv_sec*1000+(uint64_t)t.tv_nsec/1000000; return 1;
}
void tg_clock_sleep(uint64_t ms) {
    if(ms>1000) ms=1000;
    struct timespec t={(time_t)(ms/1000),(long)(ms%1000)*1000000L};
    while(nanosleep(&t,&t)!=0&&errno==EINTR) {}
}
#endif

static void native_get(Tcl_Time *out,void *data) {
    (void)data;
    if(!out) return;
#ifdef _WIN32
    FILETIME ft; GetSystemTimeAsFileTime(&ft);
    uint64_t ticks=((uint64_t)ft.dwHighDateTime<<32)|ft.dwLowDateTime;
    const uint64_t epoch=UINT64_C(116444736000000000);
    if(ticks<epoch) { out->sec=0; out->usec=0; return; }
    ticks-=epoch;
    uint64_t sec=ticks/UINT64_C(10000000);
    out->sec=sec>(uint64_t)LONG_MAX?LONG_MAX:(long)sec;
    out->usec=sec>(uint64_t)LONG_MAX?999999L:(long)((ticks%UINT64_C(10000000))/10u);
#else
    struct timeval tv;
    if(gettimeofday(&tv,NULL)!=0) { out->sec=0; out->usec=0; return; }
    out->sec=(long)tv.tv_sec; out->usec=(long)tv.tv_usec;
#endif
}
#ifdef TKGPU_REFERENCE_STUBS
#include "TKGPUTimeHooks.H"
int tg_timer_clock_now(uint64_t *ms) {
    if(!ms)return 0;
    Tcl_Time t;Tcl_GetTime(&t);
    if(t.sec<0||t.usec<0||t.usec>=1000000||(uint64_t)t.sec>(UINT64_MAX-999)/1000)return 0;
    *ms=(uint64_t)t.sec*1000+(uint64_t)t.usec/1000;return 1;
}
uint64_t tg_timer_wait_ms(uint64_t ms) {return scaled_wait(ms);}
#else
void Tcl_GetTime(Tcl_Time *out) {native_get(out,NULL);}
int tg_timer_clock_now(uint64_t *ms) {return tg_clock_now(ms);}
uint64_t tg_timer_wait_ms(uint64_t ms) {return ms;}
#endif
void Tcl_Sleep(int ms) {
    if(ms<=0)return;
    uint64_t remaining=tg_timer_wait_ms((uint64_t)ms);
    /* Reject saturated provider output rather than attempting an endless wait. */
    if(remaining==UINT64_MAX)return;
    while(remaining) {
        uint64_t chunk=remaining>1000?1000:remaining;
        tg_clock_sleep(chunk);remaining-=chunk;
    }
}
