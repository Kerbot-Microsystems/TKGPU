#include "TKGPUNotifier.H"
#include "script/TKGPUIdle.H"
#include "queue/TKGPUThreadEventQueue.H"
#include "file/TKGPUFileHandlers.H"
#include "../tk/TKGPUWindow.H"
#include "../../platform/time/TKGPUClock.H"
#include "TKGPUEventSource.H"
/* Posted events, timers, file/window readiness and idle callbacks. */
int tg_do_one_event(int flags,int queued_flags) {
    int categories=flags&~TKGPU_DONT_WAIT;
    if(!categories) categories=TKGPU_WINDOW_EVENTS|TKGPU_FILE_EVENTS|TKGPU_TIMER_EVENTS|TKGPU_IDLE_EVENTS;
    tg_notifier *s=tg_notifier_get(1);
    if(!s||s->busy) return 0;
    for(;;) {
        tg_native_mutex_lock(s->mailbox);
        uint64_t generation=s->generation;
        tg_native_mutex_unlock(s->mailbox);
        s->busy=1;
        int handled=tg_thread_event_dispatch(s,queued_flags);
        if(!handled&&s->sources) {
            tg_source_run(s,queued_flags,0);
            tg_source_run(s,queued_flags,1);
            handled=tg_thread_event_dispatch(s,queued_flags);
        }
        /* Prevent continuous animation/simulation timers and repaints from
           permanently starving after_idle callbacks (e.g. matplotlib draw_idle
           and gauge redraws). */
        static unsigned idle_starved=0;
        if(!handled&&(categories&TKGPU_IDLE_EVENTS)&&s->idle_head&&idle_starved>=4) {
            idle_starved=0;
            handled=tg_idle_dispatch(s);
        }
        if(!handled&&(categories&TKGPU_WINDOW_EVENTS)) handled=tg_window_dispatch();
        uint64_t now=0,deadline=0;
        int pending=(categories&TKGPU_TIMER_EVENTS)&&tg_timer_clock_now(&now)&&tg_timer_next(s->queue,&deadline);
        if(!handled&&pending&&deadline<=now) handled=tg_timer_dispatch_one(s->queue,now);
#ifndef _WIN32
        if(!handled&&(categories&TKGPU_FILE_EVENTS)) handled=tg_file_dispatch(s);
#endif
        if(!handled&&(categories&TKGPU_IDLE_EVENTS)) {
            idle_starved=0;
            handled=tg_idle_dispatch(s);
        } else if(handled&&(categories&TKGPU_IDLE_EVENTS)&&s->idle_head) {
            idle_starved++;
        } else if(!s->idle_head) {
            idle_starved=0;
        }
        s->busy=0;
        if(handled) {s->source_wait=-1;return 1;}
        if(flags&TKGPU_DONT_WAIT) {s->source_wait=-1;return 0;}
        int64_t wait=-1;
        if(pending) {
            if(!tg_timer_clock_now(&now)) return 0;
            uint64_t delay=deadline>now?deadline-now:0;
            uint64_t native_delay=tg_timer_wait_ms(delay);
            wait=(int64_t)(native_delay>1000?1000:native_delay);
        }
#ifndef _WIN32
        if((categories&TKGPU_FILE_EVENTS)&&tg_file_pending(s)&&(wait<0||wait>10)) wait=10;
#endif
        if((categories&TKGPU_WINDOW_EVENTS)&&tg_window_pending()&&(wait<0||wait>10)) wait=10;
        if(s->source_wait>=0&&(wait<0||s->source_wait<wait)) wait=s->source_wait;
        s->source_wait=-1;
        /* Clock can acquire the native global guard: never call it while holding mailbox. */
        tg_native_mutex_lock(s->mailbox);
        if(s->generation!=generation) { tg_native_mutex_unlock(s->mailbox); continue; }
        if(s->alerted) { s->alerted=0; tg_native_mutex_unlock(s->mailbox); return 0; }
        tg_native_condition_wait(s->wake,s->mailbox,wait);
        tg_native_mutex_unlock(s->mailbox);
    }
}
int Tcl_DoOneEvent(int flags) {
    int queued_flags=flags;
    if(!(queued_flags&~TKGPU_DONT_WAIT)) queued_flags|=~TKGPU_DONT_WAIT;
    return tg_do_one_event(flags,queued_flags);
}
