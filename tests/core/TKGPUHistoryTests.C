#include "../../src/core/history/TKGPUHistory.H"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(void) {
    tg_history *h=tg_history_create(); CHECK(h&&tg_history_next(h)==1);
    CHECK(tg_history_record(h," \n\t")==0&&tg_history_next(h)==1);
    CHECK(tg_history_record(h,"set x 1")==0);
    CHECK(tg_history_record(h,"set x 2")==0);
    CHECK(!strcmp(tg_history_event(h,-1),"set x 1"));
    CHECK(!strcmp(tg_history_event(h,0),"set x 2"));
    for(int k=0;k<21;k++) CHECK(tg_history_record(h,"test")==0);
    CHECK(!tg_history_event(h,1)&&tg_history_next(h)==24);
    CHECK(!tg_history_event(h,INT64_MIN));
    tg_history_clear(h); CHECK(tg_history_next(h)==1&&!strcmp(tg_history_event(h,0),""));
    tg_history_destroy(h); puts("history ring passed"); return 0;
}
