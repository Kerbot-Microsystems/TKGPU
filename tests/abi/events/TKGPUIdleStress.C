/* Link the production idle queue against a private notifier fixture.
   No stock Tcl allocator, event loop or UI is needed for this test. */
#include "../../../src/compat/events/script/TKGPUIdle.H"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
static tg_notifier state;
static unsigned called;
tg_notifier *tg_notifier_get(int create) { (void)create;return &state; }
void Tcl_Panic(const char *format,...) { (void)format;abort(); }
static void plain(void *p) { (void)p;++called; }
static void schedule(void *p) {
    (void)p;++called;
    Tcl_DoWhenIdle(plain,NULL);
    Tcl_CancelIdleCall(plain,(void *)(uintptr_t)1);
}
int main(void) {
    for(int k=0;k<1000;k++) {
        called=0;
        Tcl_DoWhenIdle(schedule,NULL);
        for(int j=0;j<1000;j++)Tcl_DoWhenIdle(plain,(void *)(uintptr_t)1);
        assert(tg_idle_dispatch(&state)==1&&called==1);
        assert(tg_idle_dispatch(&state)==1&&called==2);
        assert(!tg_idle_dispatch(&state));
        for(int j=0;j<1000;j++)Tcl_DoWhenIdle(plain,NULL);
        tg_idle_clear(&state);
        assert(!tg_idle_dispatch(&state));
        assert(!state.idle_head&&!state.idle_tail);
    }
    /* Check the script-facing ID cancellation and tail repair too. */
    uint64_t a=tg_idle_add(plain,NULL),b=tg_idle_add(plain,NULL),c=tg_idle_add(plain,NULL);
    assert(a&&b&&c);
    tg_idle_cancel(b);tg_idle_cancel(c);
    assert(state.idle_head==state.idle_tail&&state.idle_head->id==a);
    uint64_t d=tg_idle_add(plain,NULL);assert(d&&state.idle_tail->id==d);
    tg_idle_cancel(a);tg_idle_cancel(d);
    assert(!state.idle_head&&!state.idle_tail);
    puts("Idle queue: 1000 cycles, two million pending nodes canceled/cleared");
    return 0;
}
