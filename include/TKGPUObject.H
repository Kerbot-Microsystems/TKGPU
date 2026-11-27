#ifndef TKGPU_OBJECT_H
#define TKGPU_OBJECT_H
#include "TKGPURuntime.H"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct tg_object tg_object;
/* New objects are owned references. Retain/release are single-threaded. */
tg_object *tg_object_string(const char *bytes,size_t length);
tg_object *tg_object_integer(int64_t value);
tg_object *tg_object_retain(tg_object *o);
void tg_object_release(tg_object *o);
const char *tg_object_bytes(const tg_object *o,size_t *length);
int tg_object_as_integer(const tg_object *o,int64_t *value);
int tg_call_objects(tg_interp *i,size_t count,tg_object *const objects[]);
#ifdef __cplusplus
}
#endif
#endif
