#ifndef TKGPU_THREAD_NATIVE_H
#define TKGPU_THREAD_NATIVE_H
#include <stdint.h>
#include <stddef.h>
typedef struct tg_native_mutex tg_native_mutex;
typedef struct tg_native_condition tg_native_condition;
typedef struct tg_native_tls tg_native_tls;
void tg_native_guard_lock(void);
void tg_native_guard_unlock(void);
tg_native_mutex *tg_native_mutex_create(void);
void tg_native_mutex_destroy(tg_native_mutex *);
void tg_native_mutex_lock(tg_native_mutex *);
void tg_native_mutex_unlock(tg_native_mutex *);
tg_native_condition *tg_native_condition_create(void);
void tg_native_condition_destroy(tg_native_condition *);
void tg_native_condition_notify(tg_native_condition *);
/* -1 = indefinite, otherwise relative milliseconds. */
void tg_native_condition_wait(tg_native_condition *,tg_native_mutex *,int64_t);
tg_native_tls *tg_native_tls_create(void);
void *tg_native_tls_get(tg_native_tls *);
int tg_native_tls_set(tg_native_tls *,void *);
void *tg_native_thread_id(void);
#ifdef TKGPU_LEGACY_WINDOWS
void tg_native_tls_finalize_thread(void);
#endif
#endif
