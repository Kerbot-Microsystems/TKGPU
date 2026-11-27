#include "TKGPUCompat.H"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(void) {
    Tcl_Interp *i=Tcl_CreateInterp(); CHECK(i);
    int n=-1; const char **v=NULL;
    CHECK(Tcl_SplitList(i,"a {two words} {}",&n,&v)==0);
    CHECK(n==3&&!strcmp(v[0],"a")&&!strcmp(v[1],"two words")&&!strcmp(v[2],"")&&!v[3]);
    Tcl_Free((char *)v);
    CHECK(Tcl_SplitList(i,"",&n,&v)==0&&n==0&&!v[0]); Tcl_Free((char *)v);
    n=-1; v=NULL;
    CHECK(Tcl_SplitList(i,"{broken",&n,&v)!=0&&n==-1&&!v);
    CHECK(Tcl_SplitList(i,"\\x41\\x4142 \\u0416 \\377 \\400 \\777 \\x \\u {\\u0416}",&n,&v)==0);
    CHECK(n==8&&!strcmp(v[0],"AA42")&&!strcmp(v[1],"\xd0\x96"));
    CHECK(!strcmp(v[2],"\xc3\xbf")&&!strcmp(v[3]," 0")&&!strcmp(v[4],"?7"));
    CHECK(!strcmp(v[5],"x")&&!strcmp(v[6],"u")&&!strcmp(v[7],"\\u0416"));
    Tcl_Free((char *)v); n=-1; v=NULL;
    /* Tcl's modified UTF-8 represents NUL and isolated UTF-16 surrogates. */
    CHECK(Tcl_SplitList(i,"\\0",&n,&v)==0&&n==1&&!strcmp(v[0],"\xc0\x80"));
    Tcl_Free((char *)v);
    CHECK(Tcl_SplitList(i,"\\ud800",&n,&v)==0&&n==1&&!strcmp(v[0],"\xed\xa0\x80"));
    Tcl_Free((char *)v);
    CHECK(Tcl_SplitList(i,"\\U0001f680",&n,&v)==0&&n==1&&!strcmp(v[0],"\xef\xbf\xbd"));
    Tcl_Free((char *)v);
    const char *items[]={"two words","","a}b","#tag","slash\\"};
    char *merged=Tcl_Merge(5,items); CHECK(merged);
    CHECK(Tcl_SplitList(i,merged,&n,&v)==0&&n==5);
    for(int k=0;k<n;k++) CHECK(!strcmp(v[k],items[k]));
    Tcl_Free((char *)v); Tcl_Free(merged);
    const char *parts[]={" \t a\\  ",""," {b c}  "};
    char *joined=Tcl_Concat(3,parts); CHECK(joined&&!strcmp(joined,"a\\  {b c}"));
    Tcl_Free(joined);
    Tcl_Obj *a=Tcl_NewStringObj(" a b ",-1),*bobj=Tcl_NewStringObj(" c ",-1);
    Tcl_Obj *objects[]={a,bobj};
    Tcl_Obj *result=Tcl_ConcatObj(2,objects); CHECK(result&&!strcmp(Tcl_GetString(result),"a b c"));
    TKGPU_ReleaseObject(a); TKGPU_ReleaseObject(bobj); TKGPU_ReleaseObject(result);
    int number=0; CHECK(Tcl_GetInt(i,"0x2a",&number)==0&&number==42);
    CHECK(Tcl_GetInt(i,"bad",&number)!=0&&number==42);
    const char *options[]={"start","stop","status",NULL};
    Tcl_Obj *opt=Tcl_NewStringObj("st",-1); CHECK(opt);
    int index=-1; CHECK(Tcl_GetIndexFromObj(i,opt,options,"option",0,&index)!=0&&index==-1);
    CHECK(!strcmp(Tcl_GetStringResult(i),"ambiguous option \"st\": must be start, stop, or status"));
    Tcl_SetStringObj(opt,"stop",-1);
    CHECK(Tcl_GetIndexFromObj(i,opt,options,"option",0,&index)==0&&index==1);
    Tcl_SetStringObj(opt,"sto",-1);
    CHECK(Tcl_GetIndexFromObj(i,opt,options,"option",TKGPU_EXACT,&index)!=0&&index==1);
    CHECK(Tcl_GetIndexFromObj(i,opt,options,"option",0,&index)==0&&index==1);
    const char *tailEmpty[]={"alpha","",NULL};
    Tcl_SetStringObj(opt,"bad",-1);
    CHECK(Tcl_GetIndexFromObj(i,opt,tailEmpty,"option",0,&index)!=0);
    CHECK(!strcmp(Tcl_GetStringResult(i),"bad option \"bad\": must be alpha or "));
    TKGPU_ReleaseObject(opt);
    int b=-1;
    CHECK(Tcl_GetBoolean(i,"TRUE",&b)==0&&b==1);
    CHECK(Tcl_GetBoolean(i,"of",&b)==0&&b==0);
    CHECK(Tcl_GetBoolean(i,"o",&b)!=0&&b==0);
    CHECK(Tcl_GetBoolean(i,"1",&b)==0&&b==1);
    Tcl_DeleteInterp(i); puts("compat list and boolean checks passed"); return 0;
}
