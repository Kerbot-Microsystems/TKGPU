#include "TKGPUCompat.H"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
static int value(const char *text,const char *expected) {
    Tcl_Obj *o=Tcl_NewStringObj(text,-1); mp_int n;
    if(!o) return 0;
    int rc=Tcl_GetBignumFromObj(NULL,o,&n); TclFreeObj(o);
    if(!expected) return rc!=0;
    if(rc) return 0;
    Tcl_Obj *v=Tcl_NewBignumObj(&n);
    int ok=v&&!strcmp(Tcl_GetString(v),expected);
    if(v) TclFreeObj(v); else TclBN_mp_clear(&n);
    return ok;
}
int main(void) {
    CHECK(value(" +123 ","123")); CHECK(value("0xFF","255"));
    CHECK(value("-0b101","-5")); CHECK(value("0o77","63"));
    CHECK(value("077","63")); CHECK(value("08",NULL));
    CHECK(value("0x",NULL)); CHECK(value("-0","0"));
    mp_int n; CHECK(TclBN_mp_init(&n)==0);
    CHECK(TclBN_mp_read_radix(&n,"99",10)==0);
    CHECK(TclBN_mp_read_radix(&n,"123x",10)==TKGPU_MP_VAL&&!n.used&&!n.sign);
    CHECK(TclBN_mp_read_radix(&n,"123\nignored",10)==0&&n.used==1&&n.dp[0]==123);
    TclBN_mp_clear(&n); puts("bignum syntax passed"); return 0;
}
