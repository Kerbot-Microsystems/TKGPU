#ifndef TKGPU_THREAD_EVENT_QUEUE_H
#define TKGPU_THREAD_EVENT_QUEUE_H
#include "../TKGPUNotifier.H"
/* All operations except dispatch require the mailbox lock. */
void tg_thread_event_insert(tg_notifier *s,Tcl_Event *event,Tcl_QueuePosition position);
int tg_thread_event_dispatch(tg_notifier *s,int flags);
int tg_thread_event_remove(tg_notifier *,Tcl_Event *);
int tg_event_deleting(tg_notifier *,Tcl_Event *);
void tg_thread_event_clear(tg_notifier *s);
#endif
