#include "TKGPUCompat.H"
#include <stdio.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(void) {
    mp_int n; CHECK(TclBN_mp_init(&n)==0);
    CHECK(TclBN_mp_read_radix(&n,"10000000",16)==0&&n.used==2&&n.dp[0]==0&&n.dp[1]==1);
    CHECK(TclBN_mp_read_radix(&n,"-123456789abcdef0123456789",16)==0&&n.sign==1);
    unsigned char b[32]; unsigned long len=sizeof b;
    const unsigned char expected[]={1,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,1,0x23,0x45,0x67,0x89};
    CHECK(TclBN_mp_unsigned_bin_size(&n)==sizeof expected);
    CHECK(TclBN_mp_to_unsigned_bin_n(&n,b,&len)==0&&len==sizeof expected&&!memcmp(b,expected,len));
    len=1; CHECK(TclBN_mp_to_unsigned_bin_n(&n,b,&len)==TKGPU_MP_VAL&&len==1);
    Tcl_Obj *o=Tcl_NewBignumObj(&n); CHECK(o&&!n.dp);
    CHECK(!strcmp(Tcl_GetString(o),"-90144042682896311822508713865"));
    CHECK(Tcl_GetBignumFromObj(NULL,o,&n)==0&&n.sign==1);
    TKGPU_ReleaseObject(o);
    TclBN_mp_clear(&n);
    CHECK(TclBN_mp_init(&n)==0&&TclBN_mp_read_radix(&n,"-0",10)==0&&!n.used&&!n.sign);
    len=0; CHECK(TclBN_mp_to_unsigned_bin_n(&n,NULL,&len)==0&&len==0);
    TclBN_mp_clear(&n);
    CHECK(Tcl_InitBignumFromDouble(NULL,-123456789.75,&n)==0);
    o=Tcl_DbNewBignumObj(&n,__FILE__,__LINE__);
    CHECK(o&&!strcmp(Tcl_GetString(o),"-123456789")&&!n.dp);
    CHECK(Tcl_TakeBignumFromObj(NULL,o,&n)==0&&n.sign==1);
    CHECK(!strcmp(Tcl_GetString(o),"-123456789")); TclBN_mp_clear(&n);
    CHECK(Tcl_InitBignumFromDouble(NULL,0x1p100,&n)==0);
    Tcl_SetBignumObj(o,&n);
    CHECK(!n.dp&&!strcmp(Tcl_GetString(o),"1267650600228229401496703205376"));
    CHECK(Tcl_GetBignumFromObj(NULL,o,&n)==0&&n.used==4&&n.dp[3]==65536);
    TclBN_mp_clear(&n); TKGPU_ReleaseObject(o);
    CHECK(Tcl_InitBignumFromDouble(NULL,NAN,&n)==0&&!n.used&&!n.sign);
    TclBN_mp_clear(&n);
    CHECK(Tcl_InitBignumFromDouble(NULL,INFINITY,&n)!=0);
    puts("bignum ABI subset passed"); return 0;
}
