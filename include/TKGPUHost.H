#ifndef TKGPU_HOST_H
#define TKGPU_HOST_H
/* Independent host ABI. No Tcl/Tk or CPython headers, structs or version requirements.
   ABI 1 uses UTF-8 NUL-terminated strings and owner-thread-affine opaque handles. */
#include <stddef.h>
#include <stdint.h>
#if defined(_WIN32)
# if defined(TKGPU_COMPAT_BUILD)
#  define TKGPU_HOST_API __declspec(dllexport)
# else
#  define TKGPU_HOST_API __declspec(dllimport)
# endif
#else
# define TKGPU_HOST_API __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
extern "C" {
#endif
#define TKGPU_HOST_ABI 1u
#define TKGPU_HOST_ABI_REVISION 4u
typedef struct tg_interp TKGPUHost;
enum { TKGPU_HOST_OK=0, TKGPU_HOST_ERROR=1, TKGPU_HOST_DONT_WAIT=2,
       TKGPU_HOST_WINDOW_EVENTS=4, TKGPU_HOST_FILE_EVENTS=8,
       TKGPU_HOST_TIMER_EVENTS=16, TKGPU_HOST_IDLE_EVENTS=32 };
typedef int (*TKGPUHostCommand)(void *context,TKGPUHost *host,size_t argc,const char *const argv[]);
typedef void (*TKGPUHostDispose)(void *context);
TKGPU_HOST_API unsigned TKGPU_HostABI(void);
TKGPU_HOST_API unsigned TKGPU_HostABIRevision(void);
TKGPU_HOST_API int TKGPU_HostIsOwner(const TKGPUHost *);
 /* Explicit scalar conversions: no guessing from a text result. */
TKGPU_HOST_API int TKGPU_HostInteger(const char *,int64_t *);
TKGPU_HOST_API int TKGPU_HostDouble(const char *,double *);
TKGPU_HOST_API int TKGPU_HostBoolean(const char *,int *);
 /* Join literal list items without executing a command or touching a host result.
    The output belongs to the library; release with HostFree. */
TKGPU_HOST_API int TKGPU_HostJoin(size_t,const char *const [],char **);
TKGPU_HOST_API const char *TKGPU_HostVersion(void);
TKGPU_HOST_API TKGPUHost *TKGPU_HostCreate(void);
TKGPU_HOST_API int TKGPU_HostInit(TKGPUHost *,int with_gui);
/* Close only on the owner thread, outside active calls/callbacks. */
TKGPU_HOST_API int TKGPU_HostClose(TKGPUHost *);
TKGPU_HOST_API int TKGPU_HostCall(TKGPUHost *,size_t,const char *const []);
/* Borrowed UTF-8 result and status in one call. Copy before further operations.
   A NULL return means invalid host or status pointer. */
TKGPU_HOST_API const char *TKGPU_HostCallResult(TKGPUHost *,size_t,const char *const [],int *status);
/* The core labels only results whose type is known; unknown results are strings.
   Never infer type from the appearance of a value. */
enum { TKGPU_HOST_STRING=0, TKGPU_HOST_INTEGER=1, TKGPU_HOST_BOOLEAN=2, TKGPU_HOST_LIST=3,
       TKGPU_HOST_DOUBLE=4, TKGPU_HOST_INT_LIST=5, TKGPU_HOST_DOUBLE_LIST=6 };
TKGPU_HOST_API const char *TKGPU_HostCallTagged(TKGPUHost *,size_t,const char *const [],int *status,int *kind);
/* Tightly packed RGBA8, exact dimensions and length; no external image ABI. */
TKGPU_HOST_API int TKGPU_HostPhotoWrite(TKGPUHost *,const char *,int,int,const void *,size_t);
TKGPU_HOST_API int TKGPU_HostPhotoRead(TKGPUHost *,const char *,int,int,void *,size_t);
TKGPU_HOST_API int TKGPU_HostEval(TKGPUHost *,const char *);
TKGPU_HOST_API int TKGPU_HostEvalFile(TKGPUHost *,const char *);
/* Borrowed result; copy before next operation. */
TKGPU_HOST_API const char *TKGPU_HostResult(const TKGPUHost *);
TKGPU_HOST_API int TKGPU_HostSetResult(TKGPUHost *,const char *);
TKGPU_HOST_API int TKGPU_HostRegister(TKGPUHost *,const char *,TKGPUHostCommand,void *,TKGPUHostDispose);
TKGPU_HOST_API int TKGPU_HostUnregister(TKGPUHost *,const char *);
/* Split returns a single owned allocation, released via HostFree. */
TKGPU_HOST_API int TKGPU_HostSplit(TKGPUHost *,const char *,int *,const char ***);
TKGPU_HOST_API void TKGPU_HostFree(void *);
TKGPU_HOST_API int TKGPU_HostEvent(int flags);
TKGPU_HOST_API int TKGPU_HostWindows(void);
#ifdef __cplusplus
}
#endif
#endif
