#include "TKGPUAfter.H"
#include "TKGPUIdle.H"
#include "../../tk/TKGPUWindow.H"
#include "../../../core/TKGPUCoreInternal.H"
#include "../../../core/TKGPUList.H"
#include "../../../platform/time/TKGPUClock.H"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <inttypes.h>
typedef struct tg_after_task {
    struct tg_after_task *next;
    Tcl_Interp *interp;
    Tcl_TimerToken timer;
    uint64_t idle;
    char name[32];
    char *script;
} task;
static void remove_task(task *t,int cancel) {
    task **link=&t->interp->after_tasks;
    while(*link&&*link!=t) link=&(*link)->next;
    if(*link) *link=t->next;
    if(cancel) {
        if(t->timer) Tcl_DeleteTimerHandler(t->timer);
        if(t->idle) tg_idle_cancel(t->idle);
    }
}
static void fire(void *p) {
    task *t=p; Tcl_Interp *i=t->interp;
    remove_task(t,0);
#ifdef TKGPU_REFERENCE_STUBS
    tg_compat_lifetime_enter(i);
#endif
    const char *s=t->script;
    int rc=tg_eval_script(i,&s,0);
    /* Background errors do not change update's return code. */
    if(rc!=TG_OK&&!i->deleted) (void)tg_var_store(i,"errorInfo",i->result);
    free(t->script); free(t);
#ifdef TKGPU_REFERENCE_STUBS
    tg_compat_lifetime_leave(i);
#endif
}
void tg_after_dispose(Tcl_Interp *i) {
    while(i->after_tasks) {
        task *t=i->after_tasks; remove_task(t,1); free(t->script); free(t);
    }
}
static int err(Tcl_Interp *i) { return tg_set_result(i,"invalid or unsupported after/update command",TG_ERROR); }
static int delay(const char *s,int *ms) {
    char *end; errno=0; long x=strtol(s,&end,10);
    if(errno||end==s||*end||x>INT_MAX||x<INT_MIN) return 0;
    *ms=x<0?0:(int)x; return 1;
}
static int schedule(Tcl_Interp *i,int idle,int ms,const char *script) {
    tg_notifier *s=tg_notifier_get(1);
    if(!s||s->after_next==UINT64_MAX) return err(i);
    task *t=calloc(1,sizeof *t); if(!t) return err(i);
    t->interp=i; t->script=tg_copy_string(script);
    if(!t->script) { free(t); return err(i); }
    snprintf(t->name,sizeof t->name,"after#%" PRIu64,s->after_next++);
    if(idle) t->idle=tg_idle_add(fire,t);
    else t->timer=Tcl_CreateTimerHandler(ms,fire,t);
    if(!t->idle&&!t->timer) { free(t->script); free(t); return err(i); }
    t->next=i->after_tasks; i->after_tasks=t;
    return tg_set_result(i,t->name,TG_OK);
}
/* A controlled owner-thread wait, not an unrestricted recursive notifier.
   Watch writes/unsets rather than comparing values: same-value writes count. */
static int wait_command(Tcl_Interp *i,size_t n,char **a) {
    int variable=!strcmp(a[0],"vwait"),visibility=n==3&&!strcmp(a[1],"visibility");
    if((variable&&n!=2)||(!variable&&n!=3))
        return tg_set_result(i,"invalid wait arguments",TG_ERROR);
    if(!variable) {
        if(!strcmp(a[1],"variable")) variable=1;
        else if(strcmp(a[1],"window")&&!visibility)
            return tg_set_result(i,"unsupported tkwait mode",TG_ERROR);
    }
    tg_notifier *s=tg_notifier_get(1);
    if(!s||i->event_pump_depth>=16)
        return tg_set_result(i,"nested wait depth limit reached",TG_ERROR);
    const char *name=a[n-1];
    tg_id id=0;
    tg_window *root=Tk_MainWindow(i);
    if(!variable) {
        id=tg_ui_find(i->ui,name);
        if(!id||(!strcmp(name,".")&&!root))
            return tg_set_result(i,"unknown wait window",TG_ERROR);
    } else {
        if(name[0]==':'&&name[1]==':') name+=2;
        if(!*name||strstr(name,"::"))
            return tg_set_result(i,"unsupported wait variable name",TG_ERROR);
    }
    tg_var_wait watch={i->var_waits,name,0};
    if(variable) i->var_waits=&watch;
    int saved_busy=s->busy;
    s->busy=0;
    i->event_pump_depth++;
    int cancelled=0;
    for(;;) {
        if(visibility) { tg_window *w=tg_window_for_widget(i,name); if(!w) { cancelled=1; break; } if(!w->withdrawn) break; }
        if(variable?watch.changed:(!tg_ui_get(i->ui,id)||(id==1&&!Tk_MainWindow(i)))) break;
        if(i->disposing||(root&&!Tk_MainWindow(i))) { cancelled=1; break; }
        if(!Tcl_DoOneEvent(TKGPU_DONT_WAIT)) tg_clock_sleep(5);
    }
    i->event_pump_depth--;
    s->busy=saved_busy;
    if(variable) i->var_waits=watch.next;
    return tg_set_result(i,cancelled?"wait interrupted by root close":"",cancelled?TG_ERROR:TG_OK);
}
int tg_after_command(Tcl_Interp *i,size_t n,char **a) {
    if(!strcmp(a[0],"vwait")||!strcmp(a[0],"tkwait")) return wait_command(i,n,a);
    if(!strcmp(a[0],"update")) {
        if(n>2||(n==2&&strcmp(a[1],"idletasks"))) return err(i);
        tg_notifier *s=tg_notifier_get(1);
        if(!s) return err(i);
        if(i->event_pump_depth>=16) return tg_set_result(i,"",TG_OK);
        int flags=TKGPU_DONT_WAIT|(n==2?TKGPU_IDLE_EVENTS:0);
        int saved_busy=s->busy;
        s->busy=0;
        i->event_pump_depth++;
        unsigned k=0;
        for(;k<4096;k++) if(!Tcl_DoOneEvent(flags)) break;
        i->event_pump_depth--;
        s->busy=saved_busy;
        return k<4096?tg_set_result(i,"",TG_OK):tg_set_result(i,"update event limit reached",TG_ERROR);
    }
    if(strcmp(a[0],"after")) return -1;
    if(n<2) return err(i);
    if(!strcmp(a[1],"idle")) return n==3?schedule(i,1,0,a[2]):err(i);
    if(!strcmp(a[1],"cancel")) {
        if(n!=3) return err(i);
        for(task *t=i->after_tasks;t;t=t->next) if(!strcmp(a[2],t->name)||!strcmp(a[2],t->script)) {
            remove_task(t,1); free(t->script); free(t); break;
        }
        return tg_set_result(i,"",TG_OK);
    }
    if(!strcmp(a[1],"info")) {
        char b[MAX_WORD]="";
        if(n==2) {
            for(task *t=i->after_tasks;t;t=t->next) if(!tg_list_append(b,sizeof b,t->name)) return err(i);
            return tg_set_result(i,b,TG_OK);
        }
        if(n!=3) return err(i);
        for(task *t=i->after_tasks;t;t=t->next) if(!strcmp(a[2],t->name)) {
            if(!tg_list_append(b,sizeof b,t->script)||!tg_list_append(b,sizeof b,t->idle?"idle":"timer")) return err(i);
            return tg_set_result(i,b,TG_OK);
        }
        return err(i);
    }
    int ms;
    if(!delay(a[1],&ms)) return err(i);
    if(n==3) return schedule(i,0,ms,a[2]);
    if(n!=2) return err(i);
    Tcl_Sleep(ms);
    return tg_set_result(i,"",TG_OK);
}
