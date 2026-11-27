#include "../TKGPUFacadeTest.H"
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)

static jmp_buf panic_jump;
static char panic_seen[128];
static void custom_panic(const char *fmt,...) {
    va_list args;
    va_start(args,fmt);
    vsnprintf(panic_seen,sizeof panic_seen,fmt,args);
    va_end(args);
    longjmp(panic_jump,1);
}

static int exit_trace[8],exit_count;
static void dummy_exit(void *data) { (void)data; }
static void thread_exit_cb(void *data) {
    int id=(int)(intptr_t)data;
    if(exit_count<8) exit_trace[exit_count++]=id;
    if(id==2) Tcl_CreateThreadExitHandler(thread_exit_cb,(void *)(intptr_t)99);
}

static int cancel_cmd(void *data,Tcl_Interp *i,int objc,Tcl_Obj *const objv[]) {
    int unwind=*(int *)data;
    (void)objc; (void)objv;
    Tcl_Obj *msg=Tcl_NewStringObj("stopped",-1);
    if(Tcl_CancelEval(i,msg,NULL,unwind?TKGPU_CANCEL_UNWIND:0)!=0) return 1;
    return 0;
}

static char bg_seen[64];
static int bgerror_cmd(void *data,Tcl_Interp *i,int objc,Tcl_Obj *const objv[]) {
    (void)data; (void)i;
    if(objc>=2) snprintf(bg_seen,sizeof bg_seen,"%s",Tcl_GetString(objv[1]));
    return 0;
}
static int dummy_pkg_init(Tcl_Interp *i) { (void)i; return 0; }
static void dummy_main_loop(void) {}
static char *dummy_var_trace(void *d,Tcl_Interp *i,const char *p1,const char *p2,int f) {
    (void)d; (void)i; (void)p1; (void)p2; (void)f; return NULL;
}
static void dummy_cmd_trace(void *d,Tcl_Interp *i,const char *o,const char *n,int f) {
    (void)d; (void)i; (void)o; (void)n; (void)f;
}
static void dummy_exec_trace(void *d,Tcl_Interp *i,int l,char *c,Tcl_CmdProc *p,void *cd,int a,const char *av[]) {
    (void)d; (void)i; (void)l; (void)c; (void)p; (void)cd; (void)a; (void)av;
}
static int dummy_obj_trace(void *d,Tcl_Interp *i,int l,const char *c,Tcl_Command tok,int o,Tcl_Obj *const ov[]) {
    (void)d; (void)i; (void)l; (void)c; (void)tok; (void)o; (void)ov; return 0;
}
static int trace_deleted;
static void on_trace_delete(void *d) { ++*(int *)d; }
static int nre_post(void *data[],Tcl_Interp *i,int result) {
    int *acc=data[0]; *acc+=10;
    (void)i; return result;
}
static int nre_cmd(void *d,Tcl_Interp *i,int objc,Tcl_Obj *const objv[]) {
    (void)objc; (void)objv;
    Tcl_NRAddCallback(i,nre_post,d,NULL,NULL,NULL);
    Tcl_SetObjResult(i,Tcl_NewStringObj("nre-ok",-1));
    return 0;
}
static int dummy_math(void *d,Tcl_Interp *i,Tcl_Value *args,Tcl_Value *res) {
    (void)d; (void)i; (void)args; res->type=0; res->intValue=1; return 0;
}
#ifdef _WIN32
static unsigned __stdcall worker_thread(void *d) { *(int *)d=99; return 0; }
#else
static void worker_thread(void *d) { *(int *)d=99; }
#endif

int main(void) {
    TKGPU_TEST_PREPARE();

    /* Version */
    int maj=0,min=0,pat=-1,typ=-1;
    Tcl_GetVersion(&maj,&min,&pat,&typ);
    CHECK(maj==8&&min==6&&pat>=0&&typ==TKGPU_FINAL_RELEASE);
    Tcl_GetVersion(&maj,NULL,NULL,NULL);
    CHECK(maj==8);

    /* Panic proc */
    Tcl_SetPanicProc(custom_panic);
    if(setjmp(panic_jump)==0) {
        Tcl_Panic("boom %d",42);
        CHECK(0);
    }
    Tcl_SetPanicProc(NULL);
    CHECK(strstr(panic_seen,"boom 42")!=NULL);

    /* Exit handlers & SetExitProc */
    Tcl_CreateExitHandler(dummy_exit,(void *)1);
    Tcl_DeleteExitHandler(dummy_exit,(void *)1);
    Tcl_ExitProc *prev=Tcl_SetExitProc(dummy_exit);
    CHECK(Tcl_SetExitProc(prev)==dummy_exit);

    /* Thread exit handlers (LIFO + selective delete + reentrant register) */
    exit_count=0;
    Tcl_CreateThreadExitHandler(thread_exit_cb,(void *)(intptr_t)1);
    Tcl_CreateThreadExitHandler(thread_exit_cb,(void *)(intptr_t)2);
    Tcl_CreateThreadExitHandler(thread_exit_cb,(void *)(intptr_t)3);
    Tcl_DeleteThreadExitHandler(thread_exit_cb,(void *)(intptr_t)3);
    Tcl_FinalizeThread();
    CHECK(exit_count==3&&exit_trace[0]==2&&exit_trace[1]==99&&exit_trace[2]==1);

    /* PrintDouble */
    char dbuf[64];
    Tcl_PrintDouble(NULL,0.0,dbuf); CHECK(!strcmp(dbuf,"0.0"));
    Tcl_PrintDouble(NULL,-0.0,dbuf); CHECK(!strcmp(dbuf,"-0.0"));
    Tcl_PrintDouble(NULL,42.0,dbuf); CHECK(!strcmp(dbuf,"42.0"));
    Tcl_PrintDouble(NULL,0.1,dbuf); CHECK(!strcmp(dbuf,"0.1"));
    Tcl_PrintDouble(NULL,1e20,dbuf); CHECK(!strcmp(dbuf,"1e+20"));
    Tcl_PrintDouble(NULL,INFINITY,dbuf); CHECK(!strcmp(dbuf,"Inf"));
    Tcl_PrintDouble(NULL,-INFINITY,dbuf); CHECK(!strcmp(dbuf,"-Inf"));
    Tcl_PrintDouble(NULL,fabs(NAN),dbuf); CHECK(!strcmp(dbuf,"NaN"));
    Tcl_PrintDouble(NULL,-fabs(NAN),dbuf); CHECK(!strcmp(dbuf,"-NaN"));

    /* ObjPrintf & AppendPrintfToObj */
    Tcl_Obj *fmt=Tcl_ObjPrintf("v=%d s=%s",7,"ok");
    CHECK(fmt!=NULL);
    TKGPU_RetainObject(fmt);
    CHECK(!strcmp(Tcl_GetString(fmt),"v=7 s=ok"));
    Tcl_AppendPrintfToObj(fmt," %04x",0x2a);
    CHECK(!strcmp(Tcl_GetString(fmt),"v=7 s=ok 002a"));
    char big[320]; memset(big,'x',sizeof big-1); big[sizeof big-1]=0;
    Tcl_AppendPrintfToObj(fmt," %s",big);
    int flen=0;
    CHECK(Tcl_GetStringFromObj(fmt,&flen)&&flen==13+1+319);
    TKGPU_ReleaseObject(fmt);

    /* CancelEval & Canceled */
    Tcl_Interp *i=Tcl_CreateInterp();
    CHECK(i!=NULL);
    CHECK(Tcl_Canceled(i,0)==0);
    CHECK(Tcl_CancelEval(i,NULL,NULL,0)==0);
    CHECK(Tcl_Canceled(i,0)==0);
    int unwind=0;
    CHECK(Tcl_CreateObjCommand(i,"do_cancel",cancel_cmd,&unwind,NULL)!=NULL);
    CHECK(Tcl_Eval(i,"do_cancel; set after_cancel 1")==1);
    CHECK(!strcmp(Tcl_GetStringResult(i),"stopped"));
    CHECK(Tcl_GetVar(i,"after_cancel",0)==NULL);
    CHECK(Tcl_Canceled(i,0)==0);

    unwind=1;
    CHECK(Tcl_Eval(i,"do_cancel; set after_unwind 1")==1);
    CHECK(!strcmp(Tcl_GetStringResult(i),"stopped"));
    CHECK(Tcl_GetVar(i,"after_unwind",0)==NULL);
    const char *ec=Tcl_GetVar(i,"errorCode",TKGPU_GLOBAL_ONLY);
    CHECK(ec&&strstr(ec,"TCL CANCEL IUNWIND")!=NULL);

    /* GetReturnOptions & SetReturnOptions */
    Tcl_Obj *opts=Tcl_NewStringObj("-code error -level 0 -errorcode {MY ERR} -errorinfo {my trace} -errorline 9",-1);
    CHECK(Tcl_SetReturnOptions(i,opts)==1);
    CHECK(!strcmp(Tcl_GetVar(i,"errorCode",TKGPU_GLOBAL_ONLY),"MY ERR"));
    CHECK(!strcmp(Tcl_GetVar(i,"errorInfo",TKGPU_GLOBAL_ONLY),"my trace"));
    CHECK(Tcl_GetErrorLine(i)==9);

    Tcl_Obj *got=Tcl_GetReturnOptions(i,1);
    CHECK(got!=NULL);
    TKGPU_RetainObject(got);
    Tcl_Obj *kCode=Tcl_NewStringObj("-code",-1),*val=NULL;
    TKGPU_RetainObject(kCode);
    CHECK(Tcl_DictObjGet(i,got,kCode,&val)==0&&val&&!strcmp(Tcl_GetString(val),"1"));
    TKGPU_ReleaseObject(kCode);
    TKGPU_ReleaseObject(got);

    CHECK(Tcl_SetReturnOptions(i,Tcl_NewStringObj("-code ok",-1))==2);
    CHECK(Tcl_SetReturnOptions(i,Tcl_NewStringObj("-code break -level 0",-1))==3);
    CHECK(Tcl_SetReturnOptions(i,Tcl_NewStringObj("-code bogus",-1))==1);
    CHECK(strstr(Tcl_GetStringResult(i),"bad completion code")!=NULL);
    CHECK(Tcl_SetReturnOptions(i,Tcl_NewStringObj("-level -1",-1))==1);
    CHECK(strstr(Tcl_GetStringResult(i),"bad -level value")!=NULL);
    CHECK(Tcl_SetReturnOptions(i,Tcl_NewStringObj("odd",-1))==1);
    CHECK(strstr(Tcl_GetStringResult(i),"expected dict")!=NULL);

    /* Tcl_Format & Tcl_AppendFormatToObj */
    Tcl_Obj *fargs[3]={Tcl_NewStringObj("hi",-1),Tcl_NewIntObj(42),Tcl_NewIntObj(65)};
    for(int k=0;k<3;k++) TKGPU_RetainObject(fargs[k]);
    Tcl_Obj *formatted=Tcl_Format(i,"%s=%04d(%c) %%",3,fargs);
    CHECK(formatted!=NULL);
    TKGPU_RetainObject(formatted);
    CHECK(!strcmp(Tcl_GetString(formatted),"hi=0042(A) %"));
    CHECK(Tcl_AppendFormatToObj(i,formatted," %2$x:%1$s",2,fargs)==0);
    CHECK(!strcmp(Tcl_GetString(formatted),"hi=0042(A) % 2a:hi"));
    TKGPU_ReleaseObject(formatted);
    CHECK(Tcl_Format(i,"%s %s",1,fargs)==NULL);
    CHECK(Tcl_Format(i,"%q",1,fargs)==NULL);
    for(int k=0;k<3;k++) TKGPU_ReleaseObject(fargs[k]);

    /* BackgroundError / BackgroundException */
    bg_seen[0]=0;
    CHECK(Tcl_CreateObjCommand(i,"bgerror",bgerror_cmd,NULL,NULL)!=NULL);
    Tcl_SetObjResult(i,Tcl_NewStringObj("async oops",-1));
    Tcl_BackgroundError(i);
    CHECK(Tcl_DoOneEvent(TKGPU_DONT_WAIT)!=0);
    CHECK(!strcmp(bg_seen,"async oops"));

    /* LogCommandInfo & AllowExceptions */
    Tcl_AllowExceptions(i);
    CHECK(Tcl_Eval(i,"error top")==1);
    const char *script="line1\nline2\nboom";
    Tcl_LogCommandInfo(i,script,script+12,4);
    CHECK(Tcl_GetErrorLine(i)==3);
    const char *einfo=Tcl_GetVar(i,"errorInfo",TKGPU_GLOBAL_ONLY);
    CHECK(einfo&&strstr(einfo,"while executing")&&strstr(einfo,"\"boom\""));

    /* PutEnv */
    CHECK(Tcl_PutEnv("TKGPU_STUB_TEST_ENV=hello")==0);
    CHECK(getenv("TKGPU_STUB_TEST_ENV")&&!strcmp(getenv("TKGPU_STUB_TEST_ENV"),"hello"));
    CHECK(Tcl_PutEnv("NOEQUALS")!=0);

    /* RegisterConfig */
    static const Tcl_Config cfg[]={{"version","1.0"},{"mode","fast"},{NULL,NULL}};
    Tcl_RegisterConfig(i,"demo",cfg,"utf-8");
    CHECK(Tcl_Eval(i,"::demo::pkgconfig list")==0);
    CHECK(!strcmp(Tcl_GetStringResult(i),"version mode"));
    CHECK(Tcl_Eval(i,"::demo::pkgconfig get mode")==0);
    CHECK(!strcmp(Tcl_GetStringResult(i),"fast"));
    CHECK(Tcl_Eval(i,"::demo::pkgconfig get missing")==1);

    /* Misc hooks & memory diagnostics */
    Tcl_StaticPackage(i,"DemoPkg",dummy_pkg_init,NULL);
    Tcl_SourceRCFile(i);
    Tcl_Mutex *am=Tcl_GetAllocMutex();
    CHECK(am!=NULL);
    Tcl_MutexLock(am); Tcl_MutexUnlock(am);
    Tcl_InitMemory(i);
#ifdef TKGPU_TEST_REFERENCE_STUBS
    /* Non-debug Tcl headers deliberately expand this API to a void macro. */
    Tcl_DumpActiveMemory(NULL);
#else
    CHECK(Tcl_DumpActiveMemory(NULL)==0);
#endif
    Tcl_ValidateAllMemory(__FILE__,__LINE__);
    Tcl_SetMainLoop(dummy_main_loop);
    Tcl_SetMainLoop(NULL);
    Tcl_ServiceModeHook(1);

    /* Namespaces & Ensembles */
    int nsDeleted=0;
    Tcl_Namespace *child=Tcl_CreateNamespace(i,"::alpha::beta",&nsDeleted,dummy_exit);
    CHECK(child&&!strcmp(child->name,"beta")&&!strcmp(child->fullName,"::alpha::beta"));
    CHECK(Tcl_FindNamespace(i,"::alpha::beta",NULL,0)==child);
    CHECK(Tcl_FindNamespace(i,"::alpha",NULL,0)!=NULL);
    CHECK(Tcl_CreateNamespace(i,"::alpha::beta",NULL,NULL)==NULL);
    Tcl_Obj *unk=Tcl_NewStringObj("my_unknown",-1);
    TKGPU_RetainObject(unk);
    CHECK(Tcl_SetNamespaceUnknownHandler(i,child,unk)==0);
    CHECK(Tcl_GetNamespaceUnknownHandler(i,child)==unk);
    CHECK(Tcl_SetNamespaceUnknownHandler(i,child,NULL)==0);
    CHECK(Tcl_GetNamespaceUnknownHandler(i,child)==NULL);
    TKGPU_ReleaseObject(unk);
    CHECK(Tcl_Import(i,child,"::alpha::*",1)==0);
    CHECK(Tcl_ForgetImport(i,child,"::alpha::*")==0);

    Tcl_Command ens=Tcl_CreateEnsemble(i,"myens",child,1);
    CHECK(ens!=NULL&&Tcl_IsEnsemble(ens));
    Tcl_Obj *ensName=Tcl_NewStringObj("myens",-1);
    TKGPU_RetainObject(ensName);
    CHECK(Tcl_FindEnsemble(i,ensName,TKGPU_LEAVE_ERR_MSG)==ens);
    TKGPU_ReleaseObject(ensName);
    int eflags=0;
    CHECK(Tcl_GetEnsembleFlags(i,ens,&eflags)==0&&eflags==1);
    CHECK(Tcl_SetEnsembleFlags(i,ens,0)==0);
    CHECK(Tcl_GetEnsembleFlags(i,ens,&eflags)==0&&eflags==0);
    Tcl_Namespace *ensNs=NULL;
    CHECK(Tcl_GetEnsembleNamespace(i,ens,&ensNs)==0&&ensNs==child);
    Tcl_Obj *map=Tcl_NewStringObj("ping {set ens_ok}",-1);
    TKGPU_RetainObject(map);
    CHECK(Tcl_SetEnsembleMappingDict(i,ens,map)==0);
    Tcl_Obj *gotMap=NULL;
    CHECK(Tcl_GetEnsembleMappingDict(i,ens,&gotMap)==0&&gotMap==map);
    CHECK(Tcl_Eval(i,"myens ping 77")==0&&!strcmp(Tcl_GetStringResult(i),"77"));
    TKGPU_ReleaseObject(map);
    Tcl_Obj *subs=Tcl_NewStringObj("ping pong",-1);
    TKGPU_RetainObject(subs);
    CHECK(Tcl_SetEnsembleSubcommandList(i,ens,subs)==0);
    Tcl_Obj *gotSubs=NULL;
    CHECK(Tcl_GetEnsembleSubcommandList(i,ens,&gotSubs)==0&&gotSubs==subs);
    CHECK(Tcl_SetEnsembleParameterList(i,ens,subs)==0);
    CHECK(Tcl_GetEnsembleParameterList(i,ens,&gotSubs)==0&&gotSubs==subs);
    CHECK(Tcl_SetEnsembleUnknownHandler(i,ens,subs)==0);
    CHECK(Tcl_GetEnsembleUnknownHandler(i,ens,&gotSubs)==0&&gotSubs==subs);
    TKGPU_ReleaseObject(subs);
    Tcl_DeleteNamespace(child);
    CHECK(Tcl_FindNamespace(i,"::alpha::beta",NULL,0)==NULL);

    /* Traces, LinkVar, UpVar */
    CHECK(Tcl_TraceVar(i,"traced_v",TKGPU_TRACE_WRITES,dummy_var_trace,(void *)11)==0);
    CHECK(Tcl_TraceVar2(i,"traced_v",NULL,TKGPU_TRACE_READS,dummy_var_trace,(void *)22)==0);
    void *ti1=Tcl_VarTraceInfo(i,"traced_v",0,dummy_var_trace,NULL);
    void *ti2=Tcl_VarTraceInfo2(i,"traced_v",NULL,0,dummy_var_trace,ti1);
    CHECK(ti1==(void *)22&&ti2==(void *)11);
    Tcl_UntraceVar(i,"traced_v",TKGPU_TRACE_WRITES,dummy_var_trace,(void *)11);
    Tcl_UntraceVar2(i,"traced_v",NULL,TKGPU_TRACE_READS,dummy_var_trace,(void *)22);
    CHECK(Tcl_VarTraceInfo(i,"traced_v",0,dummy_var_trace,NULL)==NULL);

    CHECK(Tcl_TraceCommand(i,"bgerror",TKGPU_TRACE_DELETE,dummy_cmd_trace,(void *)33)==0);
    CHECK(Tcl_CommandTraceInfo(i,"bgerror",0,dummy_cmd_trace,NULL)==(void *)33);
    Tcl_UntraceCommand(i,"bgerror",TKGPU_TRACE_DELETE,dummy_cmd_trace,(void *)33);
    CHECK(Tcl_CommandTraceInfo(i,"bgerror",0,dummy_cmd_trace,NULL)==NULL);

    int linkedVal=123;
    CHECK(Tcl_LinkVar(i,"c_int",(char *)&linkedVal,TKGPU_LINK_INT)==0);
    CHECK(!strcmp(Tcl_GetVar(i,"c_int",TKGPU_GLOBAL_ONLY),"123"));
    linkedVal=456;
    Tcl_UpdateLinkedVar(i,"c_int");
    CHECK(!strcmp(Tcl_GetVar(i,"c_int",TKGPU_GLOBAL_ONLY),"456"));
    Tcl_UnlinkVar(i,"c_int");

    CHECK(Tcl_SetVar(i,"src_v","999",TKGPU_GLOBAL_ONLY)!=NULL);
    CHECK(Tcl_UpVar(i,"#0","src_v","dst_v",0)==0);
    CHECK(!strcmp(Tcl_GetVar(i,"dst_v",0),"999"));

    /* Safe/Slave Interps, Hide/Expose, Aliases, Execution Traces */
    CHECK(!Tcl_IsSafe(i));
    Tcl_Interp *slave=Tcl_CreateSlave(i,"child1",1);
    CHECK(slave!=NULL&&Tcl_IsSafe(slave)&&Tcl_GetSlave(i,"child1")==slave&&Tcl_GetMaster(slave)==i);
    CHECK(Tcl_GetInterpPath(i,slave)==0&&!strcmp(Tcl_GetStringResult(i),"child1"));
    Tcl_Interp *grand=Tcl_CreateSlave(slave,"grand",0);
    CHECK(grand!=NULL&&Tcl_IsSafe(grand));
    CHECK(Tcl_GetInterpPath(i,grand)==0&&!strcmp(Tcl_GetStringResult(i),"child1 grand"));

    const char *aliasArgs[]={"from_slave"};
    CHECK(Tcl_CreateAlias(slave,"set_parent",i,"set",1,aliasArgs)==0);
    CHECK(Tcl_Eval(slave,"set_parent 777")==0&&!strcmp(Tcl_GetStringResult(slave),"777"));
    CHECK(!strcmp(Tcl_GetVar(i,"from_slave",TKGPU_GLOBAL_ONLY),"777"));
    Tcl_Interp *atarget=NULL; const char *acmd=NULL; int aargc=0; const char **aargv=NULL;
    CHECK(Tcl_GetAlias(slave,"set_parent",&atarget,&acmd,&aargc,&aargv)==0);
    CHECK(atarget==i&&!strcmp(acmd,"set")&&aargc==1&&!strcmp(aargv[0],"from_slave"));
    Tcl_Obj **aobjv=NULL;
    CHECK(Tcl_GetAliasObj(slave,"set_parent",&atarget,&acmd,&aargc,&aobjv)==0&&aargc==1);

    CHECK(Tcl_HideCommand(slave,"set_parent","hidden_set")==0);
    CHECK(Tcl_Eval(slave,"set_parent 888")==1);
    CHECK(Tcl_ExposeCommand(slave,"hidden_set","set_parent")==0);
    CHECK(Tcl_Eval(slave,"set_parent 888")==0&&!strcmp(Tcl_GetVar(i,"from_slave",TKGPU_GLOBAL_ONLY),"888"));

    trace_deleted=0;
    Tcl_Trace t1=Tcl_CreateTrace(i,1,dummy_exec_trace,NULL);
    Tcl_Trace t2=Tcl_CreateObjTrace(i,1,0,dummy_obj_trace,&trace_deleted,on_trace_delete);
    CHECK(t1&&t2);
    Tcl_DeleteTrace(i,t1);
    Tcl_DeleteTrace(i,t2);
    CHECK(trace_deleted==1);

    /* NRE, SubstObj, MathFunc, ParseArgsObjv */
    int nreAcc=0;
    CHECK(Tcl_NRCreateCommand(i,"nre_test",NULL,nre_cmd,&nreAcc,NULL)!=NULL);
    CHECK(Tcl_Eval(i,"nre_test")==0&&!strcmp(Tcl_GetStringResult(i),"nre-ok")&&nreAcc==10);

    CHECK(Tcl_SetVar(i,"sv","world",TKGPU_GLOBAL_ONLY)!=NULL);
    Tcl_Obj *subIn=Tcl_NewStringObj("hi $sv [expr {2+3}] \\n",-1);
    TKGPU_RetainObject(subIn);
    Tcl_Obj *subOut=Tcl_SubstObj(i,subIn,7);
    CHECK(subOut!=NULL);
    TKGPU_RetainObject(subOut);
    CHECK(!strcmp(Tcl_GetString(subOut),"hi world 5 \n"));
    TKGPU_ReleaseObject(subOut);
    CHECK(Tcl_NRSubstObj(i,subIn,2)==0&&!strcmp(Tcl_GetStringResult(i),"hi world [expr {2+3}] \\n"));
    TKGPU_ReleaseObject(subIn);

    Tcl_Obj *exprIn=Tcl_NewStringObj("6*7",-1),*exprOut=Tcl_NewObj();
    TKGPU_RetainObject(exprIn); TKGPU_RetainObject(exprOut);
    CHECK(Tcl_NRExprObj(i,exprIn,exprOut)==0&&!strcmp(Tcl_GetString(exprOut),"42"));
    TKGPU_ReleaseObject(exprIn); TKGPU_ReleaseObject(exprOut);

    Tcl_ValueType mtypes[1]={0};
    Tcl_CreateMathFunc(i,"myfn",1,mtypes,dummy_math,(void *)7);
    int margs=0; Tcl_ValueType *mtypesOut=NULL; Tcl_MathProc *mproc=NULL; void *mdata=NULL;
    CHECK(Tcl_GetMathFuncInfo(i,"myfn",&margs,&mtypesOut,&mproc,&mdata)==0);
    CHECK(margs==1&&mproc==dummy_math&&mdata==(void *)7&&mtypesOut!=NULL);
    Tcl_Free((char *)mtypesOut);
    Tcl_Obj *mlist=Tcl_ListMathFuncs(i,"my*");
    CHECK(mlist!=NULL); TKGPU_RetainObject(mlist);
    CHECK(strstr(Tcl_GetString(mlist),"myfn")!=NULL);
    TKGPU_ReleaseObject(mlist);

    int optFlag=0,optNum=0; const char *optStr=NULL;
    Tcl_ArgvInfo argTable[]={
        {15,"-flag",(void *)1,&optFlag,NULL,NULL},
        {16,"-num",NULL,&optNum,NULL,NULL},
        {17,"-name",NULL,&optStr,NULL,NULL},
        {0,NULL,NULL,NULL,NULL,NULL}
    };
    Tcl_Obj *pargv[7]={
        Tcl_NewStringObj("cmd",-1),Tcl_NewStringObj("-flag",-1),
        Tcl_NewStringObj("-num",-1),Tcl_NewStringObj("42",-1),
        Tcl_NewStringObj("-name",-1),Tcl_NewStringObj("bob",-1),
        Tcl_NewStringObj("extra",-1)
    };
    for(int k=0;k<7;k++) TKGPU_RetainObject(pargv[k]);
    int popc=7; Tcl_Obj **rem=NULL;
    CHECK(Tcl_ParseArgsObjv(i,argTable,&popc,pargv,&rem)==0);
    CHECK(optFlag==1&&optNum==42&&optStr&&!strcmp(optStr,"bob")&&popc==2&&!strcmp(Tcl_GetString(rem[1]),"extra"));
    Tcl_Free((char *)rem);
    for(int k=0;k<7;k++) TKGPU_ReleaseObject(pargv[k]);

    /* Threads & Encodings */
    int threadOut=0; Tcl_ThreadId tid=NULL;
    CHECK(Tcl_CreateThread(&tid,worker_thread,&threadOut,0,1)==0);
    int tres=-1;
    CHECK(Tcl_JoinThread(tid,&tres)==0&&threadOut==99);
    Tcl_ReapDetachedProcs();

    Tcl_Encoding latin=Tcl_GetEncoding(i,"iso8859-1");
    CHECK(latin!=NULL&&!strcmp(Tcl_GetEncodingName(latin),"iso8859-1"));
    Tcl_DString eds;
    const char *utf=Tcl_ExternalToUtfDString(latin,"\xe9",1,&eds);
    CHECK(!strcmp(utf,"\xc3\xa9"));
    Tcl_DStringFree(&eds);
    const char *ext=Tcl_UtfToExternalDString(latin,"\xc3\xa9",2,&eds);
    CHECK((unsigned char)ext[0]==0xe9);
    Tcl_DStringFree(&eds);
    Tcl_FreeEncoding(latin);
    Tcl_GetEncodingNames(i);
    CHECK(strstr(Tcl_GetStringResult(i),"utf-8")!=NULL);
    Tcl_SetDefaultEncodingDir("/tmp/enc");
    CHECK(Tcl_GetDefaultEncodingDir()&&!strcmp(Tcl_GetDefaultEncodingDir(),"/tmp/enc"));
    Tcl_Obj *sp=Tcl_NewStringObj("/tmp/enc",-1);
    TKGPU_RetainObject(sp);
    CHECK(Tcl_SetEncodingSearchPath(sp)==1&&Tcl_GetEncodingSearchPath()!=NULL);
    TKGPU_ReleaseObject(sp);

    /* Invalid syntax must fail compilation, rather than report no match. */
    CHECK(Tcl_RegExpMatch(i,"text","[")==-1);
    CHECK(Tcl_RegExpCompile(i,"(abc")==NULL);
    CHECK(Tcl_RegExpMatch(i,"text","[z-a]")==-1);
    CHECK(strstr(Tcl_GetStringResult(i),"compile regular expression")!=NULL);
    /* RegExp */
    CHECK(Tcl_RegExpMatch(i,"hello 123 world","[a-z]+ [0-9]+")==1);
    CHECK(Tcl_RegExpMatch(i,"hello","^world$")==0);
    Tcl_Obj *rePat=Tcl_NewStringObj("([a-z]+)=([0-9]+)",-1);
    Tcl_Obj *reTxt=Tcl_NewStringObj("key=42!",-1);
    TKGPU_RetainObject(rePat); TKGPU_RetainObject(reTxt);
    Tcl_RegExp re=Tcl_GetRegExpFromObj(i,rePat,0);
    CHECK(re!=NULL&&Tcl_RegExpExecObj(i,re,reTxt,0,3,0)==1);
    Tcl_RegExpInfo rinfo; Tcl_RegExpGetInfo(re,&rinfo);
    CHECK(rinfo.nsubs==2&&rinfo.matches[0].start==0&&rinfo.matches[0].end==6);
    CHECK(rinfo.matches[1].start==0&&rinfo.matches[1].end==3);
    CHECK(rinfo.matches[2].start==4&&rinfo.matches[2].end==6);
    const char *rs=NULL,*rend=NULL;
    Tcl_RegExpRange(re,2,&rs,&rend);
    CHECK(rs&&rend&&(rend-rs)==2&&!memcmp(rs,"42",2));
    CHECK(Tcl_RegExpMatchObj(i,reTxt,rePat)==1);
    TKGPU_ReleaseObject(rePat); TKGPU_ReleaseObject(reTxt);

    /* Independently generated dynamic-Huffman stream, not our own encoder. */
    static const unsigned char external_zlib[]={
        0x78,0x9c,0xed,0xc6,0xb1,0x09,0x00,0x20,0x0c,0x04,0xc0,0x55,0x32,0x9c,0x42,0x8a,0x87,0x80,0x8d,0xeb,0x3b,0x86,0xcd,0x5d,0x75,0xbd,0x93,0xa9,0x3b,0x27,0xab,0xda,0xdd,0xdd,0xdd,0xdd,0xdd,0xdd,0xdd,0xdd,0xdd,0xdd,0xdd,0xdd,0xdd,0xdd,0xdd,0xdd,0xdd,0xdd,0xdd,0xdd,0xdd,0xdd,0xfd,0xd3,0x1f,0xc3,0x8c,0x85,0x60
    };
    Tcl_Obj *external=Tcl_NewByteArrayObj(external_zlib,sizeof external_zlib);
    TKGPU_RetainObject(external);
    CHECK(Tcl_ZlibInflate(i,2,external,0,NULL)==0);
    int external_len=0;
    const unsigned char *external_bytes=Tcl_GetByteArrayFromObj(Tcl_GetObjResult(i),&external_len);
    CHECK(external_len==12000);
    for(int chunk=0;chunk<1000;chunk++) CHECK(!memcmp(external_bytes+chunk*12,"hello world ",12));
    TKGPU_ReleaseObject(external);
    /* Zlib deflate/inflate & stream */
    Tcl_Obj *plain=Tcl_NewByteArrayObj((const unsigned char *)"hello zlib world!",17);
    TKGPU_RetainObject(plain);
    CHECK(Tcl_ZlibDeflate(i,2,plain,0,NULL)==0);
    Tcl_Obj *comp=Tcl_GetObjResult(i);
    TKGPU_RetainObject(comp);
    CHECK(Tcl_ZlibInflate(i,2,comp,0,NULL)==0);
    int outLen=0;
    unsigned char *outBytes=Tcl_GetByteArrayFromObj(Tcl_GetObjResult(i),&outLen);
    CHECK(outLen==17&&!memcmp(outBytes,"hello zlib world!",17));
    TKGPU_ReleaseObject(comp);

    Tcl_ZlibStream zs=NULL;
    CHECK(Tcl_ZlibStreamInit(i,16,4,0,NULL,&zs)==0&&zs!=NULL);
    CHECK(Tcl_ZlibStreamGetCommandName(zs)!=NULL&&!Tcl_ZlibStreamEof(zs));
    CHECK(Tcl_ZlibStreamPut(zs,plain,4)==0&&Tcl_ZlibStreamEof(zs));
    Tcl_Obj *zout=Tcl_NewObj(); TKGPU_RetainObject(zout);
    CHECK(Tcl_ZlibStreamGet(zs,zout,-1)==0);
    CHECK(Tcl_ZlibInflate(i,4,zout,0,NULL)==0);
    outBytes=Tcl_GetByteArrayFromObj(Tcl_GetObjResult(i),&outLen);
    CHECK(outLen==17&&!memcmp(outBytes,"hello zlib world!",17));
    TKGPU_ReleaseObject(zout);
    CHECK(Tcl_ZlibStreamReset(zs)==0&&!Tcl_ZlibStreamEof(zs));
    CHECK(Tcl_ZlibStreamClose(zs)==0);
    TKGPU_ReleaseObject(plain);

    Tcl_DeleteInterp(i);
    return 0;
}
