/* Native Linux diagnostics. Legacy English descriptions were checked
   against the public Tcl ErrnoMsg API, not copied from Tcl source. */
#include <unistd.h>
#include <stdlib.h>
#ifndef TKGPU_SYSTEM_API
#define TKGPU_SYSTEM_API(n) Tcl_##n
#endif
const char *TKGPU_SYSTEM_API(ErrnoMsg)(int err) {
#define E(n,s) if(err==(n)) return s;
    E(EPERM,"not owner")
    E(ENOENT,"no such file or directory")
    E(ESRCH,"no such process")
    E(EINTR,"interrupted system call")
    E(EIO,"I/O error")
    E(ENXIO,"no such device or address")
    E(E2BIG,"argument list too long")
    E(ENOEXEC,"exec format error")
    E(EBADF,"bad file number")
    E(ECHILD,"no children")
    E(EAGAIN,"resource temporarily unavailable")
    E(ENOMEM,"not enough memory")
    E(EACCES,"permission denied")
    E(EFAULT,"bad address in system call argument")
    E(ENOTBLK,"block device required")
    E(EBUSY,"file busy")
    E(EEXIST,"file already exists")
    E(EXDEV,"cross-domain link")
    E(ENODEV,"no such device")
    E(ENOTDIR,"not a directory")
    E(EISDIR,"illegal operation on a directory")
    E(EINVAL,"invalid argument")
    E(ENFILE,"file table overflow")
    E(EMFILE,"too many open files")
    E(ENOTTY,"inappropriate device for ioctl")
    E(ETXTBSY,"text file or pseudo-device busy")
    E(EFBIG,"file too large")
    E(ENOSPC,"no space left on device")
    E(ESPIPE,"invalid seek")
    E(EROFS,"read-only file system")
    E(EMLINK,"too many links")
    E(EPIPE,"broken pipe")
    E(EDOM,"math argument out of range")
    E(ERANGE,"math result unrepresentable")
    E(EDEADLOCK,"resource deadlock avoided")
    E(ENAMETOOLONG,"file name too long")
    E(ENOLCK,"no locks available")
    E(ENOSYS,"function not implemented")
    E(ENOTEMPTY,"directory not empty")
    E(ELOOP,"too many levels of symbolic links")
    E(ENOMSG,"no message of desired type")
    E(EIDRM,"identifier removed")
    E(ECHRNG,"channel number out of range")
    E(EL2NSYNC,"level 2 not synchronized")
    E(EL3HLT,"level 3 halted")
    E(EL3RST,"level 3 reset")
    E(ELNRNG,"link number out of range")
    E(EUNATCH,"protocol driver not attached")
    E(ENOCSI,"no CSI structure available")
    E(EL2HLT,"level 2 halted")
    E(EBADE,"bad exchange descriptor")
    E(EBADR,"bad request descriptor")
    E(EXFULL,"message tables full")
    E(ENOANO,"anode table overflow")
    E(EBADRQC,"bad request code")
    E(EBADSLT,"invalid slot")
    E(EBFONT,"bad font file format")
    E(ENOSTR,"not a stream device")
    E(ENODATA,"no data available")
    E(ETIME,"timer expired")
    E(ENOSR,"out of stream resources")
    E(ENONET,"machine is not on the network")
    E(ENOPKG,"package not installed")
    E(EREMOTE,"pathname hit remote file system")
    E(ENOLINK,"link has been severed")
    E(EADV,"advertise error")
    E(ESRMNT,"srmount error")
    E(ECOMM,"communication error on send")
    E(EPROTO,"protocol error")
    E(EMULTIHOP,"multihop attempted")
    E(EDOTDOT,"cross mount point")
    E(EBADMSG,"not a data message")
    E(EOVERFLOW,"file too big")
    E(ENOTUNIQ,"name not unique on network")
    E(EBADFD,"file descriptor in bad state")
    E(EREMCHG,"remote address changed")
    E(ELIBACC,"cannot access a needed shared library")
    E(ELIBBAD,"accessing a corrupted shared library")
    E(ELIBMAX,"attempting to link in more shared libraries than system limit")
    E(ELIBEXEC,"cannot exec a shared library directly")
    E(EILSEQ,"illegal byte sequence")
    E(ERESTART,"interrupted system call should be restarted")
    E(ESTRPIPE,"streams pipe error")
    E(EUSERS,"too many users")
    E(ENOTSOCK,"socket operation on non-socket")
    E(EDESTADDRREQ,"destination address required")
    E(EMSGSIZE,"message too long")
    E(EPROTOTYPE,"protocol wrong type for socket")
    E(ENOPROTOOPT,"bad protocol option")
    E(EPROTONOSUPPORT,"protocol not supported")
    E(ESOCKTNOSUPPORT,"socket type not supported")
    E(ENOTSUP,"operation not supported")
    E(EPFNOSUPPORT,"protocol family not supported")
    E(EAFNOSUPPORT,"address family not supported by protocol")
    E(EADDRINUSE,"address already in use")
    E(EADDRNOTAVAIL,"cannot assign requested address")
    E(ENETDOWN,"network is down")
    E(ENETUNREACH,"network is unreachable")
    E(ENETRESET,"network dropped connection on reset")
    E(ECONNABORTED,"software caused connection abort")
    E(ECONNRESET,"connection reset by peer")
    E(ENOBUFS,"no buffer space available")
    E(EISCONN,"socket is already connected")
    E(ENOTCONN,"socket is not connected")
    E(ESHUTDOWN,"cannot send after socket shutdown")
    E(ETOOMANYREFS,"too many references: cannot splice")
    E(ETIMEDOUT,"connection timed out")
    E(ECONNREFUSED,"connection refused")
    E(EHOSTDOWN,"host is down")
    E(EHOSTUNREACH,"host is unreachable")
    E(EALREADY,"operation already in progress")
    E(EINPROGRESS,"operation now in progress")
    E(ESTALE,"stale remote file handle")
    E(EUCLEAN,"structure needs cleaning")
    E(ENOTNAM,"not a name file")
    E(ENAVAIL,"not available")
    E(EISNAM,"is a name file")
    E(EREMOTEIO,"remote i/o error")
    E(EDQUOT,"disk quota exceeded")
    E(ENOMEDIUM,"no medium found")
    E(EMEDIUMTYPE,"wrong medium type")
    E(ECANCELED,"operation canceled")
    E(EOWNERDEAD,"owner died")
    E(ENOTRECOVERABLE,"state not recoverable")
#undef E
    return strerror(err);
}
const char *TKGPU_SYSTEM_API(PosixError)(Tcl_Interp *i) {
    int err=errno;
    const char *id=Tcl_ErrnoId();
    const char *msg=TKGPU_SYSTEM_API(ErrnoMsg)(err);
    /* The variable facade copies diagnostic strings before returning. */
    if(i) Tcl_SetErrorCode(i,"POSIX",id,msg,(char *)NULL);
    errno=err;
    return TKGPU_SYSTEM_API(ErrnoMsg)(err);
}
char *TKGPU_SYSTEM_API(GetCwd)(Tcl_Interp *i,Tcl_DString *d) {
    Tcl_DStringInit(d);
    char *path=getcwd(NULL,0);
    if(!path) {
        int err=errno;
        if(i) {
            const char *msg=TKGPU_SYSTEM_API(PosixError)(i);
            Tcl_SetObjResult(i,Tcl_NewStringObj("error getting working directory name: ",-1));
            Tcl_AppendResult(i,msg,(char *)NULL);
        }
        errno=err; return NULL;
    }
    size_t n=strlen(path);
    if(n>=INT_MAX) {free(path);errno=ENAMETOOLONG;return NULL;}
    Tcl_DStringAppend(d,path,(int)n); free(path); return d->string;
}
const char *TKGPU_SYSTEM_API(GetHostName)(void) {
    static Tcl_ThreadDataKey key;
    char *name=Tcl_GetThreadData(&key,256);
    if(!name) return "";
    if(!*name) {
        if(gethostname(name,256)!=0) name[0]=0;
        name[255]=0;
    }
    return name;
}
#undef TKGPU_SYSTEM_API
