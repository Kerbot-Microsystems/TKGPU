#include "TKGPUCompat.H"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(void) {
    const unsigned char raw[]={0,0x41,0x80,0xff};
    Tcl_Obj *o=Tcl_NewByteArrayObj(raw,4); CHECK(o);
    int n=-1; unsigned char *p=Tcl_GetByteArrayFromObj(o,&n);
    CHECK(p&&n==4&&!memcmp(p,raw,4));
    CHECK(Tcl_GetByteArrayFromObj(o,NULL)==p);
    const unsigned char expected[]={0,0x41,0xc2,0x80,0xc3,0xbf};
    CHECK(Tcl_GetStringFromObj(o,&n)&&n==6&&!memcmp(Tcl_GetString(o),expected,6));
    p=Tcl_SetByteArrayLength(o,6); CHECK(p&&!memcmp(p,raw,4));
    p[0]='Z'; p[4]=0xc0; p[5]='Q';
    CHECK(Tcl_GetByteArrayFromObj(o,&n)==p&&n==6);
    const unsigned char grown[]={'Z','A',0xc2,0x80,0xc3,0xbf,0xc3,0x80,'Q'};
    CHECK(Tcl_GetStringFromObj(o,&n)&&n==9&&!memcmp(Tcl_GetString(o),grown,9));
    Tcl_InvalidateStringRep(o); CHECK(Tcl_GetStringFromObj(o,&n)&&n==9);
    CHECK(Tcl_GetByteArrayFromObj(o,&n)==p&&n==6);
    Tcl_Obj *copy=Tcl_DuplicateObj(o); CHECK(copy);
    p=Tcl_SetByteArrayLength(o,2); CHECK(p&&p[0]=='Z'&&p[1]=='A');
    CHECK(!strcmp(Tcl_GetString(o),"ZA"));
    CHECK(Tcl_GetByteArrayFromObj(copy,&n)&&n==6);
    TKGPU_ReleaseObject(copy); TKGPU_ReleaseObject(o);
    o=Tcl_NewStringObj("A\xc3\xbf",3); CHECK(o);
    p=Tcl_GetByteArrayFromObj(o,&n); CHECK(p&&n==2&&p[0]=='A'&&p[1]==255);
    TKGPU_ReleaseObject(o);
    o=Tcl_NewByteArrayObj(NULL,0); CHECK(o);
    CHECK(Tcl_GetByteArrayFromObj(o,&n)&&n==0); TKGPU_ReleaseObject(o);
    CHECK(!Tcl_NewByteArrayObj(NULL,1)&&!Tcl_NewByteArrayObj(raw,-1));
    puts("compat byte array checks passed"); return 0;
}
