#include <tcl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define TKGPU_DEBUG_API(name) candidate_##name
#include "TKGPUReferenceDebug.H"
#define LOC __FILE__,__LINE__
#define CHECK(x) do { if(!(x)) {fprintf(stderr,"failure at line %d\n",__LINE__);return 1;} } while(0)
static uint32_t state=98713;
static uint32_t next(void){state^=state<<13;state^=state>>17;state^=state<<5;return state;}
static int same(Tcl_Obj *a,Tcl_Obj *b){
    int x=-1,y=-1; const char *p=Tcl_GetStringFromObj(a,&x),*q=Tcl_GetStringFromObj(b,&y);
    return x==y&&!memcmp(p,q,(size_t)x)&&a->refCount==b->refCount;
}
int main(int argc,char **argv){
    (void)argc; Tcl_FindExecutable(argv[0]);
    unsigned objectCases=0,allocationCases=0;
    for(unsigned i=0;i<20000;i++){
        unsigned n=next()%256+1,m=n+next()%256;
        char *a=candidate_DbCkalloc(n,LOC),*b=Tcl_DbCkalloc(n,LOC);
        CHECK(a&&b); memset(a,0xab,n);memset(b,0xab,n);
        a=candidate_DbCkrealloc(a,m,LOC);b=Tcl_DbCkrealloc(b,m,LOC);
        CHECK(a&&b&&!memcmp(a,b,n));candidate_DbCkfree(a,LOC);Tcl_DbCkfree(b,LOC);
        a=candidate_AttemptDbCkalloc(n,LOC);b=Tcl_AttemptDbCkalloc(n,LOC);
        CHECK(a&&b);memset(a,0xcd,n);memset(b,0xcd,n);
        a=candidate_AttemptDbCkrealloc(a,m,LOC);b=Tcl_AttemptDbCkrealloc(b,m,LOC);
        CHECK(a&&b&&!memcmp(a,b,n));candidate_DbCkfree(a,LOC);Tcl_DbCkfree(b,LOC);
        allocationCases+=2;
        unsigned char data[32]; for(size_t k=0;k<sizeof data;k++)data[k]=(unsigned char)next();
        int len=(int)(next()%33); int v=(int)(next()&0x7fffffff); Tcl_WideInt wide=(Tcl_WideInt)v*10000001-321;
        double real=(double)v/37.0;
        Tcl_Obj *aa[]={candidate_DbNewBooleanObj(v&1,LOC),candidate_DbNewByteArrayObj(data,len,LOC),candidate_DbNewDoubleObj(real,LOC),candidate_DbNewLongObj((long)v,LOC),candidate_DbNewObj(LOC),candidate_DbNewStringObj((const char *)data,len,LOC),candidate_DbNewWideIntObj(wide,LOC)};
        Tcl_Obj *bb[]={Tcl_DbNewBooleanObj(v&1,LOC),Tcl_DbNewByteArrayObj(data,len,LOC),Tcl_DbNewDoubleObj(real,LOC),Tcl_DbNewLongObj((long)v,LOC),Tcl_DbNewObj(LOC),Tcl_DbNewStringObj((const char *)data,len,LOC),Tcl_DbNewWideIntObj(wide,LOC)};
        for(size_t k=0;k<7;k++){
            CHECK(aa[k]&&bb[k]&&same(aa[k],bb[k])&&aa[k]->refCount==0);
            candidate_DbIncrRefCount(aa[k],LOC);Tcl_DbIncrRefCount(bb[k],LOC);
            CHECK(!candidate_DbIsShared(aa[k],LOC)&&!Tcl_DbIsShared(bb[k],LOC));
            candidate_DbIncrRefCount(aa[k],LOC);Tcl_DbIncrRefCount(bb[k],LOC);
            CHECK(candidate_DbIsShared(aa[k],LOC)&&Tcl_DbIsShared(bb[k],LOC));
            candidate_DbDecrRefCount(aa[k],LOC);Tcl_DbDecrRefCount(bb[k],LOC);
            CHECK(aa[k]->refCount==1&&bb[k]->refCount==1);
            ++objectCases;
        }
        Tcl_Obj *la=candidate_DbNewListObj(7,aa,LOC),*lb=Tcl_DbNewListObj(7,bb,LOC);
        CHECK(la&&lb&&same(la,lb)&&la->refCount==0);
        candidate_DbDecrRefCount(la,LOC);Tcl_DbDecrRefCount(lb,LOC);
        for(size_t k=0;k<7;k++){
            CHECK(aa[k]->refCount==1&&bb[k]->refCount==1);
            candidate_DbDecrRefCount(aa[k],LOC);Tcl_DbDecrRefCount(bb[k],LOC);
        }
        ++objectCases;
    }
    printf("%u allocation cases; %u object and lifetime cases passed\n",allocationCases,objectCases);return 0;
}
