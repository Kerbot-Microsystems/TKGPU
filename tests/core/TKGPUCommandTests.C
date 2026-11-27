#include "TKGPUCommand.H"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
typedef struct { int calls,disposed; } state;
static int callback(void *context,tg_interp *i,size_t argc,const char *const argv[]) {
    state *s=context; s->calls++;
    if(argc!=2) return TG_ERROR;
    if(tg_command_unregister(i,argv[0])!=TG_ERROR) return TG_ERROR;
    if(tg_eval(i,"set unexpected 1")!=TG_ERROR) return TG_ERROR;
    return tg_command_result(i,argv[1]);
}
static void dispose(void *context) { ((state *)context)->disposed++; }
int main(void) {
    tg_interp *i=tg_create(); CHECK(i); state s={0};
    CHECK(tg_command_register(i,"callback",callback,&s,dispose)==TG_OK);
    CHECK(tg_command_register(i,"callback",callback,&s,dispose)==TG_ERROR);
    CHECK(tg_eval(i,"set x [callback {literal $x}]")==TG_OK);
    CHECK(!strcmp(tg_result(i),"literal $x")&&s.calls==1);
    const char *args[]={"callback","[error nope]"};
    CHECK(tg_call(i,2,args)==TG_OK&&!strcmp(tg_result(i),args[1]));
    CHECK(tg_command_unregister(i,"callback")==TG_OK&&s.disposed==1);
    CHECK(tg_eval(i,"callback x")==TG_ERROR);
    CHECK(tg_command_register(i,"another",callback,&s,dispose)==TG_OK);
    for(int k=0;k<1024;k++) {
        char name[32]; snprintf(name,sizeof name,"callback_%d",k);
        CHECK(tg_command_register(i,name,callback,&s,dispose)==TG_OK);
    }
    CHECK(tg_eval(i,"callback_1023 ok")==TG_OK&&!strcmp(tg_result(i),"ok"));
    CHECK(tg_command_unregister(i,"callback_500")==TG_OK);
    CHECK(tg_eval(i,"callback_1023 still")==TG_OK&&!strcmp(tg_result(i),"still"));
    CHECK(tg_eval(i,"callback_500 missing")==TG_ERROR);
    for(int k=0;k<1024;k+=2) {
        if(k==500) continue;
        char name[32]; snprintf(name,sizeof name,"callback_%d",k);
        CHECK(tg_command_unregister(i,name)==TG_OK);
        CHECK(tg_command_register(i,name,callback,&s,dispose)==TG_OK);
    }
    for(int k=0;k<1024;k++) {
        char name[32]; snprintf(name,sizeof name,"callback_%d",k);
        const char *v[]={name,"verified"};
        if(k==500) CHECK(tg_call(i,2,v)==TG_ERROR);
        else CHECK(tg_call(i,2,v)==TG_OK&&!strcmp(tg_result(i),"verified"));
    }
    for(int k=0;k<1024;k++) {
        if(k==500) continue;
        char name[32]; snprintf(name,sizeof name,"callback_%d",k);
        CHECK(tg_command_unregister(i,name)==TG_OK);
    }
    CHECK(tg_eval(i,"another ok")==TG_OK&&!strcmp(tg_result(i),"ok"));
    tg_destroy(i); CHECK(s.disposed==1537);
    puts("command registry checks passed"); return 0;
}
