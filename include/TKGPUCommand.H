#ifndef TKGPU_COMMAND_H
#define TKGPU_COMMAND_H
#include "TKGPURuntime.H"
#ifdef __cplusplus
extern "C" {
#endif
typedef int (*tg_command_fn)(void *context,tg_interp *i,size_t argc,const char *const argv[]);
typedef void (*tg_command_dispose_fn)(void *context);
/* Names are unique. Registration and deletion are idle-only operations.
   Callbacks are synchronous; they may set a result but cannot reenter tg_eval/tg_call. */
int tg_command_register(tg_interp *i,const char *name,tg_command_fn fn,void *context,tg_command_dispose_fn dispose);
int tg_command_unregister(tg_interp *i,const char *name);
int tg_command_result(tg_interp *i,const char *value);
#ifdef __cplusplus
}
#endif
#endif
