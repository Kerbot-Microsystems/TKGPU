#include "../../../src/platform/time/legacy/TKGPUTickExtend.H"
#include <stdio.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(void) {
    tg_tick_state s={0};
    CHECK(tg_tick_extend(&s,UINT32_MAX-2)==UINT32_MAX-2);
    CHECK(tg_tick_extend(&s,UINT32_MAX)==UINT32_MAX);
    CHECK(tg_tick_extend(&s,0)==UINT64_C(4294967296));
    CHECK(tg_tick_extend(&s,3)==UINT64_C(4294967299));
    CHECK(tg_tick_extend(&s,3)==UINT64_C(4294967299));
    s.elapsed=UINT64_MAX-1;
    CHECK(tg_tick_extend(&s,5)==UINT64_MAX);
    puts("legacy tick extension passed"); return 0;
}
