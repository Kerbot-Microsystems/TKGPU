#include "TKGPUCompat.H"
#include "../../platform/threads/TKGPUThreadNative.H"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

/* Native shared-library handles. VFS loaders are a separate filesystem
   facility; no system Tcl runtime is loaded or called by this implementation. */
struct Tcl_LoadHandle_ {
    struct Tcl_LoadHandle_ *next;
    void *module;
    unsigned active;
    int closing;
};
static Tcl_LoadHandle handles;
static int failure(Tcl_Interp *i,const char *before,const char *name,const char *after) {
    if(i) {
        Tcl_ResetResult(i);
        Tcl_AppendResult(i,before,name?name:"",after,(char *)NULL);
    }
    return 1;
}
static Tcl_LoadHandle lookup(Tcl_LoadHandle token) {
    Tcl_LoadHandle h=handles;
    while(h&&h!=token) h=h->next;
    return h;
}
static int close_module(void *module) {
#ifdef _WIN32
    return FreeLibrary((HMODULE)module)?0:1;
#else
    return dlclose(module)==0?0:1;
#endif
}
void *Tcl_FindSymbol(Tcl_Interp *i,Tcl_LoadHandle token,const char *name) {
    if(!name) { failure(i,"invalid symbol name","",""); return NULL; }
    tg_native_guard_lock();
    Tcl_LoadHandle h=lookup(token);
    if(!h||h->closing) {
        tg_native_guard_unlock();
        failure(i,"invalid library handle","",""); return NULL;
    }
    ++h->active;
    void *module=h->module;
    tg_native_guard_unlock();
    void *address=NULL;
#ifdef _WIN32
    FARPROC proc=GetProcAddress((HMODULE)module,name);
    if(sizeof address==sizeof proc) memcpy(&address,&proc,sizeof address);
#else
    address=dlsym(module,name);
#endif
    tg_native_guard_lock(); --h->active; tg_native_guard_unlock();
    if(!address) failure(i,"cannot find symbol \"",name,"\"");
    return address;
}
int Tcl_FSUnloadFile(Tcl_Interp *i,Tcl_LoadHandle token) {
    tg_native_guard_lock();
    Tcl_LoadHandle h=lookup(token);
    if(!h||h->closing||h->active) {
        tg_native_guard_unlock();
        return failure(i,"invalid or busy library handle","","");
    }
    h->closing=1;
    tg_native_guard_unlock();
    /* Constructors/destructors and native resolvers can reenter the loader. */
    int rc=close_module(h->module);
    tg_native_guard_lock();
    if(rc) h->closing=0;
    else {
        Tcl_LoadHandle *link=&handles;
        while(*link&&*link!=h) link=&(*link)->next;
        if(*link) *link=h->next;
    }
    tg_native_guard_unlock();
    if(rc) return failure(i,"cannot unload shared library","","");
    free(h);
    return 0;
}
int Tcl_LoadFile(Tcl_Interp *i,Tcl_Obj *path,const char *const symbols[],
                 int flags,void *procedures,Tcl_LoadHandle *output) {
    if(output) *output=NULL;
    if(!path||!output||(flags&~(TKGPU_LOAD_GLOBAL|TKGPU_LOAD_LAZY)))
        return failure(i,"invalid library load arguments","","");
    int length=0;
    const char *name=Tcl_GetStringFromObj(path,&length);
    if(!name||memchr(name,0,(size_t)length))
        return failure(i,"invalid library path","","");
    size_t count=0;
    if(symbols) while(symbols[count]) {
        if(count==SIZE_MAX/sizeof(void *)-1) return failure(i,"too many library symbols","","");
        ++count;
    }
    if(count&&!procedures) return failure(i,"missing symbol output array","","");
    void **addresses=count?calloc(count,sizeof *addresses):NULL;
    if(count&&!addresses) Tcl_Panic("cannot allocate symbol output array");
    if(count) memset(procedures,0,count*sizeof(void *));
    if(!TKGPU_RetainObject(path)) { free(addresses); return 1; }
    if(i) Tcl_ResetResult(i);
    void *module=NULL;
#ifdef _WIN32
    int units=0;
    Tcl_UniChar *wide=Tcl_GetUnicodeFromObj(path,&units);
    if(wide) {
#ifdef TKGPU_LEGACY_WINDOWS
        BOOL replaced=FALSE;
        int needed=WideCharToMultiByte(CP_ACP,WC_NO_BEST_FIT_CHARS,(LPCWSTR)wide,units+1,NULL,0,NULL,&replaced);
        char *native=needed>0?malloc((size_t)needed):NULL;
        if(native&&WideCharToMultiByte(CP_ACP,WC_NO_BEST_FIT_CHARS,(LPCWSTR)wide,units+1,native,needed,NULL,&replaced)&&!replaced)
            module=(void *)LoadLibraryA(native);
        free(native);
#else
        module=(void *)LoadLibraryW((LPCWSTR)wide);
#endif
    }
    if(!module) {
        char code[64]; snprintf(code,sizeof code,"\": system error %lu",(unsigned long)GetLastError());
        failure(i,"couldn't load file \"",name,code);
    }
#else
    int mode=(flags&TKGPU_LOAD_LAZY)?RTLD_LAZY:RTLD_NOW;
    mode|=(flags&TKGPU_LOAD_GLOBAL)?RTLD_GLOBAL:RTLD_LOCAL;
    module=dlopen(name,mode);
    if(!module) {
        const char *reason=dlerror();
        failure(i,"couldn't load file \"",name,"\": ");
        if(i&&reason) Tcl_AppendResult(i,reason,(char *)NULL);
    }
#endif
    TKGPU_ReleaseObject(path);
    if(!module) { free(addresses); return 1; }
    Tcl_LoadHandle h=calloc(1,sizeof *h);
    if(!h) { close_module(module); free(addresses); Tcl_Panic("cannot allocate library handle"); }
    h->module=module;
    tg_native_guard_lock(); h->next=handles; handles=h; tg_native_guard_unlock();
    for(size_t k=0;k<count;k++) {
        addresses[k]=Tcl_FindSymbol(i,h,symbols[k]);
        if(!addresses[k]) {
            (void)Tcl_FSUnloadFile(NULL,h); free(addresses); return 1;
        }
    }
    if(count) memcpy(procedures,addresses,count*sizeof *addresses);
    free(addresses); *output=h;
    return 0;
}
int Tcl_FSLoadFile(Tcl_Interp *i,Tcl_Obj *path,const char *first,const char *second,
                   Tcl_PackageInitProc **proc1,Tcl_PackageInitProc **proc2,
                   Tcl_LoadHandle *handle,Tcl_FSUnloadFileProc **unload) {
    if(proc1) *proc1=NULL;
    if(proc2) *proc2=NULL;
    if(unload) *unload=NULL;
    const char *symbols[3]; size_t count=0;
    if(first) symbols[count++]=first;
    if(second) symbols[count++]=second;
    symbols[count]=NULL;
    void *addresses[2]={NULL,NULL};
    int rc=Tcl_LoadFile(i,path,symbols,0,addresses,handle);
    if(rc) return rc;
    size_t at=0;
    if(first) { if(proc1) memcpy(proc1,&addresses[at],sizeof *proc1); ++at; }
    if(second&&proc2) memcpy(proc2,&addresses[at],sizeof *proc2);
    /* Modern callers unload through Tcl_FSUnloadFile, as in Tcl 8.6. */
    return 0;
}
