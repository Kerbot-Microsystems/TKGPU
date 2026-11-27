/* Deliberately include ONLY the independent host ABI. */
#include "TKGPUHost.H"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
static int disposed, self_disposed;
static void cleanup(void *p) { (*(int *)p)++; }
static int callback(void *p,TKGPUHost *h,size_t n,const char *const a[]) {
    (void)p;
    if(n!=2||strcmp(a[1],"literal $ [x]")) return TKGPU_HOST_ERROR;
    const char *v[]={"set","inner","42"};
    if(TKGPU_HostCall(h,3,v)!=TKGPU_HOST_OK) return TKGPU_HOST_ERROR;
    return TKGPU_HostSetResult(h,"callback result");
}
static int self_remove(void *p,TKGPUHost *h,size_t n,const char *const a[]) {
    (void)p; (void)n; (void)a;
    if(TKGPU_HostUnregister(h,"self_remove")||self_disposed) return TKGPU_HOST_ERROR;
    return TKGPU_HostSetResult(h,"retired");
}
int main(void) {
    CHECK(TKGPU_HostABI()==TKGPU_HOST_ABI);
    CHECK(TKGPU_HostABIRevision()>=4);
    int64_t number=0; double real=0; int boolean=0;
    CHECK(TKGPU_HostInteger("-9223372036854775808",&number)==0&&number==INT64_MIN);
    CHECK(TKGPU_HostInteger("9223372036854775808",&number)!=0);
    CHECK(TKGPU_HostInteger("0b101",&number)==0&&number==5);
    CHECK(TKGPU_HostDouble("2.5",&real)==0&&real==2.5);
    CHECK(TKGPU_HostBoolean("off",&boolean)==0&&!boolean);
    CHECK(TKGPU_HostBoolean("o",&boolean)!=0);
    const char *items[]={"one","two words","$literal"};
    char *joined=NULL;
    CHECK(TKGPU_HostJoin(3,items,&joined)==0&&joined);
    TKGPU_HostFree(joined);
    CHECK(TKGPU_HostVersion()&&*TKGPU_HostVersion());
    TKGPUHost *h=TKGPU_HostCreate(),*b=TKGPU_HostCreate();
    CHECK(h&&b);
    CHECK(TKGPU_HostInit(h,0)==0&&TKGPU_HostInit(b,0)==0);
    const char *v[]={"set","x","a $ [b] ; c"};
    CHECK(TKGPU_HostCall(h,3,v)==0&&!strcmp(TKGPU_HostResult(h),v[2]));
    int status=-1;
    const char *fast=TKGPU_HostCallResult(h,3,v,&status);
    CHECK(status==0&&fast&&!strcmp(fast,v[2]));
    int kind=-1;
    const char *numeric[]={"set","numeric","007"};
    fast=TKGPU_HostCallTagged(h,3,numeric,&status,&kind);
    CHECK(status==0&&kind==TKGPU_HOST_STRING&&!strcmp(fast,"007"));
    const char *length[]={"llength","one {two words}"};
    fast=TKGPU_HostCallTagged(h,2,length,&status,&kind);
    CHECK(status==0&&kind==TKGPU_HOST_INTEGER&&!strcmp(fast,"2"));
    const char *literal_list[]={"list","one","two words"};
    fast=TKGPU_HostCallTagged(h,3,literal_list,&status,&kind);
    CHECK(status==0&&kind==TKGPU_HOST_LIST&&!strcmp(fast,"one {two words}"));
    const char *missing[]={"winfo","exists",".absent"};
    fast=TKGPU_HostCallTagged(h,3,missing,&status,&kind);
    CHECK(status==0&&kind==TKGPU_HOST_INTEGER&&!strcmp(fast,"0"));
    const char *listbox[]={"listbox",".rows"};
    CHECK(TKGPU_HostCall(h,2,listbox)==0);
    const char *insert[]={".rows","insert","end","007","two words"};
    CHECK(TKGPU_HostCall(h,5,insert)==0);
    const char *size[]={".rows","size"};
    fast=TKGPU_HostCallTagged(h,2,size,&status,&kind);
    CHECK(status==0&&kind==TKGPU_HOST_INTEGER&&!strcmp(fast,"2"));
    const char *get[]={".rows","get","0"};
    fast=TKGPU_HostCallTagged(h,3,get,&status,&kind);
    CHECK(status==0&&kind==TKGPU_HOST_STRING&&!strcmp(fast,"007"));
    const char *range[]={".rows","get","0","end"};
    fast=TKGPU_HostCallTagged(h,4,range,&status,&kind);
    CHECK(status==0&&kind==TKGPU_HOST_LIST&&!strcmp(fast,"007 {two words}"));
    const char *themes[]={"ttk::style","theme","names"};
    fast=TKGPU_HostCallTagged(h,3,themes,&status,&kind);
    CHECK(status==0&&kind==TKGPU_HOST_LIST&&strstr(fast,"clam"));
    const char *button[]={"ttk::button",".statebutton"};
    CHECK(TKGPU_HostCall(h,2,button)==0);
    const char *disable[]={".statebutton","state","disabled"};
    fast=TKGPU_HostCallTagged(h,3,disable,&status,&kind);
    CHECK(status==0&&kind==TKGPU_HOST_LIST);
    const char *instate[]={".statebutton","instate","disabled"};
    fast=TKGPU_HostCallTagged(h,3,instate,&status,&kind);
    CHECK(status==0&&kind==TKGPU_HOST_BOOLEAN&&!strcmp(fast,"1"));
    const char *bad[]={"error","expected failure"};
    fast=TKGPU_HostCallResult(h,2,bad,&status);
    CHECK(status!=0&&fast&&!strcmp(fast,bad[1]));
    const char *image[]={"image","create","photo","testphoto","-width","2","-height","1"};
    CHECK(TKGPU_HostCall(h,8,image)==0);
    const unsigned char pixels[]={1,2,3,0,4,5,6,255};
    unsigned char copied[8]={0};
    CHECK(TKGPU_HostPhotoWrite(h,"testphoto",2,1,pixels,sizeof pixels)==0);
    CHECK(TKGPU_HostPhotoRead(h,"testphoto",2,1,copied,sizeof copied)==0&&!memcmp(pixels,copied,sizeof pixels));
    CHECK(TKGPU_HostPhotoWrite(h,"testphoto",2,1,pixels,7)!=0);
    CHECK(TKGPU_HostPhotoRead(h,"testphoto",1,2,copied,sizeof copied)!=0);
    CHECK(TKGPU_HostEval(b,"info exists x")==0&&!strcmp(TKGPU_HostResult(b),"0"));
    CHECK(TKGPU_HostRegister(h,"host_callback",callback,&disposed,cleanup)==0);
    const char *c[]={"host_callback","literal $ [x]"};
    CHECK(TKGPU_HostCall(h,2,c)==0&&!strcmp(TKGPU_HostResult(h),"callback result"));
    CHECK(TKGPU_HostEval(h,"set inner")==0&&!strcmp(TKGPU_HostResult(h),"42"));
    int n=0; const char **parts=NULL;
    CHECK(TKGPU_HostSplit(h,"one {two words}",&n,&parts)==0&&n==2&&!strcmp(parts[1],"two words"));
    TKGPU_HostFree((void *)parts);
    CHECK(TKGPU_HostUnregister(h,"host_callback")==0&&disposed==1);
    CHECK(TKGPU_HostRegister(h,"self_remove",self_remove,&self_disposed,cleanup)==0);
    CHECK(TKGPU_HostEval(h,"self_remove")==0&&!strcmp(TKGPU_HostResult(h),"retired")&&self_disposed==1);
    CHECK(TKGPU_HostEval(h,"list 1 2 3 4 5 6 7 8 9 10 11 [list nested words]")==0);
    CHECK(!strcmp(TKGPU_HostResult(h),"1 2 3 4 5 6 7 8 9 10 11 {nested words}"));
    CHECK(TKGPU_HostEval(h,"list [list 1 2 3 4 5 6 7 8 9 10] end")==0);
    CHECK(!strcmp(TKGPU_HostResult(h),"{1 2 3 4 5 6 7 8 9 10} end"));
    CHECK(TKGPU_HostClose(h)==0&&TKGPU_HostClose(b)==0);
    puts("Independent TKGPU host ABI checks passed"); return 0;
}
