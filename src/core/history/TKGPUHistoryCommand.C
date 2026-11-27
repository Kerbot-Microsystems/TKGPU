#include "TKGPUHistory.H"
#include "../TKGPUCoreInternal.H"
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <errno.h>
#include <string.h>
int tg_history_command(tg_interp *i,size_t n,char **a) {
    if(strcmp(a[0],"history")) return -1;
    if(n==2&&!strcmp(a[1],"nextid")) {
        char b[32]; snprintf(b,sizeof b,"%" PRIu64,tg_history_next(i->history));
        return tg_set_result(i,b,TG_OK);
    }
    if(n==2&&!strcmp(a[1],"clear")) {
        tg_history_clear(i->history); return tg_set_result(i,"",TG_OK);
    }
    if((n==2||n==3)&&!strcmp(a[1],"event")) {
        int64_t event=-1;
        if(n==3) {
            char *end; errno=0; intmax_t v=strtoimax(a[2],&end,10);
            if(errno||end==a[2]||*end||v<INT64_MIN||v>INT64_MAX)
                return tg_set_result(i,"unsupported history event",TG_ERROR);
            event=(int64_t)v;
        }
        const char *s=tg_history_event(i->history,event);
        return s?tg_set_result(i,s,TG_OK):tg_set_result(i,"history event unavailable",TG_ERROR);
    }
    return tg_set_result(i,"unsupported history command",TG_ERROR);
}
