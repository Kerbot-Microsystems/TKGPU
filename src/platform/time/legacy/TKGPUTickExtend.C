#include "TKGPUTickExtend.H"
uint64_t tg_tick_extend(tg_tick_state *s,uint32_t raw) {
    if(!s->initialized) {
        s->initialized=1; s->last=raw; s->elapsed=raw;
    } else {
        uint32_t delta=(uint32_t)(raw-s->last);
        if(UINT64_MAX-s->elapsed<delta) s->elapsed=UINT64_MAX;
        else s->elapsed+=delta;
        s->last=raw;
    }
    return s->elapsed;
}
