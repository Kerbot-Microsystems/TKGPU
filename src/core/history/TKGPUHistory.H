#ifndef TKGPU_HISTORY_H
#define TKGPU_HISTORY_H
#include "TKGPURuntime.H"
typedef struct tg_history tg_history;
tg_history *tg_history_create(void);
void tg_history_destroy(tg_history *);
int tg_history_record(tg_history *,const char *);
uint64_t tg_history_next(const tg_history *);
const char *tg_history_event(const tg_history *,int64_t);
void tg_history_clear(tg_history *);
int tg_history_command(tg_interp *,size_t,char **);
#endif
