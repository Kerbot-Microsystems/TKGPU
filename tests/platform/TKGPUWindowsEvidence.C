/* Run on the intended Windows installation/VM, not the build machine.
   Reports native smoke evidence only, never full platform certification. */
#include "TKGPUCompat.H"
#include "TKGPUHost.H"
#include <windows.h>
#include <stdio.h>
#include <string.h>
#ifndef TKGPU_SOURCE_REVISION
#define TKGPU_SOURCE_REVISION "unknown"
#endif
#ifndef TKGPU_WINDOWS_PROFILE
#define TKGPU_WINDOWS_PROFILE "modern"
#endif
static int command(Tcl_Interp *i,const char *script,const char *expected) {
    if(Tcl_Eval(i,script)!=0) { fprintf(stderr,"%s: %s\n",script,Tcl_GetStringResult(i)); return 0; }
    return !expected||!strcmp(expected,Tcl_GetStringResult(i));
}
static void timer(void *data) { *(int *)data=1; }
static int matches_target(const OSVERSIONINFOA *v) {
    if(!strcmp(TKGPU_WINDOWS_PROFILE,"95"))
        return sizeof(void *)==4&&v->dwPlatformId==VER_PLATFORM_WIN32_WINDOWS&&v->dwMajorVersion==4&&v->dwMinorVersion==0;
    if(!strcmp(TKGPU_WINDOWS_PROFILE,"98"))
        return sizeof(void *)==4&&v->dwPlatformId==VER_PLATFORM_WIN32_WINDOWS&&v->dwMajorVersion==4&&v->dwMinorVersion==10;
    if(!strcmp(TKGPU_WINDOWS_PROFILE,"xp"))
        return sizeof(void *)==4&&v->dwPlatformId==VER_PLATFORM_WIN32_NT&&v->dwMajorVersion==5&&v->dwMinorVersion==1;
    return !strcmp(TKGPU_WINDOWS_PROFILE,"modern")&&v->dwPlatformId==VER_PLATFORM_WIN32_NT&&v->dwMajorVersion>=6;
}
int main(void) {
    OSVERSIONINFOA version; memset(&version,0,sizeof version); version.dwOSVersionInfoSize=sizeof version;
    int os=GetVersionExA(&version)!=0;
    int native=0,object=0,events=0,gui=0,cleanup=1;
    Tcl_Interp *i=Tcl_CreateInterp();
    if(i&&Tcl_Init(i)==0) {
        native=command(i,"set test 42; incr test","43");
        Tcl_Obj *o=Tcl_NewIntObj(42); int value=0;
        if(o) { object=Tcl_GetIntFromObj(i,o,&value)==0&&value==42; TKGPU_ReleaseObject(o); }
        int fired=0; Tcl_TimerToken token=Tcl_CreateTimerHandler(0,timer,&fired);
        for(int k=0;token&&!fired&&k<100;k++) { Tcl_DoOneEvent(TKGPU_TIMER_EVENTS|TKGPU_DONT_WAIT); Sleep(1); }
        if(token&&!fired) Tcl_DeleteTimerHandler(token);
        events=fired;
        if(Tk_Init(i)==0) {
            gui=Tk_MainWindow(i)!=NULL&&command(i,"wm title . {TKGPU target probe}",NULL)&&
                command(i,"wm geometry . 320x240",NULL)&&command(i,"label .l -text target; pack .l; update",NULL)&&
                command(i,"wm withdraw .; wm deiconify .; update",NULL)&&
                command(i,"destroy .; update",NULL)&&Tk_GetNumMainWindows()==0;
        }
    }
    if(i) Tcl_DeleteInterp(i);
#ifdef TKGPU_LEGACY_WINDOWS
    cleanup=TKGPU_FinalizeLegacyThread()==0;
#endif
    int target=os&&matches_target(&version);
    int ok=target&&native&&object&&events&&gui&&cleanup&&TKGPU_HostABI()==1&&TKGPU_HostABIRevision()==4;
    printf("{\"schema\":1,\"revision\":\"%s\",\"profile\":\"%s\",\"os_major\":%lu,\"os_minor\":%lu,\"os_build\":%lu,\"os_platform\":%lu,\"pointer_bits\":%u,\"target_matches\":%d,\"abi\":%u,\"abi_revision\":%u,\"interpreter\":%d,\"objects\":%d,\"events\":%d,\"gui\":%d,\"cleanup\":%d,\"smoke_passed\":%s,\"qualified\":false}\n",
        TKGPU_SOURCE_REVISION,TKGPU_WINDOWS_PROFILE,(unsigned long)version.dwMajorVersion,(unsigned long)version.dwMinorVersion,
        (unsigned long)version.dwBuildNumber,(unsigned long)version.dwPlatformId,(unsigned)(sizeof(void *)*8),
        target,TKGPU_HostABI(),TKGPU_HostABIRevision(),native,object,events,gui,cleanup,ok?"true":"false");
    return ok?0:1;
}
