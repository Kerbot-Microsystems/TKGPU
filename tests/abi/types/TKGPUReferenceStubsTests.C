/* Stock reference stub archive, independent extension-style consumer. */
#define USE_TCL_STUBS
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <utime.h>
#include <errno.h>
#include <signal.h>
#include <tcl.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#undef Tcl_CreateInterp
#undef Tcl_DeleteInterp
extern Tcl_Interp *Tcl_CreateInterp(void);
extern void Tcl_DeleteInterp(Tcl_Interp *);
extern int TKGPU_EnableReferenceStubs(Tcl_Interp *,const char *const *,size_t);
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
static int string_callback(ClientData data,Tcl_Interp *i,int n,const char *a[]) {
    if(n!=2||a[n]!=NULL) return TCL_ERROR;
    Tcl_AppendResult(i,(const char *)data,a[1],(char *)NULL);
    return TCL_OK;
}
static void info_delete_callback(ClientData data) { ++*(int *)data; }
static int inspect_obj_callback(ClientData data,Tcl_Interp *i,int n,Tcl_Obj *const a[]) {
    if(n!=2) return TCL_ERROR;
    Tcl_AppendResult(i,(const char *)data,Tcl_GetString(a[1]),(char *)NULL); return TCL_OK;
}
static void on_delete(ClientData data,Tcl_Interp *i) { (void)i; ++*(int *)data; }
static void on_deleted_state(ClientData data,Tcl_Interp *i) { *(int *)data=Tcl_InterpDeleted(i)?1:-1; }
static void on_finalizer_probe(ClientData data,Tcl_Interp *i) {
    Tcl_Obj *v[3]={Tcl_NewStringObj("info",-1),Tcl_NewStringObj("exists",-1),Tcl_NewStringObj("missing",-1)};
    for(int k=0;k<3;k++) Tcl_IncrRefCount(v[k]);
    *(int *)data=(Tcl_EvalObjv(i,3,v,0)==TCL_OK&&!strcmp(Tcl_GetStringResult(i),"0"))?1:-1;
    for(int k=0;k<3;k++) Tcl_DecrRefCount(v[k]);
}
static int delete_during_eval(ClientData data,Tcl_Interp *i,int objc,Tcl_Obj *const objv[]) {
    (void)data;(void)objc;(void)objv;
    Tcl_DeleteInterp(i); return Tcl_InterpDeleted(i)?TCL_OK:TCL_ERROR;
}
static void append_result_va(Tcl_Interp *i,...) {
    va_list args; va_start(args,i); Tcl_AppendResultVA(i,args); va_end(args);
}
static void append_object_va(Tcl_Obj *o,...) {
    va_list args; va_start(args,o); Tcl_AppendStringsToObjVA(o,args); va_end(args);
}
static void set_error_va(Tcl_Interp *i,...) {
    va_list args; va_start(args,i);
    Tcl_SetErrorCodeVA(i,args); va_end(args);
}
static int idleTrace[8],idleCount;
static void native_idle(void *context) {
    int v=(int)(uintptr_t)context;
    if(idleCount<8) idleTrace[idleCount++]=v;
    if(v==1) Tcl_DoWhenIdle(native_idle,(void *)(uintptr_t)9);
    if(v==3) Tcl_CancelIdleCall(native_idle,(void *)(uintptr_t)4);
}
static int serviceCount,serviceFlags,serviceMode;
static int service_event(Tcl_Event *event,int flags) {
    (void)event; ++serviceCount; serviceFlags=flags; serviceMode=Tcl_GetServiceMode(); return 1;
}
static void queue_service_event(void) {
    Tcl_Event *e=(Tcl_Event *)Tcl_Alloc(sizeof *e); e->proc=service_event; e->nextPtr=NULL; Tcl_QueueEvent(e,TCL_QUEUE_TAIL);
}
static int asyncCalls;
static int async_callback(ClientData data,Tcl_Interp *i,int code) {
    ++asyncCalls;
    if(i) Tcl_SetObjResult(i,Tcl_NewStringObj((const char *)data,-1));
    return code;
}
static Tcl_Time virtualTime;
static int virtualFired;
static void virtual_get(Tcl_Time *t,ClientData data) { *t=*(Tcl_Time *)data; }
static void virtual_scale(Tcl_Time *t,ClientData data) { (void)t;(void)data; }
static void virtual_timer(ClientData data) { ++*(int *)data; }
static int preserveCalls; static char preserveToken,preserveNested;
static void preserveNestedCallback(char *p) { if(p==&preserveNested) ++preserveCalls; }
static void preserveCallback(char *p) {
    if(p==&preserveToken) ++preserveCalls;
    Tcl_Preserve(&preserveNested); Tcl_EventuallyFree(&preserveNested,preserveNestedCallback); Tcl_Release(&preserveNested);
}
int main(void) {
    Tcl_Interp *i=Tcl_CreateInterp(); CHECK(i);
    CHECK(!Tcl_InitStubs(i,"8.6",0));
    const char *bad[]={"Tcl_Eval","Tcl_NotARealStubSlot"};
    CHECK(TKGPU_EnableReferenceStubs(i,bad,2)!=TCL_OK);
    CHECK(!Tcl_InitStubs(i,"8.6",0));
    const char *required[]={"Tcl_NewStringObj","TclFreeObj","Tcl_GetString",
        "Tcl_NewIntObj","Tcl_GetIntFromObj","Tcl_NewListObj","Tcl_ListObjIndex",
        "Tcl_ListObjLength","Tcl_SetObjResult","Tcl_GetObjResult","Tcl_Eval",
        "Tcl_ResetResult","Tcl_UtfAtIndex","Tcl_UniCharAtIndex","Tcl_UtfNext","Tcl_UtfPrev","Tcl_UtfToUniChar","Tcl_UtfNcmp","Tcl_UtfBackslash","Tcl_Backslash","Tcl_AppendResultVA","Tcl_AppendStringsToObjVA","Tcl_ScanElement","Tcl_ScanCountedElement","Tcl_ConvertElement","Tcl_ConvertCountedElement","Tcl_StringMatch","Tcl_AppendLimitedToObj","Tcl_RecordAndEvalObj","Tcl_UniCharIsAlnum","Tcl_UniCharIsAlpha","Tcl_UniCharIsControl","Tcl_UniCharIsDigit","Tcl_UniCharIsGraph","Tcl_UniCharIsLower","Tcl_UniCharIsPrint","Tcl_UniCharIsPunct","Tcl_UniCharIsSpace","Tcl_UniCharIsUpper","Tcl_UniCharIsWordChar","Tcl_UniCharToLower","Tcl_UniCharToUpper","Tcl_UniCharToTitle","Tcl_UniCharNcasecmp","Tcl_GetErrno","Tcl_SetErrno","Tcl_UtfToLower","Tcl_UtfToUpper","Tcl_UtfToTitle","Tcl_UtfNcasecmp","Tcl_UniCharCaseMatch","Tcl_StringCaseMatch","Tcl_SetErrorCodeVA","Tcl_AppendObjToErrorInfo","Tcl_PanicVA","Tcl_AllocStatBuf","Tcl_GetFSDeviceFromStat","Tcl_GetFSInodeFromStat","Tcl_GetModeFromStat","Tcl_GetLinkCountFromStat","Tcl_GetUserIdFromStat","Tcl_GetGroupIdFromStat","Tcl_GetDeviceTypeFromStat","Tcl_GetAccessTimeFromStat","Tcl_GetModificationTimeFromStat","Tcl_GetChangeTimeFromStat","Tcl_GetSizeFromStat","Tcl_GetBlocksFromStat","Tcl_GetBlockSizeFromStat","Tcl_ZlibCRC32","Tcl_ZlibAdler32","Tcl_ErrnoId","Tcl_SignalId","Tcl_SignalMsg","Tcl_EvalObj","Tcl_GetNameOfExecutable","Tcl_FindExecutable","Tcl_Init","Tcl_JoinPath","Tcl_GetPathType","Tcl_SplitPath","Tcl_InitHashTable","Tcl_FindHashEntry","Tcl_CreateHashEntry","Tcl_DeleteHashEntry","Tcl_DeleteHashTable","Tcl_NextHashEntry","Tcl_FirstHashEntry","Tcl_DbCkalloc","Tcl_DbCkfree","Tcl_DbCkrealloc","Tcl_AttemptDbCkalloc","Tcl_AttemptDbCkrealloc","Tcl_DbIncrRefCount","Tcl_DbDecrRefCount","Tcl_DbIsShared","Tcl_DbNewBooleanObj","Tcl_DbNewByteArrayObj","Tcl_DbNewDoubleObj","Tcl_DbNewListObj","Tcl_DbNewLongObj","Tcl_DbNewObj","Tcl_DbNewStringObj","Tcl_DbNewWideIntObj","Tcl_HashStats","Tcl_InitCustomHashTable","Tcl_InitObjHashTable","Tcl_ChannelName","Tcl_ChannelVersion","Tcl_ChannelBlockModeProc","Tcl_ChannelCloseProc","Tcl_ChannelClose2Proc","Tcl_ChannelInputProc","Tcl_ChannelOutputProc","Tcl_ChannelSeekProc","Tcl_ChannelSetOptionProc","Tcl_ChannelGetOptionProc","Tcl_ChannelWatchProc","Tcl_ChannelGetHandleProc","Tcl_ChannelFlushProc","Tcl_ChannelHandlerProc","Tcl_ChannelWideSeekProc","Tcl_ChannelThreadActionProc","Tcl_ChannelTruncateProc","Tcl_DoWhenIdle","Tcl_CancelIdleCall","Tcl_SaveResult","Tcl_DiscardResult","Tcl_RestoreResult","Tcl_FSGetPathType","Tcl_FSSplitPath","Tcl_FSPathSeparator","Tcl_FSFileSystemInfo","Tcl_FSListVolumes","Tcl_FSJoinPath","Tcl_FSJoinToPath","Tcl_ErrnoMsg","Tcl_PosixError","Tcl_GetCwd","Tcl_GetHostName","Tcl_TranslateFileName","Tcl_Access","Tcl_Stat","Tcl_FSAccess","Tcl_FSStat","Tcl_FSLstat","Tcl_Chdir","Tcl_FSChdir","Tcl_FSGetCwd","Tcl_FSCreateDirectory","Tcl_FSDeleteFile","Tcl_FSRenameFile","Tcl_FSUtime","Tcl_ParseBraces","Tcl_FreeParse","Tcl_SetStartupScript","Tcl_GetStartupScript","Tcl_Preserve","Tcl_Release","Tcl_EventuallyFree","Tcl_InterpDeleted","Tcl_GetServiceMode","Tcl_SetServiceMode","Tcl_ServiceEvent","Tcl_ServiceAll","Tcl_FSFileAttrStrings","Tcl_FSFileAttrsGet","Tcl_FSFileAttrsSet","Tcl_FindCommand","Tcl_GetCommandInfo","Tcl_GetCommandInfoFromToken","Tcl_SetCommandInfo","Tcl_SetCommandInfoFromToken","Tcl_FSLink","Tcl_NewDictObj","Tcl_DbNewDictObj","Tcl_DictObjPut","Tcl_DictObjGet","Tcl_DictObjRemove","Tcl_DictObjSize","Tcl_DictObjFirst","Tcl_DictObjNext","Tcl_DictObjDone","Tcl_DictObjPutKeyList","Tcl_DictObjRemoveKeyList","Tcl_AsyncCreate","Tcl_AsyncDelete","Tcl_AsyncMark","Tcl_AsyncReady","Tcl_AsyncInvoke","Tcl_SetChannelErrorInterp","Tcl_GetChannelErrorInterp","Tcl_SaveInterpState","Tcl_RestoreInterpState","Tcl_DiscardInterpState","Tcl_SetTimeProc","Tcl_QueryTimeProc","Tcl_GetGlobalNamespace","Tcl_GetCurrentNamespace","Tcl_FindNamespace","Tcl_Export","Tcl_AppendExportList","Tcl_PkgProvideEx","Tcl_PkgProvide","Tcl_PkgPresentEx","Tcl_PkgPresent","Tcl_PkgRequire","Tcl_PkgRequireProc","Tcl_UtfFindFirst","Tcl_UtfFindLast","Tcl_NumUtfChars","Tcl_UtfToUniCharDString","Tcl_AppendElement","Tcl_UtfCharComplete","Tcl_GetUnicode","Tcl_AppendStringsToObj","Tcl_GetIndexFromObj","Tcl_GetIndexFromObjStruct","Tcl_Merge","Tcl_Concat","Tcl_ConcatObj","Tcl_GetInt","Tcl_UniCharToUtf","Tcl_UniCharLen","Tcl_UniCharNcmp","Tcl_UniCharToUtfDString","Tcl_GetTime","Tcl_Sleep","Tcl_GlobalEval","Tcl_GlobalEvalObj","Tcl_VarEval","Tcl_VarEvalVA","Tcl_GetCommandFullName","Tcl_AddObjErrorInfo","Tcl_SetObjErrorCode","Tcl_SetErrorCode","Tcl_WrongNumArgs","Tcl_GetCommandName","Tcl_GetCommandFromObj","Tcl_DeleteCommandFromToken","Tcl_ExprObj","Tcl_AppendAllObjTypes","Tcl_SetAssocData","Tcl_GetAssocData","Tcl_DeleteAssocData","Tcl_CallWhenDeleted","Tcl_DontCallWhenDeleted","Tcl_GetVar","Tcl_SetVar","Tcl_UnsetVar","Tcl_ObjGetVar2","Tcl_ObjSetVar2","Tcl_ExprLongObj","Tcl_ExprDoubleObj","Tcl_ExprBooleanObj","Tcl_AttemptSetObjLength","Tcl_SetObjLength","Tcl_SetByteArrayLength","Tcl_GetByteArrayFromObj","Tcl_NewByteArrayObj","Tcl_GetStringFromObj","Tcl_DStringInit","Tcl_DStringFree","Tcl_DStringAppend","Tcl_DStringAppendElement","Tcl_DStringSetLength","Tcl_DStringStartSublist","Tcl_DStringEndSublist","Tcl_DStringGetResult","Tcl_DStringResult","Tcl_CreateCommand","Tcl_DeleteCommand","Tcl_AppendResult","Tcl_AppendObjToObj","Tcl_EvalEx","Tcl_EvalObjEx","Tcl_GetLongFromObj","Tcl_NewObj","Tcl_SetUnicodeObj","Tcl_GetCharLength","Tcl_GetUniChar","Tcl_GetRange","Tcl_AppendUnicodeToObj","Tcl_SetBooleanObj","Tcl_SetListObj","Tcl_SetErrorLine","Tcl_GetErrorLine","Tcl_Alloc","Tcl_Realloc","Tcl_AttemptRealloc","Tcl_Free","Tcl_GetVar2","Tcl_ExprLong"};
    CHECK(TKGPU_EnableReferenceStubs(i,required,sizeof required/sizeof required[0])==TCL_OK);
    CHECK(!Tcl_InitStubs(i,"9.0",0));
    CHECK(Tcl_InitStubs(i,"8.6",0));
    /* Every advertised slot must be present in the actual reference table. */
    CHECK(tclStubsPtr&&tclStubsPtr->tcl_PkgRequireEx&&tclStubsPtr->tclFreeObj);
#define TKGPU_SLOT(name) CHECK(tclStubsPtr->tcl_##name!=NULL);
#include "../../../src/compat/stubs/TKGPUReferenceSlots.H"
#undef TKGPU_SLOT

    CHECK(Tcl_Eval(i,"set answer 42")==TCL_OK);
    CHECK(!strcmp(Tcl_GetString(Tcl_GetObjResult(i)),"42"));
    Tcl_Obj *a=Tcl_NewIntObj(7); CHECK(a);
    Tcl_IncrRefCount(a);
    Tcl_Obj *list=Tcl_NewListObj(1,&a); CHECK(list);
    Tcl_IncrRefCount(list);
    int count=0,n=0; Tcl_Obj *item=NULL;
    CHECK(Tcl_ListObjLength(i,list,&count)==TCL_OK&&count==1);
    CHECK(Tcl_ListObjIndex(i,list,0,&item)==TCL_OK&&item==a);
    CHECK(Tcl_GetIntFromObj(i,item,&n)==TCL_OK&&n==7);
    Tcl_DecrRefCount(list); Tcl_DecrRefCount(a);
    Tcl_SetObjResult(i,Tcl_NewStringObj("stub call",-1));
    CHECK(!strcmp(Tcl_GetString(Tcl_GetObjResult(i)),"stub call"));
    char *buffer=Tcl_Alloc(4); CHECK(buffer);
    memcpy(buffer,"abc",4);
    buffer=Tcl_Realloc(buffer,32); CHECK(buffer&&!strcmp(buffer,"abc"));
    char *next=Tcl_AttemptRealloc(buffer,64); CHECK(next&&!strcmp(next,"abc"));
    Tcl_Free(next);
    CHECK(!strcmp(Tcl_GetVar2(i,"answer",NULL,0),"42"));
    long expr=0; CHECK(Tcl_ExprLong(i,"6*7",&expr)==TCL_OK&&expr==42);
    Tcl_Obj *unicode=Tcl_NewObj(); CHECK(unicode);
    Tcl_IncrRefCount(unicode);
    Tcl_UniChar letters[]={0x41,0x05d0,0x4e2d};
    Tcl_SetUnicodeObj(unicode,letters,3);
    CHECK(Tcl_GetCharLength(unicode)==3&&Tcl_GetUniChar(unicode,1)==0x05d0);
    Tcl_Obj *slice=Tcl_GetRange(unicode,1,2); CHECK(slice);
    Tcl_IncrRefCount(slice);
    CHECK(Tcl_GetCharLength(slice)==2&&Tcl_GetUniChar(slice,1)==0x4e2d);
    Tcl_AppendUnicodeToObj(unicode,letters,1);
    CHECK(Tcl_GetCharLength(unicode)==4);
    Tcl_SetBooleanObj(unicode,7); long number=0;
    CHECK(Tcl_GetLongFromObj(i,unicode,&number)==TCL_OK&&number==1);
    Tcl_SetListObj(unicode,1,&slice);
    int size=0; CHECK(Tcl_ListObjLength(i,unicode,&size)==TCL_OK&&size==1);
    Tcl_DecrRefCount(unicode); Tcl_DecrRefCount(slice);
    Tcl_SetErrorLine(i,17); CHECK(Tcl_GetErrorLine(i)==17);
    CHECK(Tcl_EvalEx(i,"set x 12INVALID",8,0)==TCL_OK);
    CHECK(!strcmp(Tcl_GetString(Tcl_GetObjResult(i)),"12"));
    const char embedded[]={'s','e','t',' ','x',' ','9',0,'x'};
    CHECK(Tcl_EvalEx(i,embedded,sizeof embedded,0)==TCL_ERROR);
    Tcl_Obj *script=Tcl_NewStringObj("set y ",-1); CHECK(script);
    Tcl_IncrRefCount(script);
    Tcl_Obj *tail=Tcl_NewStringObj("73",-1); CHECK(tail);
    Tcl_IncrRefCount(tail); Tcl_AppendObjToObj(script,tail); Tcl_DecrRefCount(tail);
    CHECK(Tcl_EvalObjEx(i,script,0)==TCL_OK&&!strcmp(Tcl_GetString(Tcl_GetObjResult(i)),"73"));
    CHECK(Tcl_GlobalEval(i,"set global 11")==TCL_OK);
    CHECK(Tcl_GlobalEvalObj(i,script)==TCL_OK);
    CHECK(Tcl_VarEval(i,"set ","global ","{two words}",(char *)NULL)==TCL_OK);
    CHECK(!strcmp(Tcl_GetString(Tcl_GetObjResult(i)),"two words"));
    Tcl_DecrRefCount(script);
    Tcl_Command token=Tcl_CreateCommand(i,"string_callback",string_callback,(ClientData)"prefix:",NULL); CHECK(token);
    Tcl_Obj *cmdName=Tcl_NewStringObj("string_callback",-1); CHECK(cmdName); Tcl_IncrRefCount(cmdName);
    CHECK(Tcl_GetCommandFromObj(i,cmdName)==token);
    CHECK(!strcmp(Tcl_GetCommandName(i,token),"string_callback")); Tcl_DecrRefCount(cmdName);
    Tcl_Obj *full=Tcl_NewObj(); CHECK(full); Tcl_IncrRefCount(full);
    Tcl_GetCommandFullName(i,token,full);
    CHECK(!strcmp(Tcl_GetString(full),"::string_callback")); Tcl_DecrRefCount(full);
    CHECK(Tcl_Eval(i,"string_callback {x y}")==TCL_OK);
    CHECK(!strcmp(Tcl_GetString(Tcl_GetObjResult(i)),"prefix:x y"));
    CHECK(Tcl_DeleteCommandFromToken(i,token)==0);
    CHECK(Tcl_SetVar(i,"alias","7",0)&&!strcmp(Tcl_GetVar(i,"alias",0),"7"));
    Tcl_Obj *key=Tcl_NewStringObj("array",-1),*part=Tcl_NewStringObj("key",-1),*value=Tcl_NewIntObj(9);
    CHECK(key&&part&&value); Tcl_IncrRefCount(key); Tcl_IncrRefCount(part); Tcl_IncrRefCount(value);
    CHECK(Tcl_ObjSetVar2(i,key,part,value,0)==value);
    CHECK(Tcl_ObjGetVar2(i,key,part,0)==value);
    Tcl_DecrRefCount(key); Tcl_DecrRefCount(part); Tcl_DecrRefCount(value);
    CHECK(Tcl_UnsetVar(i,"alias",0)==TCL_OK&&!Tcl_GetVar(i,"alias",0));
    Tcl_Obj *formula=Tcl_NewStringObj("6*7",-1); CHECK(formula); Tcl_IncrRefCount(formula);
    long integer=0; double real=0; int truth=0;
    CHECK(Tcl_ExprLongObj(i,formula,&integer)==TCL_OK&&integer==42);
    CHECK(Tcl_ExprDoubleObj(i,formula,&real)==TCL_OK&&real==42);
    CHECK(Tcl_ExprBooleanObj(i,formula,&truth)==TCL_OK&&truth==1);
    Tcl_DecrRefCount(formula);
    Tcl_Obj *mutable=Tcl_NewStringObj("ABC",-1); CHECK(mutable); Tcl_IncrRefCount(mutable);
    CHECK(Tcl_AttemptSetObjLength(mutable,5));
    memcpy(Tcl_GetString(mutable),"ABCDE",5);
    Tcl_SetObjLength(mutable,3); CHECK(!strcmp(Tcl_GetString(mutable),"ABC"));
    unsigned char *raw=Tcl_SetByteArrayLength(mutable,4); CHECK(raw); raw[3]='D';
    CHECK(!strcmp(Tcl_GetString(mutable),"ABCD"));
    Tcl_DecrRefCount(mutable);
    Tcl_DString ds; Tcl_DStringInit(&ds);
    Tcl_DStringAppendElement(&ds,"first");
    Tcl_DStringStartSublist(&ds);
    Tcl_DStringAppendElement(&ds,"two words");
    Tcl_DStringEndSublist(&ds);
    CHECK(!strcmp(Tcl_DStringValue(&ds),"first {{two words}}"));
    Tcl_DStringResult(i,&ds);
    CHECK(Tcl_DStringLength(&ds)==0);
    Tcl_DStringGetResult(i,&ds);
    CHECK(!strcmp(Tcl_DStringValue(&ds),"first {{two words}}"));
    Tcl_DStringFree(&ds);
    Tcl_Obj *expression=Tcl_NewStringObj("2.5+1",-1),*computed=NULL;
    CHECK(expression); Tcl_IncrRefCount(expression);
    CHECK(Tcl_Eval(i,"set keep prior")==TCL_OK);
    Tcl_Obj *kept=Tcl_GetObjResult(i);
    CHECK(Tcl_ExprObj(i,expression,&computed)==TCL_OK&&computed);
    CHECK(!strcmp(Tcl_GetString(computed),"3.5")&&Tcl_GetObjResult(i)==kept);
    Tcl_DecrRefCount(expression); Tcl_DecrRefCount(computed);
    Tcl_Obj *types=Tcl_NewListObj(0,NULL); CHECK(types); Tcl_IncrRefCount(types);
    int nt=0; CHECK(Tcl_AppendAllObjTypes(i,types)==TCL_OK);
    CHECK(Tcl_ListObjLength(i,types,&nt)==TCL_OK&&nt>=5); Tcl_DecrRefCount(types);
    CHECK(Tcl_Eval(i,"error fail")==TCL_ERROR);
    Tcl_AddObjErrorInfo(i," context ignored",8);
    CHECK(!strcmp(Tcl_GetVar(i,"errorInfo",0),"fail context"));
    Tcl_SetErrorCode(i,"TKGPU","TEST",(char *)NULL);
    CHECK(!strcmp(Tcl_GetVar(i,"errorCode",0),"TKGPU TEST"));
    CHECK(!strcmp(Tcl_GetString(Tcl_GetObjResult(i)),"fail"));
    Tcl_Obj *ec=Tcl_NewStringObj("SECOND CODE",-1); CHECK(ec); Tcl_IncrRefCount(ec);
    Tcl_SetObjErrorCode(i,ec); CHECK(!strcmp(Tcl_GetVar(i,"errorCode",0),"SECOND CODE"));
    Tcl_DecrRefCount(ec);
    Tcl_Obj *cmdArg=Tcl_NewStringObj("demo",-1); CHECK(cmdArg); Tcl_IncrRefCount(cmdArg);
    Tcl_WrongNumArgs(i,1,&cmdArg,"value");
    CHECK(!strcmp(Tcl_GetString(Tcl_GetObjResult(i)),"wrong # args: should be \"demo value\""));
    Tcl_DecrRefCount(cmdArg);
    Tcl_UniChar units[]={65,0,0xd83d,0xde80,0};
    Tcl_UniChar compare[]={65,0,0xd83d,0xde81,0};
    char encoded[8]; CHECK(Tcl_UniCharToUtf(0,encoded)==2&&!memcmp(encoded,"\xc0\x80",2));
    CHECK(Tcl_UniCharToUtf(0x10000,encoded)==3&&!memcmp(encoded,"\xef\xbf\xbd",3));
    CHECK(Tcl_UniCharLen(units)==1&&Tcl_UniCharNcmp(units,compare,4)==-1);
    Tcl_DStringInit(&ds); Tcl_DStringAppend(&ds,"prefix",6);
    char *suffix=Tcl_UniCharToUtfDString(units,4,&ds);
    CHECK(suffix==Tcl_DStringValue(&ds)+6&&Tcl_DStringLength(&ds)==15);
    CHECK(!memcmp(suffix,"A\xc0\x80\xed\xa0\xbd\xed\xba\x80",9)); Tcl_DStringFree(&ds);
    Tcl_Time before,after; Tcl_GetTime(&before); Tcl_Sleep(2); Tcl_GetTime(&after);
    CHECK(before.sec>0&&before.usec>=0&&before.usec<1000000);
    CHECK(after.sec>before.sec||(after.sec==before.sec&&after.usec>=before.usec));
    const char *parts[]={"  a b "," {c d} "};
    char *joined=Tcl_Concat(2,parts); CHECK(joined&&!strcmp(joined,"a b {c d}")); Tcl_Free(joined);
    const char *words[]={"a b","c"};
    char *merged=Tcl_Merge(2,words); CHECK(merged&&!strcmp(merged,"{a b} c")); Tcl_Free(merged);
    Tcl_Obj *pieces[]={Tcl_NewStringObj(" a b ",-1),Tcl_NewStringObj(" c ",-1)};
    CHECK(pieces[0]&&pieces[1]); Tcl_IncrRefCount(pieces[0]); Tcl_IncrRefCount(pieces[1]);
    Tcl_Obj *combined=Tcl_ConcatObj(2,pieces); CHECK(combined); Tcl_IncrRefCount(combined);
    CHECK(!strcmp(Tcl_GetString(combined),"a b c")); Tcl_DecrRefCount(combined);
    Tcl_DecrRefCount(pieces[0]); Tcl_DecrRefCount(pieces[1]);
    int parsed=0; CHECK(Tcl_GetInt(i,"42",&parsed)==TCL_OK&&parsed==42);
    const char *options[]={"start","stop","status",NULL};
    Tcl_Obj *opt=Tcl_NewStringObj("sto",-1); CHECK(opt); Tcl_IncrRefCount(opt);
    int index=-1; CHECK(Tcl_GetIndexFromObj(i,opt,options,"option",0,&index)==TCL_OK&&index==1);
    CHECK(Tcl_GetIndexFromObj(i,opt,options,"option",TCL_EXACT,&index)==TCL_ERROR&&index==1);
    Tcl_DecrRefCount(opt);
    struct { const char *name; int value; } rows[]={{"alpha",1},{"beta",2},{NULL,0}};
    opt=Tcl_NewStringObj("b",-1); CHECK(opt); Tcl_IncrRefCount(opt);
    CHECK(Tcl_GetIndexFromObjStruct(i,opt,rows,sizeof rows[0],"option",0,&index)==TCL_OK&&index==1);
    Tcl_DecrRefCount(opt);
    CHECK(!Tcl_UtfCharComplete("\xc2",1)&&Tcl_UtfCharComplete("\xc2\xa2",2));
    CHECK(!Tcl_UtfCharComplete("\xf0\x9f",2)&&Tcl_UtfCharComplete("\xf0\x9f\x9a",3));
    Tcl_Obj *joinedObj=Tcl_NewStringObj("abc",-1); CHECK(joinedObj); Tcl_IncrRefCount(joinedObj);
    const char *borrow=Tcl_GetString(joinedObj);
    Tcl_AppendStringsToObj(joinedObj,borrow,borrow+1,(char *)NULL);
    CHECK(!strcmp(Tcl_GetString(joinedObj),"abcabcbc"));
    CHECK(Tcl_GetUnicode(joinedObj)[7]=='c'); Tcl_DecrRefCount(joinedObj);
    const char *emptyOptions[]={"","start",NULL};
    opt=Tcl_NewStringObj("bad",-1); CHECK(opt); Tcl_IncrRefCount(opt);
    CHECK(Tcl_GetIndexFromObj(i,opt,emptyOptions,"option",0,&index)==TCL_ERROR);
    CHECK(!strcmp(Tcl_GetString(Tcl_GetObjResult(i)),"bad option \"bad\": must be start"));
    Tcl_DecrRefCount(opt);
    const char modified[]="A\xc0\x80\xf0\x9f\x9a\x80";
    CHECK(Tcl_NumUtfChars(modified,-1)==4);
    CHECK(Tcl_NumUtfChars("a\0b",3)==3);
    CHECK(Tcl_NumUtfChars("\xe2\x82",2)==2);
    Tcl_DStringInit(&ds); Tcl_DStringAppend(&ds,"AB",2);
    Tcl_UniChar *decoded=Tcl_UtfToUniCharDString(modified,-1,&ds);
    CHECK(Tcl_DStringLength(&ds)==10&&decoded[0]==65&&decoded[1]==0&&
          decoded[2]==0xd83d&&decoded[3]==0xde80&&decoded[4]==0);
    Tcl_DStringFree(&ds);
    Tcl_DStringInit(&ds);
    decoded=Tcl_UtfToUniCharDString("\xf0\x9f\x9a",3,&ds);
    CHECK(Tcl_DStringLength(&ds)==6&&decoded[0]==0xd83d&&decoded[1]==0x9f&&decoded[2]==0x9a&&decoded[3]==0);
    Tcl_DStringFree(&ds);
    /* Object cache and bounded conversion must agree on malformed bytes,
       modified NUL, and incomplete supplementary sequences. */
    const struct { const char *bytes; int len, count; Tcl_UniChar expected[4]; } utfCases[]={
        {"\xff",1,1,{0xff}}, {"\x80",1,1,{0x80}},
        {"\xc0\x81",2,2,{0xc0,0x81}}, {"\xe2\x82",2,2,{0xe2,0x82}},
        {"\xf0\x9f\x9a",3,3,{0xd83d,0x9f,0x9a}},
        {"\xf0\x9f\x9a\x80",4,2,{0xd83d,0xde80}},
        {"a\0b",3,3,{97,0,98}}, {"\xc0\x80",2,1,{0}}
    };
    for(size_t k=0;k<sizeof utfCases/sizeof utfCases[0];k++) {
        Tcl_Obj *u=Tcl_NewStringObj(utfCases[k].bytes,utfCases[k].len); CHECK(u);
        Tcl_IncrRefCount(u);
        int un=-1; Tcl_UniChar *up=Tcl_GetUnicodeFromObj(u,&un);
        CHECK(up&&un==utfCases[k].count&&up[un]==0);
        CHECK(!memcmp(up,utfCases[k].expected,(size_t)un*sizeof *up));
        CHECK(Tcl_GetUnicode(u)==up&&Tcl_GetCharLength(u)==un);
        Tcl_AppendToObj(u,"A",1);
        up=Tcl_GetUnicodeFromObj(u,&un);
        CHECK(up&&un==utfCases[k].count+1&&up[un-1]==65&&up[un]==0);
        CHECK(!memcmp(up,utfCases[k].expected,(size_t)(un-1)*sizeof *up));
        Tcl_DecrRefCount(u);
    }
    const char search[]="x\xf0\x9f\x9a\x80y\xed\xa0\xbd\xed\xba\x80x\xc0\x80";
    CHECK(Tcl_UtfFindFirst(search,0x1f680)==search+1);
    CHECK(Tcl_UtfFindLast(search,0x1f680)==search+6);
    CHECK(Tcl_UtfFindFirst(search,0xd83d)==NULL);
    CHECK(Tcl_UtfFindLast(search,0xde80)==NULL);
    CHECK(Tcl_UtfFindFirst(search,'x')==search);
    CHECK(Tcl_UtfFindLast(search,'x')==search+12);
    CHECK(Tcl_UtfFindFirst(search,0)==search+13);
    CHECK(Tcl_UtfFindLast(search,0)==search+15);
    CHECK(Tcl_UtfFindFirst(search,'z')==NULL);
    const char broken[]="\xf0\x9f\x9a";
    CHECK(Tcl_UtfFindFirst(broken,0xd83d)==broken);
    CHECK(Tcl_UtfFindLast(broken,0x9a)==broken+2);
    const char nav[]="A\xf0\x9f\x9a\x80""B";
    CHECK(Tcl_UtfAtIndex(nav,0)==nav&&Tcl_UtfAtIndex(nav,1)==nav+1);
    CHECK(Tcl_UtfAtIndex(nav,2)==nav+2&&Tcl_UtfAtIndex(nav,3)==nav+5);
    CHECK(Tcl_UtfAtIndex(nav,4)==nav+6&&Tcl_UtfAtIndex(nav,99)==nav+6);
    CHECK(Tcl_UniCharAtIndex(nav,1)==0xd83d&&Tcl_UniCharAtIndex(nav,2)==0xde80);
    CHECK(Tcl_UniCharAtIndex(nav,3)=='B'&&Tcl_UniCharAtIndex(nav,-1)==0);
    CHECK(Tcl_UtfNext(nav+1)==nav+2&&Tcl_UtfNext(nav+6)==nav+6);
    const char nav2[]="\xed\xa0\xbd\xed\xba\x80\xc0\x80";
    CHECK(Tcl_UtfAtIndex(nav2,1)==nav2+3&&Tcl_UtfAtIndex(nav2,2)==nav2+6);
    CHECK(Tcl_UtfNext(nav2)==nav2+3&&Tcl_UtfNext(nav2+6)==nav2+8);
    CHECK(Tcl_UniCharAtIndex(nav2,2)==0&&Tcl_UtfAtIndex(nav2,3)==nav2+8);
    CHECK(Tcl_UniCharAtIndex(broken,0)==0xd83d&&Tcl_UniCharAtIndex(broken,1)==0x9f);
    CHECK(Tcl_UtfPrev(nav+5,nav)==nav+4&&Tcl_UtfPrev(nav,nav)==nav);
    CHECK(Tcl_UtfPrev(nav2+3,nav2)==nav2);
    Tcl_UniChar unit=0; CHECK(Tcl_UtfToUniChar(nav+1,&unit)==1&&unit==0xd83d);
    CHECK(Tcl_UtfToUniChar(nav+2,&unit)==3&&unit==0xde80);
    CHECK(Tcl_UtfNcmp(nav+1,nav2,2)==0);
    CHECK(Tcl_UtfNcmp("a","z",1)<0&&Tcl_UtfNcmp("\xc0\x80","",1)==0);
    char esc[8]; int consumed=0;
    CHECK(Tcl_UtfBackslash("\\xFF",&consumed,esc)==2&&consumed==4&&!memcmp(esc,"\xc3\xbf",2));
    CHECK(Tcl_UtfBackslash("\\0",&consumed,esc)==2&&consumed==2&&!memcmp(esc,"\xc0\x80",2));
    CHECK(Tcl_UtfBackslash("\\U00110000",&consumed,esc)==3&&consumed==9&&!memcmp(esc,"\xef\xbf\xbd",3));
    CHECK(Tcl_UtfBackslash("\\\n \tX",&consumed,esc)==1&&consumed==4&&esc[0]==' ');
    CHECK(Tcl_Backslash("\\777",&consumed)==63&&consumed==3);
    Tcl_ResetResult(i); append_result_va(i,"left"," right",(char *)NULL);
    CHECK(!strcmp(Tcl_GetStringResult(i),"left right"));
    Tcl_Obj *vaObj=Tcl_NewStringObj("ab",-1); CHECK(vaObj); Tcl_IncrRefCount(vaObj);
    const char *vaBorrow=Tcl_GetString(vaObj);
    append_object_va(vaObj,vaBorrow,vaBorrow+1,(char *)NULL);
    CHECK(!strcmp(Tcl_GetString(vaObj),"ababb")); Tcl_DecrRefCount(vaObj);
    int quoteFlags=-1; char quote[64];
    CHECK(Tcl_ScanElement("a b",&quoteFlags)==5&&quoteFlags==2);
    CHECK(Tcl_ConvertElement("a b",quote,quoteFlags)==5&&!strcmp(quote,"{a b}"));
    CHECK(Tcl_ConvertElement("a b",quote,quoteFlags|1)==4&&!strcmp(quote,"a\\ b"));
    CHECK(Tcl_ScanElement("#a]",&quoteFlags)==6&&quoteFlags==2);
    CHECK(Tcl_ConvertElement("#a]",quote,quoteFlags)==5&&!strcmp(quote,"{#a]}"));
    CHECK(Tcl_ScanElement("a{b}c\"",&quoteFlags)==9&&quoteFlags==6);
    CHECK(Tcl_ConvertElement("a{b}c\"",quote,quoteFlags)==7&&!strcmp(quote,"a{b}c\\\""));
    CHECK(Tcl_ScanElement("a\\",&quoteFlags)==3&&quoteFlags==4);
    CHECK(Tcl_ConvertElement("a\\",quote,quoteFlags)==3&&!strcmp(quote,"a\\\\"));
    const char binaryList[]={'a',0,'b',' '};
    CHECK(Tcl_ScanCountedElement(binaryList,4,&quoteFlags)==6&&quoteFlags==2);
    CHECK(Tcl_ConvertCountedElement(binaryList,4,quote,quoteFlags)==6&&
          !memcmp(quote,"{a\0b }",6)&&quote[6]==0);
    CHECK(Tcl_ScanElement("#tag",&quoteFlags)==6&&quoteFlags==0);
    CHECK(Tcl_ConvertElement("#tag",quote,quoteFlags|8)==4&&!strcmp(quote,"#tag"));
    CHECK(Tcl_StringMatch("abc","a*[c-a]"));
    CHECK(Tcl_StringMatch("a","[a")&&!Tcl_StringMatch("a","[a-"));
    CHECK(Tcl_StringMatch("\xf0\x9f\x9a\x80","?")&&!Tcl_StringMatch("\xf0\x9f\x9a\x80","??"));
    CHECK(Tcl_StringMatch("\xed\xa0\xbd\xed\xba\x80","\xf0\x9f\x9a\x80"));
    CHECK(Tcl_StringMatch("\xc0\x80","?")&&!Tcl_StringMatch("\xc0\x80",""));
    CHECK(Tcl_StringMatch("a*b","a\\*b")&&!Tcl_StringMatch("ab","a\\*b"));
    Tcl_Obj *limited=Tcl_NewStringObj("abcdef",-1); CHECK(limited); Tcl_IncrRefCount(limited);
    const char *limitedBorrow=Tcl_GetString(limited);
    Tcl_AppendLimitedToObj(limited,limitedBorrow,6,4,limitedBorrow+5);
    CHECK(!strcmp(Tcl_GetString(limited),"abcdefabcf"));
    Tcl_SetStringObj(limited,"",0);
    Tcl_AppendLimitedToObj(limited,"\xc3\xa9\xc3\xa9\xc3\xa9",6,4,NULL);
    CHECK(!strcmp(Tcl_GetString(limited),"..."));
    Tcl_SetStringObj(limited,"",0);
    Tcl_AppendLimitedToObj(limited,"a\0bcd",5,3,"!");
    int limLen=0; const char *limBytes=Tcl_GetStringFromObj(limited,&limLen);
    CHECK(limBytes&&limLen==3&&!memcmp(limBytes,"a\0!",3));
    Tcl_DecrRefCount(limited);
    Tcl_Obj *historyObj=Tcl_NewStringObj("set recorded 42",-1); CHECK(historyObj);
    Tcl_IncrRefCount(historyObj);
    CHECK(Tcl_RecordAndEvalObj(i,historyObj,0)==TCL_OK);
    CHECK(!strcmp(Tcl_GetVar(i,"recorded",0),"42"));
    Tcl_SetStringObj(historyObj,"set recorded 99",-1);
    CHECK(Tcl_RecordAndEvalObj(i,historyObj,TCL_NO_EVAL)==TCL_OK);
    CHECK(!strcmp(Tcl_GetVar(i,"recorded",0),"42"));
    Tcl_SetStringObj(historyObj,"set recorded 0\0x",16);
    CHECK(Tcl_RecordAndEvalObj(i,historyObj,0)==TCL_ERROR);
    Tcl_DecrRefCount(historyObj);
    CHECK(Tcl_UniCharIsAlnum(0x0665)&&Tcl_UniCharIsAlpha(0x4e2d));
    CHECK(Tcl_UniCharIsUpper(0x0416)&&Tcl_UniCharIsLower(0x0436));
    CHECK(Tcl_UniCharIsDigit(0xff19)&&!Tcl_UniCharIsDigit(0x00b2));
    CHECK(Tcl_UniCharIsControl(0x200b)&&!Tcl_UniCharIsGraph(0x200b));
    CHECK(Tcl_UniCharIsSpace(0xfeff)&&Tcl_UniCharIsSpace(0x85));
    CHECK(Tcl_UniCharIsPrint(0x2028)&&!Tcl_UniCharIsGraph(0x2028));
    CHECK(Tcl_UniCharIsPunct('_')&&Tcl_UniCharIsWordChar('_'));
    CHECK(!Tcl_UniCharIsWordChar(0x0301)&&!Tcl_UniCharIsAlpha(0xd800));
    CHECK(Tcl_UniCharIsUpper(0x10041)&&!Tcl_UniCharIsAlpha(-1));
    CHECK(Tcl_UniCharIsAlpha(0x1c89)&&Tcl_UniCharIsLower(0x1c8a));
    CHECK(Tcl_UniCharToLower(0x0130)==0x0069);
    CHECK(Tcl_UniCharToUpper(0x1f80)==0x1f88);
    CHECK(Tcl_UniCharToTitle(0x01c4)==0x01c5);
    CHECK(Tcl_UniCharToTitle(0x10d0)==0x10d0);
    CHECK(Tcl_UniCharToUpper(0x0436)==0x0416);
    CHECK(Tcl_UniCharToLower(0x1c89)==0x1c8a);
    CHECK(Tcl_UniCharToLower(0x10041)==0x0041);
    Tcl_UniChar caseA[]={0x0416,0,0x0042},caseB[]={0x0436,0,0x0061};
    CHECK(Tcl_UniCharNcasecmp(caseA,caseB,2)==0);
    CHECK(Tcl_UniCharNcasecmp(caseA,caseB,3)==1);
    int oldErrno=Tcl_GetErrno();
    Tcl_SetErrno(23); CHECK(Tcl_GetErrno()==23);
    Tcl_SetErrno(oldErrno);
    char lowerUtf[]="\xc4\xb0""ABC";
    CHECK(Tcl_UtfToLower(lowerUtf)==4&&!strcmp(lowerUtf,"iabc"));
    char upperUtf[]="\xc9\x90""abc";
    CHECK(Tcl_UtfToUpper(upperUtf)==5&&!strcmp(upperUtf,"\xc9\x90""ABC"));
    char titleUtf[]="hELLO\xe1\xb2\x90";
    CHECK(Tcl_UtfToTitle(titleUtf)==8&&!strcmp(titleUtf,"Hello\xe1\xb2\x90"));
    char safeScalar[]="\xf0\x9f\x9a\x80""ABC";
    CHECK(Tcl_UtfToLower(safeScalar)==7&&!strcmp(safeScalar,"\xf0\x9f\x9a\x80""abc"));
    CHECK(Tcl_UtfNcasecmp("\xd0\x96","\\",1)>0);
    CHECK(Tcl_UtfNcasecmp("\xd0\x96","\xd0\xb6",1)==0);
    CHECK(Tcl_UtfNcasecmp("\xc0\x80""a","\xc0\x80""b",2)==-1);
    Tcl_UniChar globS[]={'A','b','c',0},globP[]={'a','*','[','C','-','A',']',0};
    CHECK(Tcl_UniCharCaseMatch(globS,globP,1)&&!Tcl_UniCharCaseMatch(globS,globP,0));
    Tcl_UniChar globPair[]={0xd83d,0xde80,0},globOne[]={'?',0},globTwo[]={'?','?',0};
    CHECK(!Tcl_UniCharCaseMatch(globPair,globOne,0)&&Tcl_UniCharCaseMatch(globPair,globTwo,0));
    CHECK(Tcl_StringCaseMatch("AbC","a*[c-a]",1));
    CHECK(!Tcl_StringCaseMatch("AbC","a*[c-a]",0));
    CHECK(Tcl_StringCaseMatch("\xd0\x96","\xd0\xb6",1));
    CHECK(Tcl_StringCaseMatch("\xf0\x9f\x9a\x80","?",1));
    CHECK(Tcl_Eval(i,"error wrapper")==TCL_ERROR);
    Tcl_Obj *errExtra=Tcl_NewStringObj(" extra",-1); CHECK(errExtra);
    Tcl_IncrRefCount(errExtra);
    Tcl_AppendObjToErrorInfo(i,errExtra);
    CHECK(!strcmp(Tcl_GetVar(i,"errorInfo",0),"wrapper extra"));
    CHECK(errExtra->refCount==1); Tcl_DecrRefCount(errExtra);
    set_error_va(i,"TKGPU","VA",(char *)NULL);
    CHECK(!strcmp(Tcl_GetVar(i,"errorCode",0),"TKGPU VA"));
    CHECK(!strcmp(Tcl_GetStringResult(i),"wrapper"));

    Tcl_StatBuf *statBuf=Tcl_AllocStatBuf(); CHECK(statBuf);
    memset(statBuf,0,sizeof *statBuf);
    statBuf->st_dev=23; statBuf->st_ino=456; statBuf->st_mode=0100644;
    statBuf->st_nlink=2; statBuf->st_uid=1000; statBuf->st_gid=1001;
    statBuf->st_rdev=7; statBuf->st_atime=100; statBuf->st_mtime=200;
    statBuf->st_ctime=300; statBuf->st_size=1234567;
    statBuf->st_blocks=4096; statBuf->st_blksize=4096;
    CHECK(Tcl_GetFSDeviceFromStat(statBuf)==(unsigned)statBuf->st_dev);
    CHECK(Tcl_GetFSInodeFromStat(statBuf)==(unsigned)statBuf->st_ino);
    CHECK(Tcl_GetModeFromStat(statBuf)==(unsigned)statBuf->st_mode);
    CHECK(Tcl_GetLinkCountFromStat(statBuf)==(int)statBuf->st_nlink);
    CHECK(Tcl_GetUserIdFromStat(statBuf)==(int)statBuf->st_uid);
    CHECK(Tcl_GetGroupIdFromStat(statBuf)==(int)statBuf->st_gid);
    CHECK(Tcl_GetDeviceTypeFromStat(statBuf)==(int)statBuf->st_rdev);
    CHECK(Tcl_GetAccessTimeFromStat(statBuf)==(Tcl_WideInt)statBuf->st_atime);
    CHECK(Tcl_GetModificationTimeFromStat(statBuf)==(Tcl_WideInt)statBuf->st_mtime);
    CHECK(Tcl_GetChangeTimeFromStat(statBuf)==(Tcl_WideInt)statBuf->st_ctime);
    CHECK(Tcl_GetSizeFromStat(statBuf)==(Tcl_WideUInt)statBuf->st_size);
    CHECK(Tcl_GetBlocksFromStat(statBuf)==(Tcl_WideUInt)statBuf->st_blocks);
    CHECK(Tcl_GetBlockSizeFromStat(statBuf)==(unsigned)statBuf->st_blksize);
    if(sizeof statBuf->st_size>=8) {
        statBuf->st_size=(Tcl_WideInt)0x100000001LL;
        CHECK(Tcl_GetSizeFromStat(statBuf)==(Tcl_WideUInt)0x100000001LL);
    }
    Tcl_Free((char *)statBuf);
    const unsigned char checksumText[]="123456789";
    CHECK(Tcl_ZlibCRC32(0,checksumText,9)==0xcbf43926u);
    CHECK(Tcl_ZlibAdler32(1,checksumText,9)==0x091e01deu);
    CHECK(Tcl_ZlibCRC32(Tcl_ZlibCRC32(0,checksumText,4),checksumText+4,5)==0xcbf43926u);
    CHECK(Tcl_ZlibAdler32(Tcl_ZlibAdler32(1,checksumText,4),checksumText+4,5)==0x091e01deu);
    CHECK(Tcl_ZlibCRC32(123,NULL,0)==0&&Tcl_ZlibAdler32(123,NULL,0)==1);
    CHECK(Tcl_ZlibAdler32(0xffffffffu,checksumText,0)==0x000e000eu);
    CHECK(Tcl_ZlibCRC32(123,checksumText,0)==123);
    CHECK(Tcl_UniCharToUpper(0x10061)==0x0061);
    CHECK(Tcl_UniCharToTitle(-1)==0xffff);
    int diagnosticSavedErrno=Tcl_GetErrno();
    Tcl_SetErrno(EACCES); CHECK(!strcmp(Tcl_ErrnoId(),"EACCES"));
    Tcl_SetErrno(ENOTSUP); CHECK(!strcmp(Tcl_ErrnoId(),"ENOTSUP"));
    Tcl_SetErrno(-1); CHECK(!strcmp(Tcl_ErrnoId(),"unknown error"));
    Tcl_SetErrno(diagnosticSavedErrno);
    CHECK(!strcmp(Tcl_SignalId(SIGINT),"SIGINT"));
    CHECK(!strcmp(Tcl_SignalMsg(SIGINT),"interrupt"));
    CHECK(!strcmp(Tcl_SignalId(-1),"unknown signal"));
    CHECK(!strcmp(Tcl_SignalMsg(-1),"unknown signal"));
    Tcl_Obj *legacyScript=Tcl_NewStringObj("set legacy_obj 19",-1); CHECK(legacyScript);
    Tcl_IncrRefCount(legacyScript);
    CHECK(Tcl_EvalObj(i,legacyScript)==TCL_OK);
    CHECK(!strcmp(Tcl_GetStringResult(i),"19"));
    CHECK(legacyScript->refCount==1);
    Tcl_DecrRefCount(legacyScript);
    Tcl_FindExecutable("tkgpu_reference_stubs_tests");
    const char *exeName=Tcl_GetNameOfExecutable();
    CHECK(exeName&&*exeName);
    CHECK(Tcl_Init(i)==TCL_OK);
    CHECK(!strcmp(exeName,Tcl_GetVar(i,"tkgpu_executable",0)));
    CHECK(Tcl_GetPathType("/tmp")==TCL_PATH_ABSOLUTE);
    CHECK(Tcl_GetPathType("~user/path")==TCL_PATH_ABSOLUTE);
    CHECK(Tcl_GetPathType("./~user")==TCL_PATH_RELATIVE);
    CHECK(Tcl_GetPathType("")==TCL_PATH_RELATIVE);
    CHECK(Tcl_GetPathType("C:/tmp")==TCL_PATH_RELATIVE);
    int pathCount=-1; const char **pathParts=NULL;
    Tcl_SplitPath("///a//~user/../",&pathCount,&pathParts);
    CHECK(pathCount==4&&pathParts&&pathParts[4]==NULL);
    CHECK(!strcmp(pathParts[0],"/")&&!strcmp(pathParts[1],"a"));
    CHECK(!strcmp(pathParts[2],"./~user")&&!strcmp(pathParts[3],".."));
    Tcl_Free((char *)pathParts);
    Tcl_SplitPath("",&pathCount,&pathParts);
    CHECK(pathCount==0&&pathParts&&pathParts[0]==NULL);
    Tcl_Free((char *)pathParts);
    Tcl_SplitPath("~user//\xd0\xb6/",&pathCount,&pathParts);
    CHECK(pathCount==2&&!strcmp(pathParts[0],"~user")&&!strcmp(pathParts[1],"\xd0\xb6"));
    Tcl_Free((char *)pathParts);
    Tcl_DString joinedPath; Tcl_DStringInit(&joinedPath);
    const char *joinedParts[]={"a","/./~user","b"};
    CHECK(!strcmp(Tcl_JoinPath(3,joinedParts,&joinedPath),"/~user/b"));
    Tcl_DStringFree(&joinedPath);
    Tcl_DStringInit(&joinedPath); Tcl_DStringAppend(&joinedPath,"prefix:",-1);
    const char *joinedTilde[]={"~user//","./~b",".."};
    CHECK(!strcmp(Tcl_JoinPath(3,joinedTilde,&joinedPath),"prefix:~user/~b/.."));
    Tcl_DStringFree(&joinedPath);
    Tcl_DStringInit(&joinedPath); Tcl_DStringAppend(&joinedPath,"abc",-1);
    const char *joinAlias[]={Tcl_DStringValue(&joinedPath),"x"};
    CHECK(!strcmp(Tcl_JoinPath(2,joinAlias,&joinedPath),"abcabc/x"));
    Tcl_DStringFree(&joinedPath);
    Tcl_HashTable hash; Tcl_InitHashTable(&hash,TCL_STRING_KEYS);
    for(int k=0;k<1024;k++) {
        char key[32]; snprintf(key,sizeof key,"key-%d",k);
        int isNew=-1; Tcl_HashEntry *entry=tclStubsPtr->tcl_CreateHashEntry(&hash,key,&isNew);
        CHECK(entry&&isNew==1);
        Tcl_SetHashValue(entry,(ClientData)(uintptr_t)(k+1));
        CHECK(Tcl_FindHashEntry(&hash,key)==entry);
        CHECK(tclStubsPtr->tcl_FindHashEntry(&hash,key)==entry);
        CHECK(Tcl_CreateHashEntry(&hash,key,&isNew)==entry&&isNew==0);
        CHECK(!strcmp((const char *)Tcl_GetHashKey(&hash,entry),key));
    }
    char *hashStats=tclStubsPtr->tcl_HashStats(&hash);
    CHECK(hashStats&&strstr(hashStats,"1024 entries in table,")&&strstr(hashStats,"average search distance for entry:"));
    Tcl_Free(hashStats);
    Tcl_HashSearch hashSearch; int hashSeen=0;
    for(Tcl_HashEntry *entry=Tcl_FirstHashEntry(&hash,&hashSearch);entry;entry=Tcl_NextHashEntry(&hashSearch)) {
        CHECK(Tcl_GetHashValue(entry)!=NULL); ++hashSeen; Tcl_DeleteHashEntry(entry);
    }
    CHECK(hashSeen==1024&&hash.numEntries==0); Tcl_DeleteHashTable(&hash);
    Tcl_InitHashTable(&hash,TCL_ONE_WORD_KEYS);
    int hashNew=0; Tcl_HashEntry *hashEntry=Tcl_CreateHashEntry(&hash,(const char *)NULL,&hashNew);
    CHECK(hashEntry&&hashNew==1&&Tcl_FindHashEntry(&hash,(const char *)NULL)==hashEntry);
    Tcl_DeleteHashTable(&hash);
    Tcl_InitHashTable(&hash,3);
    int compositeKey[]={1,-2,3}; int equivalentKey[]={1,-2,3};
    hashEntry=Tcl_CreateHashEntry(&hash,(const char *)compositeKey,&hashNew);
    CHECK(hashEntry&&hashNew==1&&Tcl_FindHashEntry(&hash,(const char *)equivalentKey)==hashEntry);
    compositeKey[0]=0; CHECK(!Tcl_FindHashEntry(&hash,(const char *)compositeKey));
    Tcl_DeleteHashTable(&hash);
    /* Call the slots explicitly; ordinary macros may select nondebug APIs. */
    char *dbmem=tclStubsPtr->tcl_DbCkalloc(4,__FILE__,__LINE__); CHECK(dbmem);
    memcpy(dbmem,"abc",4);
    dbmem=tclStubsPtr->tcl_DbCkrealloc(dbmem,64,__FILE__,__LINE__);
    CHECK(dbmem&&!strcmp(dbmem,"abc")); tclStubsPtr->tcl_DbCkfree(dbmem,__FILE__,__LINE__);
    dbmem=tclStubsPtr->tcl_AttemptDbCkalloc(8,__FILE__,__LINE__); CHECK(dbmem);
    memcpy(dbmem,"1234567",8);
    dbmem=tclStubsPtr->tcl_AttemptDbCkrealloc(dbmem,128,__FILE__,__LINE__);
    CHECK(dbmem&&!strcmp(dbmem,"1234567")); tclStubsPtr->tcl_DbCkfree(dbmem,__FILE__,__LINE__);
    Tcl_Obj *dbobjs[]={
        tclStubsPtr->tcl_DbNewBooleanObj(1,__FILE__,__LINE__),
        tclStubsPtr->tcl_DbNewByteArrayObj((const unsigned char *)"abc",3,__FILE__,__LINE__),
        tclStubsPtr->tcl_DbNewDoubleObj(1.25,__FILE__,__LINE__),
        tclStubsPtr->tcl_DbNewLongObj(-42,__FILE__,__LINE__),
        tclStubsPtr->tcl_DbNewObj(__FILE__,__LINE__),
        tclStubsPtr->tcl_DbNewStringObj("a\0b",3,__FILE__,__LINE__),
        tclStubsPtr->tcl_DbNewWideIntObj((Tcl_WideInt)0x100000001LL,__FILE__,__LINE__)
    };
    for(int k=0;k<7;k++) {
        CHECK(dbobjs[k]&&dbobjs[k]->refCount==0);
        tclStubsPtr->tcl_DbIncrRefCount(dbobjs[k],__FILE__,__LINE__);
        CHECK(!tclStubsPtr->tcl_DbIsShared(dbobjs[k],__FILE__,__LINE__));
        tclStubsPtr->tcl_DbIncrRefCount(dbobjs[k],__FILE__,__LINE__);
        CHECK(tclStubsPtr->tcl_DbIsShared(dbobjs[k],__FILE__,__LINE__));
        tclStubsPtr->tcl_DbDecrRefCount(dbobjs[k],__FILE__,__LINE__);
    }
    Tcl_Obj *dblist=tclStubsPtr->tcl_DbNewListObj(7,dbobjs,__FILE__,__LINE__);
    int dbcount=0; CHECK(dblist&&Tcl_ListObjLength(i,dblist,&dbcount)==TCL_OK&&dbcount==7);
    tclStubsPtr->tcl_DbDecrRefCount(dblist,__FILE__,__LINE__);
    for(int k=0;k<7;k++) {
        CHECK(dbobjs[k]->refCount==1);
        tclStubsPtr->tcl_DbDecrRefCount(dbobjs[k],__FILE__,__LINE__);
    }
    static const Tcl_HashKeyType ptrKeyType={TCL_HASH_KEY_TYPE_VERSION,TCL_HASH_KEY_RANDOMIZE_HASH,NULL,NULL,NULL,NULL};
    Tcl_InitCustomHashTable(&hash,TCL_CUSTOM_PTR_KEYS,&ptrKeyType);
    hashEntry=tclStubsPtr->tcl_CreateHashEntry(&hash,(void *)(uintptr_t)42,&hashNew);
    CHECK(hashEntry&&hashNew==1&&tclStubsPtr->tcl_FindHashEntry(&hash,(void *)(uintptr_t)42)==hashEntry);
    Tcl_DeleteHashTable(&hash);
    Tcl_InitObjHashTable(&hash);
    Tcl_Obj *keyA=Tcl_NewStringObj("a\0b",3),*keyB=Tcl_NewStringObj("a\0b",3),*keyC=Tcl_NewStringObj("a\0c",3);
    CHECK(keyA&&keyB&&keyC);
    Tcl_IncrRefCount(keyA); Tcl_IncrRefCount(keyB); Tcl_IncrRefCount(keyC);
    hashEntry=tclStubsPtr->tcl_CreateHashEntry(&hash,keyA,&hashNew);
    CHECK(hashEntry&&hashNew==1&&keyA->refCount==2);
    CHECK(tclStubsPtr->tcl_FindHashEntry(&hash,keyB)==hashEntry);
    CHECK(!tclStubsPtr->tcl_FindHashEntry(&hash,keyC));
    Tcl_DeleteHashTable(&hash); CHECK(keyA->refCount==1);
    Tcl_DecrRefCount(keyA); Tcl_DecrRefCount(keyB); Tcl_DecrRefCount(keyC);
    Tcl_ChannelType descriptor; memset(&descriptor,0,sizeof descriptor);
    descriptor.typeName="tkgpu-test"; descriptor.closeProc=TCL_CLOSE2PROC;
    for(uintptr_t level=1;level<=5;level++) {
        descriptor.version=(Tcl_ChannelTypeVersion)level;
        CHECK(!strcmp(Tcl_ChannelName(&descriptor),"tkgpu-test"));
        CHECK(Tcl_ChannelVersion(&descriptor)==descriptor.version);
        CHECK(Tcl_ChannelCloseProc(&descriptor)==TCL_CLOSE2PROC);
        CHECK(!Tcl_ChannelClose2Proc(&descriptor));
        CHECK(!Tcl_ChannelInputProc(&descriptor)&&!Tcl_ChannelOutputProc(&descriptor));
        CHECK(!Tcl_ChannelSeekProc(&descriptor)&&!Tcl_ChannelWideSeekProc(&descriptor));
        CHECK(!Tcl_ChannelSetOptionProc(&descriptor)&&!Tcl_ChannelGetOptionProc(&descriptor));
        CHECK(!Tcl_ChannelWatchProc(&descriptor)&&!Tcl_ChannelGetHandleProc(&descriptor));
        CHECK(!Tcl_ChannelFlushProc(&descriptor)&&!Tcl_ChannelHandlerProc(&descriptor));
        CHECK(!Tcl_ChannelThreadActionProc(&descriptor)&&!Tcl_ChannelTruncateProc(&descriptor));
        if(level>=2) CHECK(!Tcl_ChannelBlockModeProc(&descriptor));
    }
    Tcl_DoWhenIdle(native_idle,(void *)(uintptr_t)1);
    Tcl_DoWhenIdle(native_idle,(void *)(uintptr_t)2);
    Tcl_DoWhenIdle(native_idle,(void *)(uintptr_t)2);
    Tcl_DoWhenIdle(native_idle,(void *)(uintptr_t)3);
    Tcl_DoWhenIdle(native_idle,(void *)(uintptr_t)4);
    Tcl_CancelIdleCall(native_idle,(void *)(uintptr_t)2);
    CHECK(Tcl_DoOneEvent(TCL_IDLE_EVENTS|TCL_DONT_WAIT)==1);
    CHECK(idleCount==2&&idleTrace[0]==1&&idleTrace[1]==3);
    CHECK(Tcl_DoOneEvent(TCL_IDLE_EVENTS|TCL_DONT_WAIT)==1);
    CHECK(idleCount==3&&idleTrace[2]==9);
    CHECK(!Tcl_DoOneEvent(TCL_IDLE_EVENTS|TCL_DONT_WAIT));
    Tcl_SavedResult savedOuter,savedInner;
    Tcl_Obj *savedObject=Tcl_NewIntObj(314); CHECK(savedObject);
    Tcl_SetObjResult(i,savedObject);
    Tcl_SaveResult(i,&savedOuter);
    CHECK(!strcmp(Tcl_GetStringResult(i),"")&&savedObject->refCount==1);
    Tcl_SetObjResult(i,Tcl_NewStringObj("inner",-1));
    Tcl_SaveResult(i,&savedInner);
    Tcl_SetObjResult(i,Tcl_NewStringObj("temporary",-1));
    Tcl_DiscardResult(&savedInner); CHECK(!strcmp(Tcl_GetStringResult(i),"temporary"));
    Tcl_RestoreResult(i,&savedOuter);
    CHECK(Tcl_GetObjResult(i)==savedObject&&!strcmp(Tcl_GetStringResult(i),"314"));
    Tcl_Obj *fsPath=Tcl_NewStringObj("///a//~user/b/",-1); CHECK(fsPath);
    Tcl_IncrRefCount(fsPath); int fsCount=-1;
    CHECK(Tcl_FSGetPathType(fsPath)==TCL_PATH_ABSOLUTE);
    Tcl_Obj *fsParts=Tcl_FSSplitPath(fsPath,&fsCount); CHECK(fsParts&&fsCount==4);
    Tcl_Obj *fsPart=NULL; CHECK(Tcl_ListObjIndex(i,fsParts,2,&fsPart)==TCL_OK&&fsPart&&!strcmp(Tcl_GetString(fsPart),"./~user"));
    Tcl_DecrRefCount(fsParts);
    Tcl_Obj *fsSep=Tcl_FSPathSeparator(fsPath); CHECK(fsSep&&!strcmp(Tcl_GetString(fsSep),"/")); Tcl_DecrRefCount(fsSep);
    Tcl_Obj *fsInfo=Tcl_FSFileSystemInfo(fsPath); CHECK(fsInfo&&!strcmp(Tcl_GetString(fsInfo),"native")); Tcl_DecrRefCount(fsInfo);
    Tcl_Obj *fsVolumes=Tcl_FSListVolumes(); CHECK(fsVolumes&&!strcmp(Tcl_GetString(fsVolumes),"/")); Tcl_DecrRefCount(fsVolumes);
    Tcl_DecrRefCount(fsPath);
    Tcl_Obj *joinObjA=Tcl_NewStringObj("relative",-1),*joinObjB=Tcl_NewStringObj("/a\0b",4);
    CHECK(joinObjA&&joinObjB); Tcl_IncrRefCount(joinObjA); Tcl_IncrRefCount(joinObjB);
    Tcl_Obj *joinItems[]={joinObjA,joinObjB};
    Tcl_Obj *joinList=Tcl_NewListObj(2,joinItems); CHECK(joinList); Tcl_IncrRefCount(joinList);
    CHECK(Tcl_FSJoinPath(joinList,-1)==joinObjB);
    CHECK(Tcl_FSJoinPath(joinList,1)==joinObjA);
    CHECK(Tcl_FSJoinToPath(joinObjA,1,joinItems+1)==joinObjB);
    Tcl_DecrRefCount(joinList); Tcl_DecrRefCount(joinObjB); Tcl_DecrRefCount(joinObjA);
    joinObjA=Tcl_NewStringObj("///a/",-1); joinObjB=Tcl_NewStringObj("./~b",-1);
    CHECK(joinObjA&&joinObjB); Tcl_IncrRefCount(joinObjA); Tcl_IncrRefCount(joinObjB);
    Tcl_Obj *joinedPathObj=Tcl_FSJoinToPath(joinObjA,1,&joinObjB);
    CHECK(joinedPathObj&&!strcmp(Tcl_GetString(joinedPathObj),"/a/~b")&&joinedPathObj->refCount==0);
    Tcl_DecrRefCount(joinedPathObj); Tcl_DecrRefCount(joinObjA); Tcl_DecrRefCount(joinObjB);
    Tcl_SetErrno(ENOENT);
    CHECK(!strcmp(Tcl_ErrnoMsg(ENOENT),"no such file or directory"));
    Tcl_SetObjResult(i,Tcl_NewStringObj("keep",-1));
    CHECK(!strcmp(Tcl_PosixError(i),"no such file or directory"));
    CHECK(Tcl_GetErrno()==ENOENT&&!strcmp(Tcl_GetStringResult(i),"keep"));
    CHECK(!strcmp(Tcl_GetVar(i,"errorCode",0),"POSIX ENOENT {no such file or directory}"));
    Tcl_DString nativeCwd; CHECK(Tcl_GetCwd(i,&nativeCwd)&&nativeCwd.length>0&&nativeCwd.string[0]=='/');
    Tcl_DStringFree(&nativeCwd);
    CHECK(Tcl_GetHostName()!=NULL);
    Tcl_DString translated;
    CHECK(Tcl_TranslateFileName(i,"///tmp//",&translated)&&!strcmp(translated.string,"/tmp"));
    Tcl_DStringFree(&translated);
    CHECK(Tcl_Access("/",0)==0);
    struct stat nativeStat;
    CHECK(Tcl_Stat("/",&nativeStat)==0&&S_ISDIR(nativeStat.st_mode));
    Tcl_Obj *nativeRoot=Tcl_NewStringObj("/",1); CHECK(nativeRoot); Tcl_IncrRefCount(nativeRoot);
    Tcl_StatBuf *nativeMeta=Tcl_AllocStatBuf(); CHECK(nativeMeta);
    CHECK(Tcl_FSAccess(nativeRoot,0)==0&&Tcl_FSStat(nativeRoot,nativeMeta)==0&&S_ISDIR(nativeMeta->st_mode));
    CHECK(Tcl_FSLstat(nativeRoot,nativeMeta)==0);
    Tcl_Free((char *)nativeMeta); Tcl_DecrRefCount(nativeRoot);
    Tcl_Obj *invalidNative=Tcl_NewStringObj("/\0x",3); CHECK(invalidNative);
    Tcl_IncrRefCount(invalidNative); CHECK(Tcl_FSAccess(invalidNative,0)==-1&&Tcl_GetErrno()==EILSEQ);
    Tcl_DecrRefCount(invalidNative);
    Tcl_Obj *cwdObj=Tcl_FSGetCwd(i); CHECK(cwdObj&&cwdObj->refCount>=1);
    CHECK(Tcl_Chdir(Tcl_GetString(cwdObj))==0&&Tcl_FSChdir(cwdObj)==0);
    Tcl_DecrRefCount(cwdObj);
    char fsTemp[]="/tmp/tkgpu-ops-XXXXXX"; CHECK(mkdtemp(fsTemp));
    char fsDir[256],fsFile[256],fsRenamed[256];
    snprintf(fsDir,sizeof fsDir,"%s/dir",fsTemp);
    snprintf(fsFile,sizeof fsFile,"%s/file",fsTemp);
    snprintf(fsRenamed,sizeof fsRenamed,"%s/renamed",fsTemp);
    Tcl_Obj *dirObj=Tcl_NewStringObj(fsDir,-1),*fileObj=Tcl_NewStringObj(fsFile,-1),*renameObj=Tcl_NewStringObj(fsRenamed,-1);
    CHECK(dirObj&&fileObj&&renameObj);
    Tcl_IncrRefCount(dirObj); Tcl_IncrRefCount(fileObj); Tcl_IncrRefCount(renameObj);
    CHECK(Tcl_FSCreateDirectory(dirObj)==TCL_OK);
    FILE *nativeFile=fopen(fsFile,"wb"); CHECK(nativeFile&&fputs("test",nativeFile)>=0&&fclose(nativeFile)==0);
    CHECK(Tcl_FSRenameFile(fileObj,renameObj)==TCL_OK);
    struct utimbuf nativeTimes={123456789,123456789};
    CHECK(Tcl_FSUtime(renameObj,&nativeTimes)==0);
    CHECK(stat(fsRenamed,&nativeStat)==0&&nativeStat.st_mtime==123456789);
    CHECK(Tcl_FSDeleteFile(renameObj)==TCL_OK);
    CHECK(rmdir(fsDir)==0&&rmdir(fsTemp)==0);
    Tcl_DecrRefCount(dirObj); Tcl_DecrRefCount(fileObj); Tcl_DecrRefCount(renameObj);
    const char bracedScript[]="{a\\\n  b{c}}tail";
    Tcl_Parse braceParse;const char *braceTerm=NULL;
    CHECK(Tcl_ParseBraces(i,bracedScript,-1,&braceParse,0,&braceTerm)==TCL_OK);
    CHECK(braceTerm==strstr(bracedScript,"tail")&&braceParse.numTokens==3);
    CHECK(braceParse.tokenPtr[0].type==TCL_TOKEN_TEXT&&braceParse.tokenPtr[0].size==1);
    CHECK(braceParse.tokenPtr[1].type==TCL_TOKEN_BS&&braceParse.tokenPtr[1].size==4);
    CHECK(braceParse.tokenPtr[2].type==TCL_TOKEN_TEXT&&braceParse.tokenPtr[2].size==4);
    Tcl_FreeParse(&braceParse);
    CHECK(Tcl_ParseBraces(i,"{unfinished",-1,&braceParse,0,&braceTerm)==TCL_ERROR);
    CHECK(braceParse.errorType==TCL_PARSE_MISSING_BRACE&&braceParse.incomplete);
    CHECK(!strcmp(Tcl_GetStringResult(i),"missing close-brace"));
    Tcl_FreeParse(&braceParse);
    Tcl_Obj *startupObj=Tcl_NewStringObj("startup.tcl",-1); CHECK(startupObj);
    Tcl_IncrRefCount(startupObj);
    char startupEncoding[]="utf-8"; const char *storedEncoding=NULL;
    Tcl_SetStartupScript(startupObj,startupEncoding); startupEncoding[0]='X';
    CHECK(Tcl_GetStartupScript(&storedEncoding)==startupObj&&startupObj->refCount==2);
    CHECK(storedEncoding&&!strcmp(storedEncoding,"utf-8"));
    Tcl_SetStartupScript(startupObj,storedEncoding);
    CHECK(Tcl_GetStartupScript(&storedEncoding)==startupObj&&startupObj->refCount==2&&!strcmp(storedEncoding,"utf-8"));
    Tcl_SetStartupScript(NULL,"cp1251"); CHECK(startupObj->refCount==1&&!Tcl_GetStartupScript(&storedEncoding)&&!strcmp(storedEncoding,"cp1251"));
    Tcl_SetStartupScript(NULL,NULL); CHECK(!Tcl_GetStartupScript(&storedEncoding)&&!storedEncoding);
    Tcl_DecrRefCount(startupObj);
    char *preserved=Tcl_Alloc(8); CHECK(preserved); memcpy(preserved,"abc",4);
    Tcl_Preserve(preserved); Tcl_Preserve(preserved);
    Tcl_EventuallyFree(preserved,TCL_DYNAMIC); Tcl_Release(preserved);
    CHECK(!strcmp(preserved,"abc")); Tcl_Release(preserved);
    preserveCalls=0; Tcl_Preserve(&preserveToken); Tcl_EventuallyFree(&preserveToken,preserveCallback);
    CHECK(!preserveCalls); Tcl_Release(&preserveToken); CHECK(preserveCalls==2);
    int deletedState=0; Tcl_Interp *life=Tcl_CreateInterp(); CHECK(life&&!Tcl_InterpDeleted(life));
    Tcl_CallWhenDeleted(life,on_deleted_state,&deletedState);
    Tcl_Preserve(life); Tcl_DeleteInterp(life); CHECK(Tcl_InterpDeleted(life)&&!deletedState);
    Tcl_DeleteInterp(life); CHECK(Tcl_Eval(life,"set x 1")==TCL_ERROR);
    CHECK(!strcmp(Tcl_GetStringResult(life),"attempt to call eval in deleted interpreter"));
    Tcl_Release(life); CHECK(deletedState==1);
    deletedState=0; life=Tcl_CreateInterp(); CHECK(life);
    Tcl_CallWhenDeleted(life,on_deleted_state,&deletedState);
    CHECK(Tcl_CreateObjCommand(life,"delete_self",delete_during_eval,NULL,NULL));
    Tcl_Preserve(life);
    CHECK(Tcl_Eval(life,"delete_self; set should_not_run 1")==TCL_ERROR);
    CHECK(Tcl_InterpDeleted(life)&&!Tcl_GetVar(life,"should_not_run",0)&&!deletedState);
    Tcl_Release(life); CHECK(deletedState==1);
    int finalizerProbe=0; life=Tcl_CreateInterp(); CHECK(life);
    Tcl_CallWhenDeleted(life,on_finalizer_probe,&finalizerProbe);
    Tcl_DeleteInterp(life); CHECK(finalizerProbe==1);
    CHECK(Tcl_SetServiceMode(TCL_SERVICE_NONE)==TCL_SERVICE_NONE);
    queue_service_event(); CHECK(Tcl_ServiceAll()==0&&serviceCount==0);
    CHECK(Tcl_ServiceEvent(0)==1&&serviceCount==1&&serviceFlags==TCL_ALL_EVENTS&&serviceMode==TCL_SERVICE_NONE);
    CHECK(Tcl_SetServiceMode(TCL_SERVICE_ALL)==TCL_SERVICE_NONE);
    queue_service_event(); queue_service_event();
    CHECK(Tcl_ServiceAll()==1&&serviceCount==3&&serviceFlags==TCL_ALL_EVENTS&&serviceMode==TCL_SERVICE_NONE);
    CHECK(Tcl_GetServiceMode()==TCL_SERVICE_ALL&&Tcl_ServiceAll()==0);
    CHECK(Tcl_SetServiceMode(TCL_SERVICE_NONE)==TCL_SERVICE_ALL);
    char attrTemp[]="/tmp/tkgpu-attrs-XXXXXX"; int attrFd=mkstemp(attrTemp); CHECK(attrFd>=0); close(attrFd);
    Tcl_Obj *attrPath=Tcl_NewStringObj(attrTemp,-1); Tcl_IncrRefCount(attrPath);
    const char *const *attrNames=Tcl_FSFileAttrStrings(attrPath,NULL);
    CHECK(attrNames&&!strcmp(attrNames[0],"-group")&&!strcmp(attrNames[1],"-owner")&&!strcmp(attrNames[2],"-permissions")&&!attrNames[3]);
    Tcl_Obj *attrValue=Tcl_NewStringObj("0640",-1); Tcl_IncrRefCount(attrValue);
    CHECK(Tcl_FSFileAttrsSet(i,2,attrPath,attrValue)==TCL_OK); Tcl_DecrRefCount(attrValue);
    attrValue=NULL; CHECK(Tcl_FSFileAttrsGet(i,2,attrPath,&attrValue)==TCL_OK&&attrValue&&!strcmp(Tcl_GetString(attrValue),"00640")); Tcl_DecrRefCount(attrValue);
    attrValue=Tcl_NewStringObj("u+x,g-w",-1); Tcl_IncrRefCount(attrValue);
    CHECK(Tcl_FSFileAttrsSet(i,2,attrPath,attrValue)==TCL_OK); Tcl_DecrRefCount(attrValue);
    attrValue=NULL; CHECK(Tcl_FSFileAttrsGet(i,2,attrPath,&attrValue)==TCL_OK&&attrValue&&!strcmp(Tcl_GetString(attrValue),"00740")); Tcl_DecrRefCount(attrValue);
    CHECK(!unlink(attrTemp)); Tcl_DecrRefCount(attrPath);
    const char *infoData="prefix:";
    Tcl_Command infoString=Tcl_CreateCommand(i,"inspect_string",string_callback,(void *)infoData,NULL);
    Tcl_Command infoObject=Tcl_CreateObjCommand(i,"inspect_object",inspect_obj_callback,(void *)infoData,NULL);
    CHECK(infoString&&infoObject);
    CHECK(Tcl_FindCommand(i,"::::inspect_string",NULL,TCL_GLOBAL_ONLY)==infoString);
    CHECK(Tcl_FindCommand(i,"::inspect_object",NULL,0)==infoObject);
    Tcl_CmdInfo info;
    CHECK(Tcl_GetCommandInfo(i,"inspect_string",&info)&&!info.isNativeObjectProc&&info.proc==string_callback&&info.clientData==infoData&&info.objProc);
    Tcl_Obj *infoArgs[]={Tcl_NewStringObj("inspect_string",-1),Tcl_NewStringObj("value",-1)};
    Tcl_IncrRefCount(infoArgs[0]); Tcl_IncrRefCount(infoArgs[1]);
    Tcl_ResetResult(i); CHECK(info.objProc(info.objClientData,i,2,infoArgs)==TCL_OK&&!strcmp(Tcl_GetStringResult(i),"prefix:value"));
    Tcl_DecrRefCount(infoArgs[0]); Tcl_DecrRefCount(infoArgs[1]);
    CHECK(Tcl_GetCommandInfoFromToken(infoObject,&info)&&info.isNativeObjectProc&&info.objProc==inspect_obj_callback&&info.objClientData==infoData&&info.proc);
    const char *infoStringArgs[]={"inspect_object","value",NULL};
    Tcl_ResetResult(i); CHECK(info.proc(info.clientData,i,2,infoStringArgs)==TCL_OK&&!strcmp(Tcl_GetStringResult(i),"prefix:value"));
    CHECK(!Tcl_GetCommandInfo(i,"missing",&info)&&!Tcl_FindCommand(i,"missing",NULL,TCL_LEAVE_ERR_MSG));
    CHECK(!strcmp(Tcl_GetStringResult(i),"unknown command \"missing\""));
    int infoDeleted=0;
    CHECK(Tcl_GetCommandInfoFromToken(infoString,&info));
    info.objProc=inspect_obj_callback; info.objClientData=(void *)"object:";
    info.proc=string_callback; info.clientData=(void *)"string:";
    info.deleteProc=info_delete_callback; info.deleteData=&infoDeleted; info.isNativeObjectProc=0;
    CHECK(Tcl_SetCommandInfoFromToken(infoString,&info)&&!infoDeleted);
    CHECK(Tcl_GetCommandInfo(i,"inspect_string",&info)&&info.isNativeObjectProc&&info.deleteData==&infoDeleted);
    CHECK(Tcl_Eval(i,"inspect_string value")==TCL_OK&&!strcmp(Tcl_GetStringResult(i),"object:value"));
    info.objProc=NULL; info.objClientData=NULL;
    CHECK(Tcl_SetCommandInfo(i,"inspect_string",&info)&&!infoDeleted);
    CHECK(Tcl_GetCommandInfoFromToken(infoString,&info)&&!info.isNativeObjectProc&&info.objProc);
    CHECK(Tcl_Eval(i,"inspect_string value")==TCL_OK&&!strcmp(Tcl_GetStringResult(i),"string:value"));
    CHECK(!Tcl_SetCommandInfo(i,"missing",&info));
    CHECK(Tcl_DeleteCommandFromToken(i,infoString)==TCL_OK&&infoDeleted==1);
    CHECK(Tcl_DeleteCommandFromToken(i,infoObject)==TCL_OK);
    char linkTemp[]="/tmp/tkgpu-link-XXXXXX"; CHECK(mkdtemp(linkTemp));
    char linkSource[512],linkSym[512],linkHard[512];
    snprintf(linkSource,sizeof linkSource,"%s/source",linkTemp);
    snprintf(linkSym,sizeof linkSym,"%s/sym",linkTemp);
    snprintf(linkHard,sizeof linkHard,"%s/hard",linkTemp);
    FILE *linkFile=fopen(linkSource,"w"); CHECK(linkFile); CHECK(fclose(linkFile)==0);
    Tcl_Obj *linkTarget=Tcl_NewStringObj(linkSource,-1),*linkPath=Tcl_NewStringObj(linkSym,-1),*hardPath=Tcl_NewStringObj(linkHard,-1);
    Tcl_IncrRefCount(linkTarget); Tcl_IncrRefCount(linkPath); Tcl_IncrRefCount(hardPath);
    CHECK(Tcl_FSLink(linkPath,linkTarget,TCL_CREATE_SYMBOLIC_LINK)==linkTarget&&linkTarget->refCount==1);
    Tcl_Obj *readTarget=Tcl_FSLink(linkPath,NULL,0); CHECK(readTarget&&readTarget->refCount==1&&!strcmp(Tcl_GetString(readTarget),linkSource)); Tcl_DecrRefCount(readTarget);
    CHECK(Tcl_FSLink(hardPath,linkTarget,TCL_CREATE_HARD_LINK)==linkTarget&&linkTarget->refCount==1);
    struct stat linkA,linkB; CHECK(stat(linkSource,&linkA)==0&&stat(linkHard,&linkB)==0&&linkA.st_ino==linkB.st_ino);
    CHECK(!Tcl_FSLink(linkPath,linkTarget,TCL_CREATE_SYMBOLIC_LINK)&&errno==EEXIST);
    CHECK(unlink(linkSym)==0&&unlink(linkHard)==0&&unlink(linkSource)==0&&rmdir(linkTemp)==0);
    Tcl_DecrRefCount(linkTarget); Tcl_DecrRefCount(linkPath); Tcl_DecrRefCount(hardPath);
    Tcl_Obj *dictionary=Tcl_NewDictObj(),*dk=Tcl_NewStringObj("a b",-1),*dv=Tcl_NewStringObj("value",-1),*dgot=NULL;
    CHECK(dictionary&&dk&&dv);
    Tcl_IncrRefCount(dictionary); Tcl_IncrRefCount(dk); Tcl_IncrRefCount(dv);
    CHECK(dictionary->typePtr==Tcl_GetObjType("dict"));
    CHECK(Tcl_DictObjPut(i,dictionary,dk,dv)==TCL_OK&&dk->refCount==2&&dv->refCount==2);
    int dn=0; CHECK(Tcl_DictObjSize(i,dictionary,&dn)==TCL_OK&&dn==1);
    CHECK(Tcl_DictObjGet(i,dictionary,dk,&dgot)==TCL_OK&&dgot==dv);
    CHECK(!strcmp(Tcl_GetString(dictionary),"{a b} value"));
    Tcl_DictSearch dictSearch; Tcl_Obj *dkey=NULL; int ddone;
    CHECK(Tcl_DictObjFirst(i,dictionary,&dictSearch,&dkey,&dgot,&ddone)==TCL_OK&&!ddone&&dkey==dk&&dgot==dv);
    Tcl_DictObjNext(&dictSearch,&dkey,&dgot,&ddone); CHECK(ddone); Tcl_DictObjDone(&dictSearch);
    Tcl_Obj *dcopy=Tcl_DuplicateObj(dictionary); CHECK(dcopy); Tcl_IncrRefCount(dcopy);
    CHECK(Tcl_DictObjRemove(i,dcopy,dk)==TCL_OK&&Tcl_DictObjSize(i,dcopy,&dn)==TCL_OK&&dn==0);
    CHECK(Tcl_DictObjSize(i,dictionary,&dn)==TCL_OK&&dn==1);
    Tcl_DecrRefCount(dcopy);
    CHECK(Tcl_ListObjLength(i,dictionary,&dn)==TCL_OK&&dn==2);
    CHECK(dk->refCount==1&&dv->refCount==1);
    CHECK(Tcl_DictObjGet(i,dictionary,dk,&dgot)==TCL_OK&&dgot&&!strcmp(Tcl_GetString(dgot),"value"));
    CHECK(Tcl_DictObjRemove(i,dictionary,dk)==TCL_OK&&dk->refCount==1&&dv->refCount==1);
    Tcl_DecrRefCount(dictionary); Tcl_DecrRefCount(dk); Tcl_DecrRefCount(dv);
    {
        Tcl_Obj *nested=Tcl_NewDictObj(),*keys[]={Tcl_NewStringObj("outer",-1),Tcl_NewStringObj("inner",-1)},*value=Tcl_NewStringObj("data",-1);
        Tcl_IncrRefCount(nested); Tcl_IncrRefCount(keys[0]); Tcl_IncrRefCount(keys[1]); Tcl_IncrRefCount(value);
        CHECK(Tcl_DictObjPutKeyList(i,nested,2,keys,value)==TCL_OK);
        CHECK(!strcmp(Tcl_GetString(nested),"outer {inner data}"));
        Tcl_Obj *snapshot=Tcl_DuplicateObj(nested); Tcl_IncrRefCount(snapshot);
        CHECK(Tcl_DictObjRemoveKeyList(i,nested,2,keys)==TCL_OK);
        CHECK(!strcmp(Tcl_GetString(nested),"outer {}")&&!strcmp(Tcl_GetString(snapshot),"outer {inner data}"));
        Tcl_Obj *missing=Tcl_NewStringObj("absent",-1); Tcl_IncrRefCount(missing);
        Tcl_Obj *missingKeys[]={missing,keys[1]};
        CHECK(Tcl_DictObjRemoveKeyList(i,nested,2,missingKeys)==TCL_ERROR);
        CHECK(!strcmp(Tcl_GetStringResult(i),"key \"absent\" not known in dictionary"));
        Tcl_DecrRefCount(missing); Tcl_DecrRefCount(snapshot); Tcl_DecrRefCount(nested);
        Tcl_DecrRefCount(keys[0]); Tcl_DecrRefCount(keys[1]); Tcl_DecrRefCount(value);
    }
    {
        Tcl_AsyncHandler ah=Tcl_AsyncCreate(async_callback,(void *)"async result"); CHECK(ah&&!Tcl_AsyncReady());
        Tcl_AsyncMark(ah); Tcl_AsyncMark(ah); CHECK(Tcl_AsyncReady());
        CHECK(Tcl_AsyncInvoke(i,TCL_OK)==TCL_OK&&asyncCalls==1&&!Tcl_AsyncReady());
        CHECK(!strcmp(Tcl_GetStringResult(i),"async result"));
        Tcl_AsyncMark(ah); CHECK(Tcl_Eval(i,"set async_test 1")==TCL_OK&&asyncCalls==2);
        CHECK(!strcmp(Tcl_GetStringResult(i),"async result")&&!Tcl_AsyncReady());
        Tcl_AsyncMark(ah); Tcl_AsyncDelete(ah); CHECK(Tcl_AsyncReady());
        CHECK(Tcl_AsyncInvoke(NULL,TCL_ERROR)==TCL_OK&&!Tcl_AsyncReady());
    }
    {
        Tcl_Obj *msg=Tcl_NewStringObj("channel diagnostic",-1),*out=NULL;
        Tcl_IncrRefCount(msg);
        Tcl_SetChannelErrorInterp(i,msg); CHECK(msg->refCount==2);
        Tcl_SetChannelErrorInterp(i,msg); CHECK(msg->refCount==2);
        Tcl_GetChannelErrorInterp(i,&out); CHECK(out==msg&&msg->refCount==2);
        Tcl_DecrRefCount(out);
        Tcl_GetChannelErrorInterp(i,&out); CHECK(!out&&msg->refCount==1);
        Tcl_SetChannelErrorInterp(i,msg); Tcl_SetChannelErrorInterp(i,NULL); CHECK(msg->refCount==1);
        Tcl_DecrRefCount(msg);
        Tcl_SetVar(i,"errorInfo","old info",TCL_GLOBAL_ONLY);
        Tcl_SetVar(i,"errorCode","OLD CODE",TCL_GLOBAL_ONLY);
        Tcl_Obj *saved=Tcl_NewStringObj("saved object",-1); Tcl_IncrRefCount(saved); Tcl_SetObjResult(i,saved);
        Tcl_InterpState state=Tcl_SaveInterpState(i,TCL_BREAK);
        CHECK(state&&!strcmp(Tcl_GetStringResult(i),"saved object"));
        Tcl_SetVar(i,"errorInfo","new info",TCL_GLOBAL_ONLY);
        Tcl_SetVar(i,"errorCode","NEW CODE",TCL_GLOBAL_ONLY);
        Tcl_SetObjResult(i,Tcl_NewStringObj("different",-1));
        Tcl_SetErrorLine(i,42);
        CHECK(Tcl_RestoreInterpState(i,state)==TCL_BREAK&&Tcl_GetObjResult(i)==saved&&Tcl_GetErrorLine(i)==42);
        CHECK(!strcmp(Tcl_GetVar(i,"errorInfo",TCL_GLOBAL_ONLY),"old info"));
        CHECK(!strcmp(Tcl_GetVar(i,"errorCode",TCL_GLOBAL_ONLY),"OLD CODE"));
        Tcl_DecrRefCount(saved);
        state=Tcl_SaveInterpState(i,TCL_OK); CHECK(state);
        Tcl_SetObjResult(i,Tcl_NewStringObj("kept",-1)); Tcl_DiscardInterpState(state);
        CHECK(!strcmp(Tcl_GetStringResult(i),"kept"));
    }
    {
        Tcl_GetTimeProc *oldGet=NULL,*currentGet=NULL;
        Tcl_ScaleTimeProc *oldScale=NULL,*currentScale=NULL;
        ClientData oldTimeData=NULL,currentData=NULL;
        Tcl_QueryTimeProc(&oldGet,&oldScale,&oldTimeData); CHECK(oldGet&&oldScale);
        virtualTime.sec=123;virtualTime.usec=0;virtualFired=0;
        Tcl_SetTimeProc(virtual_get,virtual_scale,&virtualTime);
        Tcl_QueryTimeProc(&currentGet,&currentScale,&currentData);
        CHECK(currentGet==virtual_get&&currentScale==virtual_scale&&currentData==&virtualTime);
        Tcl_Time measured;Tcl_GetTime(&measured); CHECK(measured.sec==123&&measured.usec==0);
        Tcl_TimerToken vt=Tcl_CreateTimerHandler(50,virtual_timer,&virtualFired);CHECK(vt);
        CHECK(Tcl_DoOneEvent(TCL_TIMER_EVENTS|TCL_DONT_WAIT)==0&&!virtualFired);
        virtualTime.usec=50000;
        CHECK(Tcl_DoOneEvent(TCL_TIMER_EVENTS|TCL_DONT_WAIT)==1&&virtualFired==1);
        Tcl_SetTimeProc(oldGet,oldScale,oldTimeData);
    }
    {
        Tcl_Namespace *global=Tcl_GetGlobalNamespace(i); CHECK(global&&global==Tcl_GetCurrentNamespace(i));
        CHECK(!strcmp(global->name,"")&&!strcmp(global->fullName,"::")&&!global->parentPtr);
        CHECK(Tcl_FindNamespace(i,"::::",global,0)==global&&Tcl_FindNamespace(i,"",NULL,0)==global);
        CHECK(!Tcl_FindNamespace(i,"missing",global,TCL_LEAVE_ERR_MSG));
        CHECK(!strcmp(Tcl_GetStringResult(i),"unknown namespace \"missing\""));
        CHECK(Tcl_Export(i,global,"a*",1)==TCL_OK&&Tcl_Export(i,global,"b",0)==TCL_OK&&Tcl_Export(i,global,"a*",0)==TCL_OK);
        Tcl_Obj *ex=Tcl_NewObj();Tcl_IncrRefCount(ex);
        CHECK(Tcl_AppendExportList(i,global,ex)==TCL_OK&&!strcmp(Tcl_GetString(ex),"a* b"));
        Tcl_DecrRefCount(ex);
        CHECK(Tcl_Export(i,global,"::bad",1)==TCL_ERROR);
        ex=Tcl_NewObj();Tcl_IncrRefCount(ex);
        CHECK(Tcl_AppendExportList(i,global,ex)==TCL_OK&&!strcmp(Tcl_GetString(ex),""));
        Tcl_DecrRefCount(ex);
        Tcl_Command nc=Tcl_CreateCommand(i,"namespace_probe",string_callback,(void *)"ns:",NULL);CHECK(nc);
        Tcl_CmdInfo ni;CHECK(Tcl_GetCommandInfoFromToken(nc,&ni)&&ni.namespacePtr==global);
        CHECK(Tcl_FindCommand(i,"namespace_probe",global,0)==nc);CHECK(Tcl_DeleteCommandFromToken(i,nc)==TCL_OK);
    }
    {
        int pkgData=17,otherData=22; ClientData pkgOut=NULL;
        CHECK(Tcl_PkgProvideEx(i,"TKGPUProbe","1.02",&pkgData)==TCL_OK);
        CHECK(Tcl_PkgProvideEx(i,"TKGPUProbe","1.2.0",&otherData)==TCL_OK);
        CHECK(Tcl_PkgProvide(i,"TKGPUProbe","1.2")==TCL_OK);
        const char *pkgVersion=Tcl_PkgPresentEx(i,"TKGPUProbe","1.2-2",0,&pkgOut);
        CHECK(pkgVersion&&!strcmp(pkgVersion,"1.02")&&pkgOut==&otherData);
        CHECK(Tcl_PkgPresent(i,"TKGPUProbe","1.2.0",1)==pkgVersion);
        CHECK(Tcl_PkgProvide(i,"TKGPUProbe","2.0")==TCL_ERROR);
        CHECK(!Tcl_PkgPresent(i,"TKGPUProbe","2.0",0));
        CHECK(!strcmp(Tcl_PkgRequire(i,"TKGPUProbe","1",0),"1.02"));
        CHECK(!strcmp(tclStubsPtr->tcl_PkgRequire(i,"TKGPUProbe","1.2",1),"1.02"));
        Tcl_Obj *requirements[]={Tcl_NewStringObj("2",-1),Tcl_NewStringObj("1.2",-1)};
        Tcl_IncrRefCount(requirements[0]); Tcl_IncrRefCount(requirements[1]);
        pkgOut=NULL; CHECK(Tcl_PkgRequireProc(i,"TKGPUProbe",2,requirements,&pkgOut)==TCL_OK);
        CHECK(pkgOut==&otherData&&!strcmp(Tcl_GetStringResult(i),"1.02"));
        Tcl_DecrRefCount(requirements[0]); Tcl_DecrRefCount(requirements[1]);
        Tcl_Interp *isolated=Tcl_CreateInterp();CHECK(isolated);
        CHECK(!Tcl_PkgPresent(isolated,"TKGPUProbe",NULL,0));Tcl_DeleteInterp(isolated);
    }
    Tcl_ResetResult(i); Tcl_AppendElement(i,"a b"); Tcl_AppendElement(i,"#tag");
    CHECK(!strcmp(Tcl_GetString(Tcl_GetObjResult(i)),"{a b} #tag"));
    int destroyed=0;
    Tcl_SetAssocData(i,"stub.state",on_delete,&destroyed);
    CHECK(Tcl_GetAssocData(i,"stub.state",NULL)==&destroyed);
    Tcl_DeleteAssocData(i,"stub.state"); CHECK(destroyed==1);
    Tcl_CallWhenDeleted(i,on_delete,&destroyed);
    Tcl_CallWhenDeleted(i,on_delete,&destroyed);
    Tcl_DontCallWhenDeleted(i,on_delete,&destroyed);
    Tcl_ResetResult(i); Tcl_DeleteInterp(i); CHECK(destroyed==2);
    puts("Audited reference TclStubs subset passed"); return 0;
}
