#include "TKGPUWindow.H"
#include "../../core/TKGPUCoreInternal.H"
#include "../../widgets/TKGPUWidgetInternal.H"
#include "../../platform/time/TKGPUClock.H"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

enum { AN_OPACITY,AN_BACKGROUND,AN_GROUP,AN_X,AN_Y,AN_W,AN_H };
enum { E_LINEAR,E_IN,E_OUT,E_INOUT };
enum { AN_LIMIT=128,AN_MAX_MS=3600000 };
typedef struct {
    tg_id id;
    unsigned property,easing;
    int from,to;
    uint64_t start,duration;
} animation;
struct tg_animation_state {
    Tcl_TimerToken timer;
    size_t count;
    animation items[AN_LIMIT];
};
static int property(const char *s) {
    static const char *names[]={"opacity","backgroundalpha","groupopacity","x","y","width","height"};
    for(int k=0;k<7;k++) if(!strcmp(s,names[k])) return k;
    return -1;
}
static int easing(const char *s) {
    static const char *names[]={"linear","ease-in","ease-out","ease-in-out"};
    for(int k=0;k<4;k++) if(!strcmp(s,names[k])) return k;
    return -1;
}
static int current(const tg_widget *w,unsigned prop,int *value) {
    if(!w||w->id==1||w->kind==TG_MENU||w->kind==TG_TOPLEVEL) return 0;
    if(prop==AN_OPACITY) *value=w->opacity;
    else if(prop==AN_BACKGROUND) *value=w->background_alpha;
    else if(prop==AN_GROUP) *value=w->group_opacity;
    else {
        if(w->layout!=TG_PLACE) return 0;
        *value=prop==AN_X?w->request.x:prop==AN_Y?w->request.y:prop==AN_W?w->request.w:w->request.h;
    }
    return 1;
}
static int apply(tg_window *w,tg_widget *widget,unsigned prop,int value) {
    int old;
    if(!current(widget,prop,&old)) return 0;
    if(old==value) return 1;
    if(prop==AN_OPACITY) widget->opacity=(unsigned char)value;
    else if(prop==AN_BACKGROUND) widget->background_alpha=(unsigned char)value;
    else if(prop==AN_GROUP) widget->group_opacity=(unsigned char)value;
    else {
        int *dest=prop==AN_X?&widget->request.x:prop==AN_Y?&widget->request.y:prop==AN_W?&widget->request.w:&widget->request.h;
        *dest=value; if(prop==AN_W) widget->place_flags|=1; if(prop==AN_H) widget->place_flags|=2; w->layout_dirty=1;
    }
    w->dirty=1; return 1;
}
static int interpolate(const animation *a,uint64_t now) {
    if(now<a->start) return a->from;
    uint64_t elapsed=now-a->start;
    if(elapsed>=a->duration) return a->to;
    double t=(double)elapsed/(double)a->duration;
    if(a->easing==E_IN) t*=t;
    else if(a->easing==E_OUT) t=t*(2.0-t);
    else if(a->easing==E_INOUT) t=t*t*(3.0-2.0*t);
    return (int)(a->from+(a->to-a->from)*t+0.5);
}
static int step(tg_window *w,uint64_t now) {
    struct tg_animation_state *s=w->animations;
    if(!s) return 0;
    tg_ui *u=tg_interp_ui(w->interp);
    size_t dst=0;
    for(size_t k=0;k<s->count;k++) {
        animation a=s->items[k];
        tg_widget *widget=tg_lookup(u,a.id);
        if(!apply(w,widget,a.property,interpolate(&a,now))) continue;
        if(now<a.start||now-a.start<a.duration) s->items[dst++]=a;
    }
    s->count=dst; return dst!=0;
}
void tg_animation_clear(tg_window *w) {
    if(!w||!w->animations) return;
    struct tg_animation_state *s=w->animations; w->animations=NULL;
    if(s->timer) Tcl_DeleteTimerHandler(s->timer);
    free(s);
}
static void tick(void *ctx);
static int schedule(tg_window *w) {
    struct tg_animation_state *s=w->animations;
    if(s->timer) return 1;
#ifdef TKGPU_LEGACY_WINDOWS
    int interval=33;
#else
    int interval=16;
#endif
    s->timer=Tcl_CreateTimerHandler(interval,tick,w);
    return s->timer!=NULL;
}
static void tick(void *ctx) {
    tg_window *w=ctx;
    if(!w->animations) return;
    w->animations->timer=NULL;
    uint64_t now;
    if(!w->alive||!tg_clock_now(&now)||!step(w,now)) { tg_animation_clear(w); return; }
    if(!schedule(w)) {
        /* Never leave tracks silently stuck after a notifier allocation failure. */
        struct tg_animation_state *s=w->animations;
        tg_ui *u=tg_interp_ui(w->interp);
        for(size_t k=0;k<s->count;k++) {
            animation *a=&s->items[k]; (void)apply(w,tg_lookup(u,a->id),a->property,a->to);
        }
        tg_animation_clear(w);
    }
}
static int error(Tcl_Interp *i) {
    return tg_set_result(i,"invalid animation, unsupported property or animation limit",TG_ERROR);
}
static int parse_target(const char *s,int prop,int *target) {
    char *end; errno=0;
    if(prop<=AN_GROUP) {
        double v=strtod(s,&end);
        if(errno||end==s||*end||!(v>=0&&v<=1)) return 0;
        *target=(int)(v*255.0+0.5);
    } else {
        long v=strtol(s,&end,10);
        if(errno||end==s||*end||v<0||v>TG_DIM_LIMIT) return 0;
        *target=(int)v;
    }
    return 1;
}
int tg_animation_command(Tcl_Interp *i,size_t n,char **a) {
    if(strcmp(a[0],"tkgpu::animate")) return -1;
    tg_window *w=Tk_MainWindow(i);
    if(!w||n<2) return error(i);
    struct tg_animation_state *s=w->animations;
    if(n==2&&!strcmp(a[1],"active")) {
        char b[32]; snprintf(b,sizeof b,"%lu",(unsigned long)(s?s->count:0));
        return tg_set_result(i,b,TG_OK);
    }
    if(!strcmp(a[1],"cancel")) {
        if(n==2) { tg_animation_clear(w); return tg_set_result(i,"",TG_OK); }
        if(n!=3&&n!=4) return error(i);
        tg_id id=tg_ui_find(tg_interp_ui(i),a[2]);
        int prop=n==4?property(a[3]):-1;
        if(!id||(n==4&&prop<0)) return error(i);
        if(s) {
            size_t dst=0;
            for(size_t k=0;k<s->count;k++) if(s->items[k].id!=id||(prop>=0&&s->items[k].property!=(unsigned)prop))
                s->items[dst++]=s->items[k];
            s->count=dst; if(!dst) tg_animation_clear(w);
        }
        return tg_set_result(i,"",TG_OK);
    }
    if((n!=6&&n!=7)||strcmp(a[1],"start")) return error(i);
    tg_widget *widget=tg_lookup(tg_interp_ui(i),tg_ui_find(tg_interp_ui(i),a[2]));
    int prop=property(a[3]),target,from,curve=n==7?easing(a[6]):E_INOUT;
    char *end; errno=0; long duration=strtol(a[5],&end,10);
    if(prop<0||curve<0||errno||end==a[5]||*end||duration<0||duration>AN_MAX_MS||
       !parse_target(a[4],prop,&target)||!current(widget,(unsigned)prop,&from)) return error(i);
    uint64_t now; if(!tg_clock_now(&now)) return error(i);
    if(s) { (void)step(w,now); (void)current(widget,(unsigned)prop,&from); }
    size_t at=0;
    if(s) for(;at<s->count;at++) if(s->items[at].id==widget->id&&s->items[at].property==(unsigned)prop) break;
    if(!duration||from==target) {
        (void)apply(w,widget,(unsigned)prop,target);
        if(s&&at<s->count) { s->items[at]=s->items[--s->count]; }
        if(s&&!s->count) tg_animation_clear(w);
        return tg_set_result(i,"",TG_OK);
    }
    if(!s) {
        s=calloc(1,sizeof *s); if(!s) return error(i);
        w->animations=s;
    }
    if(at==AN_LIMIT) return error(i);
    if(!schedule(w)) { if(!s->count) tg_animation_clear(w); return error(i); }
    s->items[at]=(animation){widget->id,(unsigned)prop,(unsigned)curve,from,target,now,(uint64_t)duration};
    if(at==s->count) s->count++;
    return tg_set_result(i,"",TG_OK);
}
