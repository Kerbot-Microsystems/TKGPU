#ifndef TKGPU_WORKER_H
#define TKGPU_WORKER_H
typedef struct tg_worker tg_worker;
typedef void (*tg_worker_fn)(void *);
/* Owner-only control; one outstanding task per worker. */
tg_worker *tg_worker_create(void);
void tg_worker_submit(tg_worker *,tg_worker_fn,void *);
void tg_worker_wait(tg_worker *);
int tg_worker_try_wait(tg_worker *);
void tg_worker_destroy(tg_worker *);
unsigned tg_worker_cpu_count(void);
#endif
