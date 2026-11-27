/* Compile against target Tcl 8.6 headers, not the Tcl runtime.
   Never publish this incomplete table to unaudited extensions. */
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif
#include <sys/stat.h>
#include <limits.h>
#include <stdint.h>
#include <errno.h>
#include <signal.h>
#include <tcl.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#if TCL_MAJOR_VERSION != 8 || TCL_MINOR_VERSION != 6
#error Reference TclStubs preview requires Tcl 8.6 headers
#endif
/* These slots are restricted to this Linux reference-header build.
   Tcl_StatBuf is the target header's actual type, not a guessed layout.
   Allocation uses the same allocator as the existing Free slot. */
Tcl_StatBuf *Tcl_AllocStatBuf(void) {
    Tcl_StatBuf *s=(Tcl_StatBuf *)Tcl_Alloc((unsigned)sizeof *s);
    memset(s,0,sizeof *s);
    return s;
}
#define STAT_FIELD(type,name,field) \
    type Tcl_##name(const Tcl_StatBuf *s) { return (type)s->field; }
STAT_FIELD(unsigned,GetFSDeviceFromStat,st_dev)
STAT_FIELD(unsigned,GetFSInodeFromStat,st_ino)
STAT_FIELD(unsigned,GetModeFromStat,st_mode)
STAT_FIELD(int,GetLinkCountFromStat,st_nlink)
STAT_FIELD(int,GetUserIdFromStat,st_uid)
STAT_FIELD(int,GetGroupIdFromStat,st_gid)
STAT_FIELD(int,GetDeviceTypeFromStat,st_rdev)
STAT_FIELD(Tcl_WideInt,GetAccessTimeFromStat,st_atime)
STAT_FIELD(Tcl_WideInt,GetModificationTimeFromStat,st_mtime)
STAT_FIELD(Tcl_WideInt,GetChangeTimeFromStat,st_ctime)
STAT_FIELD(Tcl_WideUInt,GetSizeFromStat,st_size)
STAT_FIELD(Tcl_WideUInt,GetBlocksFromStat,st_blocks)
STAT_FIELD(unsigned,GetBlockSizeFromStat,st_blksize)
#undef STAT_FIELD

/* Linux preview diagnostics. Use native symbolic constants, not fixed
   architecture-specific integers. Unknown values never index an array.
   String results are immutable; none of these APIs sends a signal. */
const char *Tcl_ErrnoId(void) {
#define ERR(name) if(errno==(name)) return #name;
    ERR(EPERM)
    ERR(ENOENT)
    ERR(ESRCH)
    ERR(EINTR)
    ERR(EIO)
    ERR(ENXIO)
    ERR(E2BIG)
    ERR(ENOEXEC)
    ERR(EBADF)
    ERR(ECHILD)
    ERR(EAGAIN)
    ERR(ENOMEM)
    ERR(EACCES)
    ERR(EFAULT)
    ERR(ENOTBLK)
    ERR(EBUSY)
    ERR(EEXIST)
    ERR(EXDEV)
    ERR(ENODEV)
    ERR(ENOTDIR)
    ERR(EISDIR)
    ERR(EINVAL)
    ERR(ENFILE)
    ERR(EMFILE)
    ERR(ENOTTY)
    ERR(ETXTBSY)
    ERR(EFBIG)
    ERR(ENOSPC)
    ERR(ESPIPE)
    ERR(EROFS)
    ERR(EMLINK)
    ERR(EPIPE)
    ERR(EDOM)
    ERR(ERANGE)
    ERR(EDEADLK)
    ERR(ENAMETOOLONG)
    ERR(ENOLCK)
    ERR(ENOSYS)
    ERR(ENOTEMPTY)
    ERR(ELOOP)
    ERR(ENOMSG)
    ERR(EIDRM)
    ERR(ECHRNG)
    ERR(EL2NSYNC)
    ERR(EL3HLT)
    ERR(EL3RST)
    ERR(ELNRNG)
    ERR(EUNATCH)
    ERR(ENOCSI)
    ERR(EL2HLT)
    ERR(EBADE)
    ERR(EBADR)
    ERR(EXFULL)
    ERR(ENOANO)
    ERR(EBADRQC)
    ERR(EBADSLT)
    ERR(EBFONT)
    ERR(ENOSTR)
    ERR(ENODATA)
    ERR(ETIME)
    ERR(ENOSR)
    ERR(ENONET)
    ERR(ENOPKG)
    ERR(EREMOTE)
    ERR(ENOLINK)
    ERR(EADV)
    ERR(ESRMNT)
    ERR(ECOMM)
    ERR(EPROTO)
    ERR(EMULTIHOP)
    ERR(EDOTDOT)
    ERR(EBADMSG)
    ERR(EOVERFLOW)
    ERR(ENOTUNIQ)
    ERR(EBADFD)
    ERR(EREMCHG)
    ERR(ELIBACC)
    ERR(ELIBBAD)
    ERR(ELIBSCN)
    ERR(ELIBMAX)
    ERR(ELIBEXEC)
    ERR(EILSEQ)
    ERR(ERESTART)
    ERR(EUSERS)
    ERR(ENOTSOCK)
    ERR(EDESTADDRREQ)
    ERR(EMSGSIZE)
    ERR(EPROTOTYPE)
    ERR(ENOPROTOOPT)
    ERR(EPROTONOSUPPORT)
    ERR(ESOCKTNOSUPPORT)
    ERR(ENOTSUP)
    ERR(EPFNOSUPPORT)
    ERR(EAFNOSUPPORT)
    ERR(EADDRINUSE)
    ERR(EADDRNOTAVAIL)
    ERR(ENETDOWN)
    ERR(ENETUNREACH)
    ERR(ENETRESET)
    ERR(ECONNABORTED)
    ERR(ECONNRESET)
    ERR(ENOBUFS)
    ERR(EISCONN)
    ERR(ENOTCONN)
    ERR(ESHUTDOWN)
    ERR(ETOOMANYREFS)
    ERR(ETIMEDOUT)
    ERR(ECONNREFUSED)
    ERR(EHOSTDOWN)
    ERR(EHOSTUNREACH)
    ERR(EALREADY)
    ERR(EINPROGRESS)
    ERR(ESTALE)
    ERR(EUCLEAN)
    ERR(ENOTNAM)
    ERR(ENAVAIL)
    ERR(EISNAM)
    ERR(EREMOTEIO)
    ERR(EDQUOT)
    ERR(ENOMEDIUM)
    ERR(EMEDIUMTYPE)
    ERR(ECANCELED)
    ERR(EOWNERDEAD)
    ERR(ENOTRECOVERABLE)
#undef ERR
    return "unknown error";
}
const char *Tcl_SignalId(int sig) {
#define SIG(name) if(sig==(name)) return #name;
    SIG(SIGHUP)
    SIG(SIGINT)
    SIG(SIGQUIT)
    SIG(SIGILL)
    SIG(SIGTRAP)
    SIG(SIGABRT)
    SIG(SIGBUS)
    SIG(SIGFPE)
    SIG(SIGKILL)
    SIG(SIGUSR1)
    SIG(SIGSEGV)
    SIG(SIGUSR2)
    SIG(SIGPIPE)
    SIG(SIGALRM)
    SIG(SIGTERM)
    SIG(SIGCHLD)
    SIG(SIGCONT)
    SIG(SIGSTOP)
    SIG(SIGTSTP)
    SIG(SIGTTIN)
    SIG(SIGTTOU)
    SIG(SIGURG)
    SIG(SIGXCPU)
    SIG(SIGXFSZ)
    SIG(SIGVTALRM)
    SIG(SIGPROF)
    SIG(SIGWINCH)
    SIG(SIGIO)
    SIG(SIGPWR)
    SIG(SIGSYS)
#undef SIG
    return "unknown signal";
}
const char *Tcl_SignalMsg(int sig) {
#define MSG(name,desc) if(sig==(name)) return desc;
    MSG(SIGHUP,"hangup")
    MSG(SIGINT,"interrupt")
    MSG(SIGQUIT,"quit signal")
    MSG(SIGILL,"illegal instruction")
    MSG(SIGTRAP,"trace trap")
    MSG(SIGABRT,"SIGABRT")
    MSG(SIGBUS,"bus error")
    MSG(SIGFPE,"floating-point exception")
    MSG(SIGKILL,"kill signal")
    MSG(SIGUSR1,"user-defined signal 1")
    MSG(SIGSEGV,"segmentation violation")
    MSG(SIGUSR2,"user-defined signal 2")
    MSG(SIGPIPE,"write on pipe with no readers")
    MSG(SIGALRM,"alarm clock")
    MSG(SIGTERM,"software termination signal")
    MSG(SIGCHLD,"child status changed")
    MSG(SIGCONT,"continue after stop")
    MSG(SIGSTOP,"stop")
    MSG(SIGTSTP,"stop signal from tty")
    MSG(SIGTTIN,"background tty read")
    MSG(SIGTTOU,"background tty write")
    MSG(SIGURG,"urgent I/O condition")
    MSG(SIGXCPU,"exceeded CPU time limit")
    MSG(SIGXFSZ,"exceeded file size limit")
    MSG(SIGVTALRM,"virtual time alarm")
    MSG(SIGPROF,"profiling alarm")
    MSG(SIGWINCH,"window changed")
    MSG(SIGIO,"input/output possible on file")
    MSG(SIGPWR,"power-fail restart")
    MSG(SIGSYS,"bad argument to system call")
#undef MSG
    return "unknown signal";
}

#include "TKGPUReferencePaths.H"
#include "TKGPUReferenceParse.H"
#include "TKGPUReferenceParseExt.H"
#include "TKGPUReferenceStartup.H"
#include "TKGPUReferencePreserve.H"
#include "TKGPUReferenceAsync.H"
#include "TKGPUReferenceNativeFS.H"
#include "TKGPUReferenceLink.H"
#include "TKGPUReferenceSystem.H"
#include "TKGPUReferenceAttrs.H"
#include "TKGPUReferenceNativeOps.H"
#include "TKGPUReferenceFS.H"
#undef Tcl_FindHashEntry
#undef Tcl_CreateHashEntry
#include "TKGPUReferenceHash.H"
#include "TKGPUReferenceResult.H"
#undef Tcl_PkgProvide
#undef Tcl_PkgPresent
#undef Tcl_PkgRequire
#include "TKGPUReferencePackage.H"
#include "TKGPUReferencePackageRequire.H"
#include "TKGPUReferenceChannel.H"
#include "TKGPUReferenceChannelIO.H"
#include "TKGPUReferenceVFS.H"
#include "TKGPUReferenceHashStats.H"
#undef Tcl_DbNewBooleanObj
#include "TKGPUReferenceDebug.H"

static const TclStubs preview_table;
static CONST84_RETURN char *preview_require(Tcl_Interp *i,const char *name,
                                      const char *version,int exact,void *data) {
    if(name&&strcmp(name,"Tcl")) return pkg_require_one(i,name,version,exact,data);
    if(data) *(const TclStubs **)data=NULL;
    if(!name||strcmp(name,"Tcl")||!version||strcmp(version,"8.6")) {
        Tcl_SetObjResult(i,Tcl_NewStringObj("TKGPU reference Stubs preview accepts Tcl 8.6 only",-1));
        return NULL;
    }
    if(data) *(const TclStubs **)data=&preview_table;
    return "8.6";
}
CONST84_RETURN char *Tcl_PkgRequire(Tcl_Interp *i,const char *name,const char *version,int exact) {
    return preview_require(i,name,version,exact,NULL);
}
static const TclStubs preview_table={
    .magic=TCL_STUB_MAGIC,
    .hooks=NULL,
    .tcl_PkgRequireEx=preview_require,
    .tclFreeObj=TclFreeObj,
#define TKGPU_SLOT(name) .tcl_##name=Tcl_##name,
#include "TKGPUReferenceSlots.H"
#undef TKGPU_SLOT
};
const void *tg_reference_stubs_table(const char *const *required,size_t count) {
    static const char *const supported[]={
        "Tcl_PkgRequireEx","TclFreeObj",
#define TKGPU_SLOT(name) "Tcl_" #name,
#include "TKGPUReferenceSlots.H"
#undef TKGPU_SLOT
    };
    if(!required||!count||count>sizeof supported/sizeof supported[0]) return NULL;
    for(size_t k=0;k<count;k++) {
        if(!required[k]) return NULL;
        size_t n=0;
        while(n<sizeof supported/sizeof supported[0]&&strcmp(required[k],supported[n])) n++;
        if(n==sizeof supported/sizeof supported[0]) return NULL;
    }
    return &preview_table;
}
