#include "TKGPUObject.H"
#include <stdio.h>
#include <inttypes.h>
#include <errno.h>
#include <string.h>
tg_object *tg_object_integer(int64_t value) {
    char b[64]; int n=snprintf(b,sizeof b,"%" PRId64,value);
    return n<0||n>=(int)sizeof b?NULL:tg_object_string(b,(size_t)n);
}
int tg_object_as_integer(const tg_object *o,int64_t *value) {
    size_t length; const char *s=tg_object_bytes(o,&length);
    if(!s||!value||!length||memchr(s,0,length)) return TG_ERROR;
    char *end; errno=0; intmax_t n=strtoimax(s,&end,10);
    if(errno||end==s||*end||n<INT64_MIN||n>INT64_MAX) return TG_ERROR;
    *value=(int64_t)n; return TG_OK;
}
