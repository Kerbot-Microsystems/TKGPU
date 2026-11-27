#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif
#include "TKGPUExecutable.H"
#ifdef _WIN32
#include <windows.h>
int tg_executable_path(char *out,size_t cap) {
    if(!out||cap<2||cap>65535) return 0;
    DWORD n=GetModuleFileNameA(NULL,out,(DWORD)cap);
    if(!n||n>=cap) { out[0]=0; return 0; }
    out[n]=0; return 1;
}
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <stdint.h>
int tg_executable_path(char *out,size_t cap) {
    if(!out||cap<2||cap>UINT32_MAX) return 0;
    uint32_t n=(uint32_t)cap;
    if(_NSGetExecutablePath(out,&n)!=0) { out[0]=0; return 0; }
    return 1;
}
#elif defined(__linux__)
#include <unistd.h>
int tg_executable_path(char *out,size_t cap) {
    if(!out||cap<2) return 0;
    ssize_t n=readlink("/proc/self/exe",out,cap-1);
    if(n<1||(size_t)n>=cap-1) { out[0]=0; return 0; }
    out[n]=0; return 1;
}
#else
int tg_executable_path(char *out,size_t cap) {
    if(out&&cap) out[0]=0;
    return 0;
}
#endif
