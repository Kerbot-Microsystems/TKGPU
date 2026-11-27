#ifndef TKGPU_NOTIFIER_H
#define TKGPU_NOTIFIER_H
#include "TKGPUTimer.H"
#include "TKGPUCompat.H"
#include "../../platform/threads/TKGPUThreadNative.H"
enum { TG_NOTIFIER_LIMIT=256 };
typedef struct { tg_timer_id id; Tcl_TimerProc *fn; void *context; uintptr_t token; } tg_notifier_timer;
#ifndef _WIN32
typedef struct { int fd,mask; Tcl_FileProc *fn; void *context; } tg_notifier_file;
#endif
typedef struct tg_notifier_idle {
    struct tg_notifier_idle *next;
    uint64_t id; Tcl_TimerProc *fn; void *context;
} tg_notifier_idle;
struct tg_event_source;
struct tg_event_delete_guard;
typedef struct tg_notifier {
    tg_timers *queue; tg_notifier_timer slots[TG_NOTIFIER_LIMIT];
    uintptr_t next; int busy,service_mode;
    uint64_t idle_next,after_next;
    tg_notifier_idle *idle_head,*idle_tail;
    Tcl_ThreadId owner;
    tg_native_mutex *mailbox; tg_native_condition *wake;
    Tcl_Event *head,*tail,*marker,*active_event;
    int active_retired;
    struct tg_event_delete_guard *delete_guards;
    size_t event_count;
    uint64_t generation;
    int alerted;
    int timer_set; uint64_t notifier_deadline;
    struct tg_event_source *sources;
    unsigned source_depth;
    int64_t source_wait;
#ifndef _WIN32
    tg_notifier_file files[TG_NOTIFIER_LIMIT];
    size_t file_count,file_cursor;
#endif
    struct tg_notifier *registry_next;
} tg_notifier;
int tg_do_one_event(int flags,int queued_flags);
void tg_source_run(tg_notifier *,int,int);
void tg_source_clear(tg_notifier *);
void tg_notifier_register(tg_notifier *s);
void tg_notifier_unregister(tg_notifier *s);
tg_notifier *tg_notifier_get(int create);
void tg_notifier_destroy(void *state);
int tg_notifier_finalize_current(void);
#endif
