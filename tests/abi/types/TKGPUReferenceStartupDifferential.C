#include <tcl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define TKGPU_STARTUP_API(n) candidate_##n
#include "TKGPUReferenceStartup.H"
static uint32_t next(uint32_t *s){*s^=*s<<13;*s^=*s>>17;*s^=*s<<5;return *s;}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"startup differential failed at %d\n",__LINE__);abort();}}while(0)
static void *run(void *context) {
    uint32_t seed=*(uint32_t *)context;
    Tcl_Obj *a[8],*b[8];
    for(int k=0;k<8;k++){a[k]=Tcl_NewIntObj(k);b[k]=Tcl_NewIntObj(k);Tcl_IncrRefCount(a[k]);Tcl_IncrRefCount(b[k]);}
    const char *encodings[]={NULL,"","utf-8","cp1251","custom","iso8859-1"};
    const char *x="sentinel",*y="sentinel";
    CHECK(!candidate_GetStartupScript(&x)&&!x&&!Tcl_GetStartupScript(&y)&&!y);
    for(int i=0;i<10000;i++){
        unsigned j=next(&seed)%9;const char *encoding=encodings[next(&seed)%6];
        char buffer[32];
        if(encoding)strcpy(buffer,encoding);
        Tcl_Obj *p=j<8?a[j]:NULL,*q=j<8?b[j]:NULL;
        candidate_SetStartupScript(p,encoding?buffer:NULL);Tcl_SetStartupScript(q,encoding?buffer:NULL);
        if(encoding)buffer[0]='Z';
        CHECK(candidate_GetStartupScript(&x)==p&&Tcl_GetStartupScript(&y)==q);
        CHECK((!x&&!y&&!encoding)||(x&&y&&encoding&&!strcmp(x,encoding)&&!strcmp(y,encoding)));
        CHECK(candidate_GetStartupScript(NULL)==p&&Tcl_GetStartupScript(NULL)==q);
        for(unsigned k=0;k<8;k++)CHECK(a[k]->refCount==1+(k==j)&&b[k]->refCount==a[k]->refCount);
    }
    candidate_SetStartupScript(NULL,NULL);Tcl_SetStartupScript(NULL,NULL);
    for(int k=0;k<8;k++){CHECK(a[k]->refCount==1&&b[k]->refCount==1);Tcl_DecrRefCount(a[k]);Tcl_DecrRefCount(b[k]);}
    return NULL;
}
int main(int argc,char **argv) {
    (void)argc;Tcl_FindExecutable(argv[0]);
    pthread_t threads[4];uint32_t seeds[]={1,2,3,4};
    for(int k=0;k<4;k++)CHECK(!pthread_create(threads+k,NULL,run,seeds+k));
    for(int k=0;k<4;k++)CHECK(!pthread_join(threads[k],NULL));
    puts("40000 threaded startup state scenarios passed");return 0;
}
