#include <tcl.h>
#include <stdint.h>
#include <stdlib.h>
typedef struct tg_event_source tg_event_source;
typedef struct {tg_event_source *sources;unsigned source_depth;int64_t source_wait;} tg_notifier;
static __thread tg_notifier state={0,0,-1};
static tg_notifier *tg_notifier_get(int create){(void)create;return &state;}
#define TKGPU_DONT_WAIT 2
#define TKGPU_IDLE_EVENTS 32
#define SOURCE_API(n) candidate_##n
#include "../../../src/compat/events/TKGPUEventSource.H"
int candidate_pass(int flags){if(!(flags&~2))flags|=~2;tg_source_run(&state,flags,0);tg_source_run(&state,flags,1);return 0;}
int64_t candidate_wait(void){return state.source_wait;}
void candidate_reset(void){tg_source_clear(&state);state.source_wait=-1;}
