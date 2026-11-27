#include "TKGPUObject.H"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(void) {
    tg_object *number=tg_object_integer(INT64_MIN); CHECK(number);
    int64_t value=0; CHECK(tg_object_as_integer(number,&value)==TG_OK&&value==INT64_MIN);
    CHECK(tg_object_retain(number)==number); tg_object_release(number);
    CHECK(tg_object_as_integer(number,&value)==TG_OK);
    tg_object_release(number);
    const char bytes[]={'a',0,'b'};
    tg_object *binary=tg_object_string(bytes,sizeof bytes); CHECK(binary);
    size_t n=0; const char *p=tg_object_bytes(binary,&n);
    CHECK(n==3&&!memcmp(p,bytes,3)&&p[3]==0);
    CHECK(tg_object_as_integer(binary,&value)==TG_ERROR);
    tg_interp *i=tg_create(); CHECK(i);
    tg_object *args[]={tg_object_string("set",3),tg_object_string("x",1),binary};
    CHECK(args[0]&&args[1]);
    CHECK(tg_call_objects(i,3,args)==TG_ERROR);
    args[2]=tg_object_string("[error nope]",12); CHECK(args[2]);
    CHECK(tg_call_objects(i,3,args)==TG_OK);
    CHECK(!strcmp(tg_result(i),"[error nope]"));
    for(size_t k=0;k<3;k++) tg_object_release(args[k]);
    tg_object_release(binary); tg_destroy(i);
    puts("object checks passed"); return 0;
}
