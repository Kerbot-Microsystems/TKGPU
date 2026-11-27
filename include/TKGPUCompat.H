#ifndef TKGPU_COMPAT_H
#define TKGPU_COMPAT_H
/* Experimental source-level Tcl subset. Do not include with tcl.h or
   use as a binary replacement. Only the documented Tcl_Obj prefix is targeted; full ABI is not matched. */
#if defined(_WIN32)
# if defined(TKGPU_COMPAT_BUILD)
#  define TKGPU_COMPAT_API __declspec(dllexport)
# else
#  define TKGPU_COMPAT_API __declspec(dllimport)
# endif
#else
# define TKGPU_COMPAT_API __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef struct tg_interp Tcl_Interp;
#include "TKGPUAbiObject.H"
#include "TKGPUAbiBignum.H"
#include <stddef.h>
#include <stdarg.h>
enum { TKGPU_GLOBAL_ONLY=1, TKGPU_NAMESPACE_ONLY=2, TKGPU_APPEND_VALUE=4, TKGPU_LIST_ELEMENT=8, TKGPU_PARSE_PART1=0x400, TKGPU_LEAVE_ERR_MSG=0x200, TKGPU_NO_EVAL=0x010000, TKGPU_EVAL_GLOBAL=0x020000, TKGPU_EVAL_DIRECT=0x040000, TKGPU_CANCEL_UNWIND=0x100000 };
enum { TKGPU_ALPHA_RELEASE=0, TKGPU_BETA_RELEASE=1, TKGPU_FINAL_RELEASE=2 };
TKGPU_COMPAT_API void Tcl_GetVersion(int *,int *,int *,int *);
typedef void Tcl_ExitProc(void *);
TKGPU_COMPAT_API void Tcl_CreateExitHandler(Tcl_ExitProc *,void *);
TKGPU_COMPAT_API void Tcl_DeleteExitHandler(Tcl_ExitProc *,void *);
TKGPU_COMPAT_API void Tcl_CreateThreadExitHandler(Tcl_ExitProc *,void *);
TKGPU_COMPAT_API void Tcl_DeleteThreadExitHandler(Tcl_ExitProc *,void *);
TKGPU_COMPAT_API Tcl_ExitProc *Tcl_SetExitProc(Tcl_ExitProc *);
TKGPU_COMPAT_API void Tcl_FinalizeThread(void);
TKGPU_COMPAT_API void Tcl_Finalize(void);
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noreturn))
#elif defined(_MSC_VER)
__declspec(noreturn)
#endif
TKGPU_COMPAT_API void Tcl_Exit(int);
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noreturn))
#elif defined(_MSC_VER)
__declspec(noreturn)
#endif
TKGPU_COMPAT_API void Tcl_ExitThread(int);
typedef void Tcl_MainLoopProc(void);
TKGPU_COMPAT_API void Tcl_SetMainLoop(Tcl_MainLoopProc *);
TKGPU_COMPAT_API void Tcl_ServiceModeHook(int);
typedef struct TKGPUOpaqueWindow *Tk_Window;
TKGPU_COMPAT_API int Tk_Init(Tcl_Interp *);
TKGPU_COMPAT_API Tk_Window Tk_MainWindow(Tcl_Interp *);
TKGPU_COMPAT_API int Tk_GetNumMainWindows(void);
TKGPU_COMPAT_API void Tcl_FindExecutable(const char *);
TKGPU_COMPAT_API const char *Tcl_GetNameOfExecutable(void);
TKGPU_COMPAT_API int Tcl_Init(Tcl_Interp *);
TKGPU_COMPAT_API int Tcl_RecordAndEval(Tcl_Interp *,const char *,int);
TKGPU_COMPAT_API int Tcl_RecordAndEvalObj(Tcl_Interp *,Tcl_Obj *,int);
TKGPU_COMPAT_API Tcl_Interp *Tcl_CreateInterp(void);
/* Explicit audited-extension opt-in; never automatic. Unsupported slots stay
   null. The caller is responsible for a complete, trusted slot manifest. */
TKGPU_COMPAT_API int TKGPU_EnableReferenceStubs(Tcl_Interp *,const char *const *,size_t);
TKGPU_COMPAT_API void Tcl_DeleteInterp(Tcl_Interp *interp);
TKGPU_COMPAT_API int Tcl_InterpDeleted(Tcl_Interp *interp);
TKGPU_COMPAT_API int Tcl_InterpActive(Tcl_Interp *interp);
TKGPU_COMPAT_API int Tcl_SetRecursionLimit(Tcl_Interp *,int);
typedef struct Tcl_LoadHandle_ *Tcl_LoadHandle;
typedef int Tcl_PackageInitProc(Tcl_Interp *);
typedef void Tcl_FSUnloadFileProc(Tcl_LoadHandle);
enum { TKGPU_LOAD_GLOBAL=1, TKGPU_LOAD_LAZY=2 };
TKGPU_COMPAT_API int Tcl_LoadFile(Tcl_Interp *,Tcl_Obj *,const char *const [],int,void *,Tcl_LoadHandle *);
TKGPU_COMPAT_API void *Tcl_FindSymbol(Tcl_Interp *,Tcl_LoadHandle,const char *);
TKGPU_COMPAT_API int Tcl_FSUnloadFile(Tcl_Interp *,Tcl_LoadHandle);
TKGPU_COMPAT_API int Tcl_FSLoadFile(Tcl_Interp *,Tcl_Obj *,const char *,const char *,Tcl_PackageInitProc **,Tcl_PackageInitProc **,Tcl_LoadHandle *,Tcl_FSUnloadFileProc **);

typedef void Tcl_InterpDeleteProc(void *,Tcl_Interp *);
TKGPU_COMPAT_API void Tcl_SetAssocData(Tcl_Interp *,const char *,Tcl_InterpDeleteProc *,void *);
TKGPU_COMPAT_API void *Tcl_GetAssocData(Tcl_Interp *,const char *,Tcl_InterpDeleteProc **);
TKGPU_COMPAT_API void Tcl_DeleteAssocData(Tcl_Interp *,const char *);
TKGPU_COMPAT_API void Tcl_CallWhenDeleted(Tcl_Interp *,Tcl_InterpDeleteProc *,void *);
TKGPU_COMPAT_API void Tcl_DontCallWhenDeleted(Tcl_Interp *,Tcl_InterpDeleteProc *,void *);
TKGPU_COMPAT_API int Tcl_CommandComplete(const char *);
TKGPU_COMPAT_API int Tcl_Eval(Tcl_Interp *interp,const char *script);
TKGPU_COMPAT_API const char *Tcl_GetStringResult(Tcl_Interp *interp);
TKGPU_COMPAT_API void Tcl_ResetResult(Tcl_Interp *interp);
typedef void Tcl_FreeProc(char *);
TKGPU_COMPAT_API void Tcl_SetResult(Tcl_Interp *,char *,Tcl_FreeProc *);
TKGPU_COMPAT_API void Tcl_FreeResult(Tcl_Interp *);
TKGPU_COMPAT_API void Tcl_TransferResult(Tcl_Interp *,int,Tcl_Interp *);
TKGPU_COMPAT_API const char *Tcl_GetVar2(Tcl_Interp *,const char *,const char *,int);
TKGPU_COMPAT_API const char *Tcl_SetVar2(Tcl_Interp *,const char *,const char *,const char *,int);
TKGPU_COMPAT_API int Tcl_UnsetVar2(Tcl_Interp *,const char *,const char *,int);
TKGPU_COMPAT_API int Tcl_ExprDouble(Tcl_Interp *,const char *,double *);
TKGPU_COMPAT_API int Tcl_ExprLong(Tcl_Interp *,const char *,long *);
TKGPU_COMPAT_API int Tcl_ExprBoolean(Tcl_Interp *,const char *,int *);
TKGPU_COMPAT_API int Tcl_ExprString(Tcl_Interp *,const char *);
TKGPU_COMPAT_API const char *Tcl_GetVar(Tcl_Interp *,const char *,int);
TKGPU_COMPAT_API const char *Tcl_SetVar(Tcl_Interp *,const char *,const char *,int);
TKGPU_COMPAT_API int Tcl_UnsetVar(Tcl_Interp *,const char *,int);
TKGPU_COMPAT_API Tcl_Obj *Tcl_ObjGetVar2(Tcl_Interp *,Tcl_Obj *,Tcl_Obj *,int);
TKGPU_COMPAT_API Tcl_Obj *Tcl_ObjSetVar2(Tcl_Interp *,Tcl_Obj *,Tcl_Obj *,Tcl_Obj *,int);
typedef char *Tcl_VarTraceProc(void *,Tcl_Interp *,const char *,const char *,int);
typedef void Tcl_CommandTraceProc(void *,Tcl_Interp *,const char *,const char *,int);
enum { TKGPU_TRACE_READS=0x10, TKGPU_TRACE_WRITES=0x20, TKGPU_TRACE_UNSETS=0x40, TKGPU_TRACE_RENAME=0x2000, TKGPU_TRACE_DELETE=0x4000 };
enum { TKGPU_LINK_INT=1, TKGPU_LINK_DOUBLE=2, TKGPU_LINK_BOOLEAN=3, TKGPU_LINK_STRING=4, TKGPU_LINK_WIDE_INT=5, TKGPU_LINK_READ_ONLY=0x80 };
TKGPU_COMPAT_API int Tcl_TraceVar(Tcl_Interp *,const char *,int,Tcl_VarTraceProc *,void *);
TKGPU_COMPAT_API int Tcl_TraceVar2(Tcl_Interp *,const char *,const char *,int,Tcl_VarTraceProc *,void *);
TKGPU_COMPAT_API void Tcl_UntraceVar(Tcl_Interp *,const char *,int,Tcl_VarTraceProc *,void *);
TKGPU_COMPAT_API void Tcl_UntraceVar2(Tcl_Interp *,const char *,const char *,int,Tcl_VarTraceProc *,void *);
TKGPU_COMPAT_API void *Tcl_VarTraceInfo(Tcl_Interp *,const char *,int,Tcl_VarTraceProc *,void *);
TKGPU_COMPAT_API void *Tcl_VarTraceInfo2(Tcl_Interp *,const char *,const char *,int,Tcl_VarTraceProc *,void *);
TKGPU_COMPAT_API int Tcl_TraceCommand(Tcl_Interp *,const char *,int,Tcl_CommandTraceProc *,void *);
TKGPU_COMPAT_API void Tcl_UntraceCommand(Tcl_Interp *,const char *,int,Tcl_CommandTraceProc *,void *);
TKGPU_COMPAT_API void *Tcl_CommandTraceInfo(Tcl_Interp *,const char *,int,Tcl_CommandTraceProc *,void *);
TKGPU_COMPAT_API int Tcl_LinkVar(Tcl_Interp *,const char *,char *,int);
TKGPU_COMPAT_API void Tcl_UnlinkVar(Tcl_Interp *,const char *);
TKGPU_COMPAT_API void Tcl_UpdateLinkedVar(Tcl_Interp *,const char *);
TKGPU_COMPAT_API int Tcl_UpVar(Tcl_Interp *,const char *,const char *,const char *,int);
TKGPU_COMPAT_API int Tcl_UpVar2(Tcl_Interp *,const char *,const char *,const char *,const char *,int);
TKGPU_COMPAT_API int Tcl_ProcObjCmd(void *,Tcl_Interp *,int,Tcl_Obj *const []);
TKGPU_COMPAT_API int Tcl_ExprLongObj(Tcl_Interp *,Tcl_Obj *,long *);
TKGPU_COMPAT_API int Tcl_ExprDoubleObj(Tcl_Interp *,Tcl_Obj *,double *);
TKGPU_COMPAT_API int Tcl_ExprBooleanObj(Tcl_Interp *,Tcl_Obj *,int *);
TKGPU_COMPAT_API int Tcl_ExprObj(Tcl_Interp *,Tcl_Obj *,Tcl_Obj **);
TKGPU_COMPAT_API int Tcl_AppendAllObjTypes(Tcl_Interp *,Tcl_Obj *);
TKGPU_COMPAT_API char *Tcl_AttemptAlloc(unsigned int);
TKGPU_COMPAT_API char *Tcl_AttemptRealloc(char *,unsigned int);
TKGPU_COMPAT_API char *Tcl_Alloc(unsigned int);
TKGPU_COMPAT_API char *Tcl_Realloc(char *,unsigned int);
TKGPU_COMPAT_API void Tcl_Free(char *);
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noreturn,format(printf,1,2)))
#elif defined(_MSC_VER)
__declspec(noreturn)
#endif
TKGPU_COMPAT_API void Tcl_Panic(const char *,...);
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noreturn,format(printf,1,0)))
#elif defined(_MSC_VER)
__declspec(noreturn)
#endif
TKGPU_COMPAT_API void Tcl_PanicVA(const char *,va_list);
typedef void Tcl_PanicProc(const char *,...);
TKGPU_COMPAT_API void Tcl_SetPanicProc(Tcl_PanicProc *);
TKGPU_COMPAT_API void Tcl_AppendObjToObj(Tcl_Obj *,Tcl_Obj *);
TKGPU_COMPAT_API int Tcl_EvalEx(Tcl_Interp *,const char *,int,int);
TKGPU_COMPAT_API int Tcl_EvalObjEx(Tcl_Interp *,Tcl_Obj *,int);
TKGPU_COMPAT_API int Tcl_EvalObj(Tcl_Interp *,Tcl_Obj *);
TKGPU_COMPAT_API int Tcl_GlobalEval(Tcl_Interp *,const char *);
TKGPU_COMPAT_API int Tcl_GlobalEvalObj(Tcl_Interp *,Tcl_Obj *);
TKGPU_COMPAT_API int Tcl_VarEval(Tcl_Interp *,...);
TKGPU_COMPAT_API int Tcl_VarEvalVA(Tcl_Interp *,va_list);
typedef struct Tcl_DString {
    char *string; int length,spaceAvl; char staticSpace[200];
} Tcl_DString;
#define Tcl_DStringValue(d) ((d)->string)
#define Tcl_DStringLength(d) ((d)->length)
#define Tcl_DStringTrunc Tcl_DStringSetLength
TKGPU_COMPAT_API void Tcl_DStringInit(Tcl_DString *);
TKGPU_COMPAT_API void Tcl_DStringFree(Tcl_DString *);
TKGPU_COMPAT_API char *Tcl_DStringAppend(Tcl_DString *,const char *,int);
TKGPU_COMPAT_API char *Tcl_DStringAppendElement(Tcl_DString *,const char *);
TKGPU_COMPAT_API void Tcl_DStringSetLength(Tcl_DString *,int);
TKGPU_COMPAT_API void Tcl_DStringStartSublist(Tcl_DString *);
TKGPU_COMPAT_API void Tcl_DStringEndSublist(Tcl_DString *);
TKGPU_COMPAT_API void Tcl_DStringGetResult(Tcl_Interp *,Tcl_DString *);
TKGPU_COMPAT_API void Tcl_DStringResult(Tcl_Interp *,Tcl_DString *);
TKGPU_COMPAT_API int Tcl_SplitList(Tcl_Interp *,const char *,int *,const char ***);
TKGPU_COMPAT_API int Tcl_ScanElement(const char *,int *);
TKGPU_COMPAT_API int Tcl_ScanCountedElement(const char *,int,int *);
TKGPU_COMPAT_API int Tcl_ConvertElement(const char *,char *,int);
TKGPU_COMPAT_API int Tcl_ConvertCountedElement(const char *,int,char *,int);
TKGPU_COMPAT_API char *Tcl_Merge(int,const char *const []);
TKGPU_COMPAT_API char *Tcl_Concat(int,const char *const []);
TKGPU_COMPAT_API Tcl_Obj *Tcl_ConcatObj(int,Tcl_Obj *const []);
TKGPU_COMPAT_API int Tcl_GetInt(Tcl_Interp *,const char *,int *);
enum { TKGPU_EXACT=1 };
TKGPU_COMPAT_API int Tcl_GetIndexFromObj(Tcl_Interp *,Tcl_Obj *,const char *const [],const char *,int,int *);
TKGPU_COMPAT_API int Tcl_GetIndexFromObjStruct(Tcl_Interp *,Tcl_Obj *,const void *,int,const char *,int,int *);
TKGPU_COMPAT_API int Tcl_GetBoolean(Tcl_Interp *,const char *,int *);
/* Tcl-named constructors return zero-reference preview objects. */
TKGPU_COMPAT_API void TclFreeObj(Tcl_Obj *);
TKGPU_COMPAT_API Tcl_Obj *Tcl_NewStringObj(const char *,int);
TKGPU_COMPAT_API Tcl_Obj *Tcl_NewObj(void);
TKGPU_COMPAT_API void Tcl_SetStringObj(Tcl_Obj *,const char *,int);
TKGPU_COMPAT_API void Tcl_AppendToObj(Tcl_Obj *,const char *,int);
TKGPU_COMPAT_API void Tcl_AppendLimitedToObj(Tcl_Obj *,const char *,int,int,const char *);
TKGPU_COMPAT_API void Tcl_AppendStringsToObj(Tcl_Obj *,...);
TKGPU_COMPAT_API void Tcl_AppendStringsToObjVA(Tcl_Obj *,va_list);
TKGPU_COMPAT_API void Tcl_SetIntObj(Tcl_Obj *,int);
TKGPU_COMPAT_API void Tcl_SetLongObj(Tcl_Obj *,long);
TKGPU_COMPAT_API void Tcl_SetWideIntObj(Tcl_Obj *,long long);
TKGPU_COMPAT_API void Tcl_SetDoubleObj(Tcl_Obj *,double);
TKGPU_COMPAT_API void Tcl_SetByteArrayObj(Tcl_Obj *,const unsigned char *,int);
TKGPU_COMPAT_API void Tcl_InvalidateStringRep(Tcl_Obj *);
TKGPU_COMPAT_API int Tcl_AttemptSetObjLength(Tcl_Obj *,int);
TKGPU_COMPAT_API void Tcl_SetObjLength(Tcl_Obj *,int);
TKGPU_COMPAT_API unsigned char *Tcl_SetByteArrayLength(Tcl_Obj *,int);
typedef uint16_t Tcl_UniChar;
TKGPU_COMPAT_API int Tcl_UtfCharComplete(const char *,int);
TKGPU_COMPAT_API int Tcl_UniCharIsAlnum(int);
TKGPU_COMPAT_API int Tcl_UtfToLower(char *);
TKGPU_COMPAT_API int Tcl_UtfToUpper(char *);
TKGPU_COMPAT_API int Tcl_UtfToTitle(char *);
TKGPU_COMPAT_API int Tcl_UtfNcasecmp(const char *,const char *,unsigned long);
TKGPU_COMPAT_API int Tcl_UniCharCaseMatch(const Tcl_UniChar *,const Tcl_UniChar *,int);
TKGPU_COMPAT_API Tcl_UniChar Tcl_UniCharToLower(int);
TKGPU_COMPAT_API Tcl_UniChar Tcl_UniCharToUpper(int);
TKGPU_COMPAT_API Tcl_UniChar Tcl_UniCharToTitle(int);
TKGPU_COMPAT_API int Tcl_UniCharNcasecmp(const Tcl_UniChar *,const Tcl_UniChar *,unsigned long);
TKGPU_COMPAT_API int Tcl_GetErrno(void);
TKGPU_COMPAT_API unsigned int Tcl_ZlibCRC32(unsigned int,const unsigned char *,int);
TKGPU_COMPAT_API unsigned int Tcl_ZlibAdler32(unsigned int,const unsigned char *,int);
typedef struct Tcl_RegExp_ *Tcl_RegExp;
typedef struct Tcl_RegExpIndices { long start; long end; } Tcl_RegExpIndices;
typedef struct Tcl_RegExpInfo { int nsubs; Tcl_RegExpIndices *matches; long extendStart; long reserved; } Tcl_RegExpInfo;
TKGPU_COMPAT_API Tcl_RegExp Tcl_RegExpCompile(Tcl_Interp *,const char *);
TKGPU_COMPAT_API int Tcl_RegExpExec(Tcl_Interp *,Tcl_RegExp,const char *,const char *);
TKGPU_COMPAT_API int Tcl_RegExpMatch(Tcl_Interp *,const char *,const char *);
TKGPU_COMPAT_API void Tcl_RegExpRange(Tcl_RegExp,int,const char **,const char **);
TKGPU_COMPAT_API Tcl_RegExp Tcl_GetRegExpFromObj(Tcl_Interp *,Tcl_Obj *,int);
TKGPU_COMPAT_API int Tcl_RegExpExecObj(Tcl_Interp *,Tcl_RegExp,Tcl_Obj *,int,int,int);
TKGPU_COMPAT_API int Tcl_RegExpMatchObj(Tcl_Interp *,Tcl_Obj *,Tcl_Obj *);
TKGPU_COMPAT_API void Tcl_RegExpGetInfo(Tcl_RegExp,Tcl_RegExpInfo *);
typedef struct Tcl_ZlibStream_ *Tcl_ZlibStream;
TKGPU_COMPAT_API int Tcl_ZlibDeflate(Tcl_Interp *,int,Tcl_Obj *,int,Tcl_Obj *);
TKGPU_COMPAT_API int Tcl_ZlibInflate(Tcl_Interp *,int,Tcl_Obj *,int,Tcl_Obj *);
TKGPU_COMPAT_API int Tcl_ZlibStreamInit(Tcl_Interp *,int,int,int,Tcl_Obj *,Tcl_ZlibStream *);
TKGPU_COMPAT_API int Tcl_ZlibStreamClose(Tcl_ZlibStream);
TKGPU_COMPAT_API int Tcl_ZlibStreamReset(Tcl_ZlibStream);
TKGPU_COMPAT_API Tcl_Obj *Tcl_ZlibStreamGetCommandName(Tcl_ZlibStream);
TKGPU_COMPAT_API int Tcl_ZlibStreamEof(Tcl_ZlibStream);
TKGPU_COMPAT_API int Tcl_ZlibStreamChecksum(Tcl_ZlibStream);
TKGPU_COMPAT_API int Tcl_ZlibStreamPut(Tcl_ZlibStream,Tcl_Obj *,int);
TKGPU_COMPAT_API int Tcl_ZlibStreamGet(Tcl_ZlibStream,Tcl_Obj *,int);
TKGPU_COMPAT_API void Tcl_ZlibStreamSetCompressionDictionary(Tcl_ZlibStream,Tcl_Obj *);
TKGPU_COMPAT_API void Tcl_SetErrno(int);
TKGPU_COMPAT_API int Tcl_UniCharIsAlpha(int);
TKGPU_COMPAT_API int Tcl_UniCharIsControl(int);
TKGPU_COMPAT_API int Tcl_UniCharIsDigit(int);
TKGPU_COMPAT_API int Tcl_UniCharIsGraph(int);
TKGPU_COMPAT_API int Tcl_UniCharIsLower(int);
TKGPU_COMPAT_API int Tcl_UniCharIsPrint(int);
TKGPU_COMPAT_API int Tcl_UniCharIsPunct(int);
TKGPU_COMPAT_API int Tcl_UniCharIsSpace(int);
TKGPU_COMPAT_API int Tcl_UniCharIsUpper(int);
TKGPU_COMPAT_API int Tcl_UniCharIsWordChar(int);
TKGPU_COMPAT_API int Tcl_NumUtfChars(const char *,int);
TKGPU_COMPAT_API const char *Tcl_UtfFindFirst(const char *,int);
TKGPU_COMPAT_API const char *Tcl_UtfFindLast(const char *,int);
TKGPU_COMPAT_API const char *Tcl_UtfAtIndex(const char *,int);
TKGPU_COMPAT_API Tcl_UniChar Tcl_UniCharAtIndex(const char *,int);
TKGPU_COMPAT_API const char *Tcl_UtfNext(const char *);
TKGPU_COMPAT_API const char *Tcl_UtfPrev(const char *,const char *);
TKGPU_COMPAT_API int Tcl_UtfToUniChar(const char *,Tcl_UniChar *);
TKGPU_COMPAT_API int Tcl_UtfNcmp(const char *,const char *,unsigned long);
TKGPU_COMPAT_API int Tcl_StringMatch(const char *,const char *);
TKGPU_COMPAT_API int Tcl_StringCaseMatch(const char *,const char *,int);
TKGPU_COMPAT_API int Tcl_UtfBackslash(const char *,int *,char *);
TKGPU_COMPAT_API char Tcl_Backslash(const char *,int *);
TKGPU_COMPAT_API Tcl_UniChar *Tcl_UtfToUniCharDString(const char *,int,Tcl_DString *);
TKGPU_COMPAT_API Tcl_UniChar *Tcl_GetUnicode(Tcl_Obj *);
TKGPU_COMPAT_API int Tcl_UniCharToUtf(int,char *);
TKGPU_COMPAT_API int Tcl_UniCharLen(const Tcl_UniChar *);
TKGPU_COMPAT_API int Tcl_UniCharNcmp(const Tcl_UniChar *,const Tcl_UniChar *,unsigned long);
TKGPU_COMPAT_API char *Tcl_UniCharToUtfDString(const Tcl_UniChar *,int,Tcl_DString *);
TKGPU_COMPAT_API Tcl_Obj *Tcl_NewUnicodeObj(const Tcl_UniChar *,int);
TKGPU_COMPAT_API Tcl_UniChar *Tcl_GetUnicodeFromObj(Tcl_Obj *,int *);
TKGPU_COMPAT_API char *Tcl_GetStringFromObj(Tcl_Obj *,int *);
TKGPU_COMPAT_API char *Tcl_GetString(Tcl_Obj *);
TKGPU_COMPAT_API Tcl_Obj *Tcl_NewIntObj(int);
TKGPU_COMPAT_API Tcl_Obj *Tcl_NewLongObj(long);
TKGPU_COMPAT_API Tcl_Obj *Tcl_NewWideIntObj(long long);
TKGPU_COMPAT_API int Tcl_GetWideIntFromObj(Tcl_Interp *,Tcl_Obj *,long long *);
TKGPU_COMPAT_API int Tcl_GetIntFromObj(Tcl_Interp *,Tcl_Obj *,int *);
TKGPU_COMPAT_API Tcl_Obj *Tcl_GetObjResult(Tcl_Interp *);
TKGPU_COMPAT_API void Tcl_SetObjResult(Tcl_Interp *,Tcl_Obj *);
TKGPU_COMPAT_API void Tcl_SetChannelErrorInterp(Tcl_Interp *,Tcl_Obj *);
TKGPU_COMPAT_API void Tcl_GetChannelErrorInterp(Tcl_Interp *,Tcl_Obj **);
/* Bounded preview: result and facade diagnostic variables, not full return options. */
typedef struct Tcl_InterpState_ *Tcl_InterpState;
TKGPU_COMPAT_API Tcl_InterpState Tcl_SaveInterpState(Tcl_Interp *,int);
TKGPU_COMPAT_API int Tcl_RestoreInterpState(Tcl_Interp *,Tcl_InterpState);
TKGPU_COMPAT_API void Tcl_DiscardInterpState(Tcl_InterpState);
TKGPU_COMPAT_API Tcl_Obj *Tcl_GetVar2Ex(Tcl_Interp *,const char *,const char *,int);
TKGPU_COMPAT_API Tcl_Obj *Tcl_SetVar2Ex(Tcl_Interp *,const char *,const char *,Tcl_Obj *,int);
TKGPU_COMPAT_API int Tcl_GetBooleanFromObj(Tcl_Interp *,Tcl_Obj *,int *);
TKGPU_COMPAT_API Tcl_Obj *Tcl_NewByteArrayObj(const unsigned char *,int);
TKGPU_COMPAT_API unsigned char *Tcl_GetByteArrayFromObj(Tcl_Obj *,int *);
TKGPU_COMPAT_API Tcl_Obj *Tcl_NewListObj(int,Tcl_Obj *const []);
TKGPU_COMPAT_API int Tcl_ListObjGetElements(Tcl_Interp *,Tcl_Obj *,int *,Tcl_Obj ***);
TKGPU_COMPAT_API int Tcl_ListObjLength(Tcl_Interp *,Tcl_Obj *,int *);
TKGPU_COMPAT_API int Tcl_ListObjAppendElement(Tcl_Interp *,Tcl_Obj *,Tcl_Obj *);
TKGPU_COMPAT_API int Tcl_ListObjReplace(Tcl_Interp *,Tcl_Obj *,int,int,int,Tcl_Obj *const []);
TKGPU_COMPAT_API int Tcl_ListObjAppendList(Tcl_Interp *,Tcl_Obj *,Tcl_Obj *);
TKGPU_COMPAT_API int Tcl_ListObjIndex(Tcl_Interp *,Tcl_Obj *,int,Tcl_Obj **);
TKGPU_COMPAT_API const Tcl_ObjType *Tcl_GetObjType(const char *);
TKGPU_COMPAT_API void Tcl_RegisterObjType(const Tcl_ObjType *);
TKGPU_COMPAT_API int Tcl_ConvertToType(Tcl_Interp *,Tcl_Obj *,const Tcl_ObjType *);
typedef struct Tcl_DictSearch {
    void *next;
    int epoch;
    void *dictionaryPtr;
} Tcl_DictSearch;
TKGPU_COMPAT_API Tcl_Obj *Tcl_NewDictObj(void);
TKGPU_COMPAT_API Tcl_Obj *Tcl_DbNewDictObj(const char *,int);
TKGPU_COMPAT_API int Tcl_DictObjPut(Tcl_Interp *,Tcl_Obj *,Tcl_Obj *,Tcl_Obj *);
TKGPU_COMPAT_API int Tcl_DictObjGet(Tcl_Interp *,Tcl_Obj *,Tcl_Obj *,Tcl_Obj **);
TKGPU_COMPAT_API int Tcl_DictObjRemove(Tcl_Interp *,Tcl_Obj *,Tcl_Obj *);
TKGPU_COMPAT_API int Tcl_DictObjSize(Tcl_Interp *,Tcl_Obj *,int *);
TKGPU_COMPAT_API int Tcl_DictObjFirst(Tcl_Interp *,Tcl_Obj *,Tcl_DictSearch *,Tcl_Obj **,Tcl_Obj **,int *);
TKGPU_COMPAT_API void Tcl_DictObjNext(Tcl_DictSearch *,Tcl_Obj **,Tcl_Obj **,int *);
TKGPU_COMPAT_API void Tcl_DictObjDone(Tcl_DictSearch *);
TKGPU_COMPAT_API int Tcl_DictObjPutKeyList(Tcl_Interp *,Tcl_Obj *,int,Tcl_Obj *const [],Tcl_Obj *);
TKGPU_COMPAT_API int Tcl_DictObjRemoveKeyList(Tcl_Interp *,Tcl_Obj *,int,Tcl_Obj *const []);
TKGPU_COMPAT_API Tcl_Obj *Tcl_DuplicateObj(Tcl_Obj *);
TKGPU_COMPAT_API Tcl_Obj *Tcl_NewDoubleObj(double);
TKGPU_COMPAT_API int Tcl_GetDouble(Tcl_Interp *,const char *,double *);
TKGPU_COMPAT_API int Tcl_GetDoubleFromObj(Tcl_Interp *,Tcl_Obj *,double *);
TKGPU_COMPAT_API void Tcl_PrintDouble(Tcl_Interp *,double,char *);
#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf,1,2)))
#endif
TKGPU_COMPAT_API Tcl_Obj *Tcl_ObjPrintf(const char *,...);
#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf,2,3)))
#endif
TKGPU_COMPAT_API void Tcl_AppendPrintfToObj(Tcl_Obj *,const char *,...);
TKGPU_COMPAT_API int Tcl_CancelEval(Tcl_Interp *,Tcl_Obj *,void *,int);
TKGPU_COMPAT_API int Tcl_Canceled(Tcl_Interp *,int);
TKGPU_COMPAT_API Tcl_Obj *Tcl_GetReturnOptions(Tcl_Interp *,int);
TKGPU_COMPAT_API int Tcl_SetReturnOptions(Tcl_Interp *,Tcl_Obj *);
TKGPU_COMPAT_API int Tcl_AppendFormatToObj(Tcl_Interp *,Tcl_Obj *,const char *,int,Tcl_Obj *const []);
TKGPU_COMPAT_API Tcl_Obj *Tcl_Format(Tcl_Interp *,const char *,int,Tcl_Obj *const []);
TKGPU_COMPAT_API void Tcl_AllowExceptions(Tcl_Interp *);
TKGPU_COMPAT_API void Tcl_BackgroundError(Tcl_Interp *);
TKGPU_COMPAT_API void Tcl_BackgroundException(Tcl_Interp *,int);
TKGPU_COMPAT_API void Tcl_LogCommandInfo(Tcl_Interp *,const char *,const char *,int);
TKGPU_COMPAT_API int Tcl_PutEnv(const char *);
typedef struct Tcl_Config { const char *key; const char *value; } Tcl_Config;
TKGPU_COMPAT_API void Tcl_RegisterConfig(Tcl_Interp *,const char *,const Tcl_Config *,const char *);
TKGPU_COMPAT_API void Tcl_SourceRCFile(Tcl_Interp *);
TKGPU_COMPAT_API void Tcl_StaticPackage(Tcl_Interp *,const char *,Tcl_PackageInitProc *,Tcl_PackageInitProc *);
TKGPU_COMPAT_API void Tcl_InitMemory(Tcl_Interp *);
TKGPU_COMPAT_API int Tcl_DumpActiveMemory(const char *);
TKGPU_COMPAT_API void Tcl_ValidateAllMemory(const char *,int);
/* Timer and posted event subset. Tokens are opaque and thread-affine. */
typedef struct TKGPUOpaqueTimer *Tcl_TimerToken;
typedef void Tcl_TimerProc(void *);
typedef void Tcl_IdleProc(void *);
TKGPU_COMPAT_API void Tcl_DoWhenIdle(Tcl_IdleProc *,void *);
TKGPU_COMPAT_API void Tcl_CancelIdleCall(Tcl_IdleProc *,void *);
enum { TKGPU_DONT_WAIT=2, TKGPU_WINDOW_EVENTS=4, TKGPU_FILE_EVENTS=8,
       TKGPU_TIMER_EVENTS=16, TKGPU_IDLE_EVENTS=32 };
TKGPU_COMPAT_API Tcl_TimerToken Tcl_CreateTimerHandler(int,Tcl_TimerProc *,void *);
TKGPU_COMPAT_API void Tcl_DeleteTimerHandler(Tcl_TimerToken);
TKGPU_COMPAT_API int Tcl_DoOneEvent(int);
TKGPU_COMPAT_API int Tcl_GetServiceMode(void);
TKGPU_COMPAT_API int Tcl_SetServiceMode(int);
TKGPU_COMPAT_API int Tcl_ServiceEvent(int);
TKGPU_COMPAT_API int Tcl_ServiceAll(void);
#ifndef _WIN32
typedef void Tcl_FileProc(void *,int);
enum { TKGPU_READABLE=2,TKGPU_WRITABLE=4,TKGPU_EXCEPTION=8 };
TKGPU_COMPAT_API void Tcl_CreateFileHandler(int,int,Tcl_FileProc *,void *);
TKGPU_COMPAT_API void Tcl_DeleteFileHandler(int);
#endif
/* Native synchronization subset; handles are lazily initialized. */
typedef struct TKGPUOpaqueMutex *Tcl_Mutex;
typedef struct TKGPUOpaqueCondition *Tcl_Condition;
typedef struct TKGPUOpaqueThread *Tcl_ThreadId;
typedef struct TKGPUOpaqueThreadData *Tcl_ThreadDataKey;
typedef struct TKGPUTime { long sec,usec; } Tcl_Time;
TKGPU_COMPAT_API void *Tcl_InitNotifier(void);
TKGPU_COMPAT_API void Tcl_FinalizeNotifier(void *);
TKGPU_COMPAT_API void Tcl_AlertNotifier(void *);
TKGPU_COMPAT_API void Tcl_SetTimer(const Tcl_Time *);
TKGPU_COMPAT_API int Tcl_WaitForEvent(const Tcl_Time *);
typedef void Tcl_EventSetupProc(void *,int);
typedef void Tcl_EventCheckProc(void *,int);
TKGPU_COMPAT_API void Tcl_CreateEventSource(Tcl_EventSetupProc *,Tcl_EventCheckProc *,void *);
TKGPU_COMPAT_API void Tcl_DeleteEventSource(Tcl_EventSetupProc *,Tcl_EventCheckProc *,void *);
TKGPU_COMPAT_API void Tcl_SetMaxBlockTime(const Tcl_Time *);
/* Tcl 8.6 resource limits: per-interpreter state, enforced by evaluation. */
enum { TKGPU_LIMIT_COMMANDS=1, TKGPU_LIMIT_TIME=2 };
typedef void Tcl_LimitHandlerProc(void *,Tcl_Interp *);
typedef void Tcl_LimitHandlerDeleteProc(void *);
TKGPU_COMPAT_API void Tcl_LimitAddHandler(Tcl_Interp *,int,Tcl_LimitHandlerProc *,void *,Tcl_LimitHandlerDeleteProc *);
TKGPU_COMPAT_API void Tcl_LimitRemoveHandler(Tcl_Interp *,int,Tcl_LimitHandlerProc *,void *);
TKGPU_COMPAT_API int Tcl_LimitReady(Tcl_Interp *);
TKGPU_COMPAT_API int Tcl_LimitCheck(Tcl_Interp *);
TKGPU_COMPAT_API int Tcl_LimitExceeded(Tcl_Interp *);
TKGPU_COMPAT_API void Tcl_LimitSetCommands(Tcl_Interp *,int);
TKGPU_COMPAT_API void Tcl_LimitSetTime(Tcl_Interp *,Tcl_Time *);
TKGPU_COMPAT_API void Tcl_LimitSetGranularity(Tcl_Interp *,int,int);
TKGPU_COMPAT_API int Tcl_LimitTypeEnabled(Tcl_Interp *,int);
TKGPU_COMPAT_API int Tcl_LimitTypeExceeded(Tcl_Interp *,int);
TKGPU_COMPAT_API void Tcl_LimitTypeSet(Tcl_Interp *,int);
TKGPU_COMPAT_API void Tcl_LimitTypeReset(Tcl_Interp *,int);
TKGPU_COMPAT_API int Tcl_LimitGetCommands(Tcl_Interp *);
TKGPU_COMPAT_API void Tcl_LimitGetTime(Tcl_Interp *,Tcl_Time *);
TKGPU_COMPAT_API int Tcl_LimitGetGranularity(Tcl_Interp *,int);
typedef void Tcl_GetTimeProc(Tcl_Time *,void *);
typedef void Tcl_ScaleTimeProc(Tcl_Time *,void *);
#ifdef TKGPU_REFERENCE_STUBS
TKGPU_COMPAT_API void Tcl_SetTimeProc(Tcl_GetTimeProc *,Tcl_ScaleTimeProc *,void *);
TKGPU_COMPAT_API void Tcl_QueryTimeProc(Tcl_GetTimeProc **,Tcl_ScaleTimeProc **,void **);
#endif
TKGPU_COMPAT_API void Tcl_GetTime(Tcl_Time *);
TKGPU_COMPAT_API void Tcl_Sleep(int);
TKGPU_COMPAT_API void Tcl_MutexLock(Tcl_Mutex *);
TKGPU_COMPAT_API void Tcl_MutexUnlock(Tcl_Mutex *);
TKGPU_COMPAT_API void Tcl_MutexFinalize(Tcl_Mutex *);
TKGPU_COMPAT_API Tcl_Mutex *Tcl_GetAllocMutex(void);
TKGPU_COMPAT_API void Tcl_ConditionWait(Tcl_Condition *,Tcl_Mutex *,const Tcl_Time *);
TKGPU_COMPAT_API void Tcl_ConditionNotify(Tcl_Condition *);
TKGPU_COMPAT_API void Tcl_ConditionFinalize(Tcl_Condition *);
typedef struct Tcl_Event Tcl_Event;
typedef int Tcl_EventProc(Tcl_Event *,int);
typedef int Tcl_EventDeleteProc(Tcl_Event *,void *);
TKGPU_COMPAT_API void Tcl_DeleteEvents(Tcl_EventDeleteProc *,void *);
struct Tcl_Event { Tcl_EventProc *proc; Tcl_Event *nextPtr; };
typedef enum { TKGPU_QUEUE_TAIL=0,TKGPU_QUEUE_HEAD=1,TKGPU_QUEUE_MARK=2 } Tcl_QueuePosition;
TKGPU_COMPAT_API void Tcl_QueueEvent(Tcl_Event *,Tcl_QueuePosition);
TKGPU_COMPAT_API void Tcl_ThreadQueueEvent(Tcl_ThreadId,Tcl_Event *,Tcl_QueuePosition);
TKGPU_COMPAT_API void Tcl_ThreadAlert(Tcl_ThreadId);
#ifdef TKGPU_LEGACY_WINDOWS
/* Explicit owner-thread cleanup required for the Win9x/XP preview backend. */
TKGPU_COMPAT_API int TKGPU_FinalizeLegacyThread(void);
#endif
TKGPU_COMPAT_API Tcl_ThreadId Tcl_GetCurrentThread(void);
TKGPU_COMPAT_API void *Tcl_GetThreadData(Tcl_ThreadDataKey *,int);
#ifdef _WIN32
typedef unsigned (__stdcall Tcl_ThreadCreateProc)(void *);
#else
typedef void Tcl_ThreadCreateProc(void *);
#endif
TKGPU_COMPAT_API int Tcl_CreateThread(Tcl_ThreadId *,Tcl_ThreadCreateProc *,void *,int,int);
TKGPU_COMPAT_API int Tcl_JoinThread(Tcl_ThreadId,int *);
typedef struct Tcl_NotifierProcs {
    void (*setTimerProc)(const Tcl_Time *);
    int (*waitForEventProc)(const Tcl_Time *);
    void (*createFileHandlerProc)(int,int,void (*)(void *,int),void *);
    void (*deleteFileHandlerProc)(int);
    void *(*initNotifierProc)(void);
    void (*finalizeNotifierProc)(void *);
    void (*alertNotifierProc)(void *);
    void (*serviceModeHookProc)(int);
} Tcl_NotifierProcs;
TKGPU_COMPAT_API void Tcl_SetNotifier(Tcl_NotifierProcs *);
typedef struct Tcl_Pid_ *Tcl_Pid;
TKGPU_COMPAT_API void Tcl_DetachPids(int,Tcl_Pid *);
TKGPU_COMPAT_API void Tcl_ReapDetachedProcs(void);
TKGPU_COMPAT_API Tcl_Pid Tcl_WaitPid(Tcl_Pid,int *,int);
typedef struct Tcl_Encoding_ *Tcl_Encoding;
typedef struct Tcl_EncodingState_ *Tcl_EncodingState;
typedef int Tcl_EncodingConvertProc(void *,const char *,int,int,Tcl_EncodingState *,char *,int,int *,int *,int *);
typedef void Tcl_EncodingFreeProc(void *);
typedef struct Tcl_EncodingType {
    const char *encodingName;
    Tcl_EncodingConvertProc *toUtfProc;
    Tcl_EncodingConvertProc *fromUtfProc;
    Tcl_EncodingFreeProc *freeProc;
    void *clientData;
    int nullSize;
} Tcl_EncodingType;
TKGPU_COMPAT_API Tcl_Encoding Tcl_GetEncoding(Tcl_Interp *,const char *);
TKGPU_COMPAT_API void Tcl_FreeEncoding(Tcl_Encoding);
TKGPU_COMPAT_API int Tcl_GetEncodingFromObj(Tcl_Interp *,Tcl_Obj *,Tcl_Encoding *);
TKGPU_COMPAT_API const char *Tcl_GetEncodingName(Tcl_Encoding);
TKGPU_COMPAT_API void Tcl_GetEncodingNames(Tcl_Interp *);
TKGPU_COMPAT_API const char *Tcl_GetEncodingNameFromEnvironment(Tcl_DString *);
TKGPU_COMPAT_API int Tcl_SetSystemEncoding(Tcl_Interp *,const char *);
TKGPU_COMPAT_API Tcl_Encoding Tcl_CreateEncoding(const Tcl_EncodingType *);
TKGPU_COMPAT_API int Tcl_ExternalToUtf(Tcl_Interp *,Tcl_Encoding,const char *,int,int,Tcl_EncodingState *,char *,int,int *,int *,int *);
TKGPU_COMPAT_API char *Tcl_ExternalToUtfDString(Tcl_Encoding,const char *,int,Tcl_DString *);
TKGPU_COMPAT_API int Tcl_UtfToExternal(Tcl_Interp *,Tcl_Encoding,const char *,int,int,Tcl_EncodingState *,char *,int,int *,int *,int *);
TKGPU_COMPAT_API char *Tcl_UtfToExternalDString(Tcl_Encoding,const char *,int,Tcl_DString *);
TKGPU_COMPAT_API const char *Tcl_GetDefaultEncodingDir(void);
TKGPU_COMPAT_API void Tcl_SetDefaultEncodingDir(const char *);
TKGPU_COMPAT_API Tcl_Obj *Tcl_GetEncodingSearchPath(void);
TKGPU_COMPAT_API int Tcl_SetEncodingSearchPath(Tcl_Obj *);
typedef struct TKGPUOpaqueCommand *Tcl_Command;
typedef int Tcl_ObjCmdProc(void *,Tcl_Interp *,int,Tcl_Obj *const []);
typedef void Tcl_CmdDeleteProc(void *);
typedef int Tcl_CmdProc(void *,Tcl_Interp *,int,const char *[]);
typedef void Tcl_NamespaceDeleteProc(void *);
typedef struct Tcl_Namespace {
    char *name,*fullName;
    void *clientData;
    Tcl_NamespaceDeleteProc *deleteProc;
    struct Tcl_Namespace *parentPtr;
} Tcl_Namespace;
TKGPU_COMPAT_API Tcl_Namespace *Tcl_GetGlobalNamespace(Tcl_Interp *);
TKGPU_COMPAT_API Tcl_Namespace *Tcl_GetCurrentNamespace(Tcl_Interp *);
TKGPU_COMPAT_API Tcl_Namespace *Tcl_FindNamespace(Tcl_Interp *,const char *,Tcl_Namespace *,int);
TKGPU_COMPAT_API int Tcl_Export(Tcl_Interp *,Tcl_Namespace *,const char *,int);
TKGPU_COMPAT_API int Tcl_AppendExportList(Tcl_Interp *,Tcl_Namespace *,Tcl_Obj *);
TKGPU_COMPAT_API Tcl_Namespace *Tcl_CreateNamespace(Tcl_Interp *,const char *,void *,Tcl_NamespaceDeleteProc *);
TKGPU_COMPAT_API void Tcl_DeleteNamespace(Tcl_Namespace *);
TKGPU_COMPAT_API int Tcl_Import(Tcl_Interp *,Tcl_Namespace *,const char *,int);
TKGPU_COMPAT_API int Tcl_ForgetImport(Tcl_Interp *,Tcl_Namespace *,const char *);
TKGPU_COMPAT_API Tcl_Obj *Tcl_GetNamespaceUnknownHandler(Tcl_Interp *,Tcl_Namespace *);
TKGPU_COMPAT_API int Tcl_SetNamespaceUnknownHandler(Tcl_Interp *,Tcl_Namespace *,Tcl_Obj *);
TKGPU_COMPAT_API Tcl_Command Tcl_CreateEnsemble(Tcl_Interp *,const char *,Tcl_Namespace *,int);
TKGPU_COMPAT_API Tcl_Command Tcl_FindEnsemble(Tcl_Interp *,Tcl_Obj *,int);
TKGPU_COMPAT_API int Tcl_IsEnsemble(Tcl_Command);
TKGPU_COMPAT_API int Tcl_GetEnsembleFlags(Tcl_Interp *,Tcl_Command,int *);
TKGPU_COMPAT_API int Tcl_SetEnsembleFlags(Tcl_Interp *,Tcl_Command,int);
TKGPU_COMPAT_API int Tcl_GetEnsembleMappingDict(Tcl_Interp *,Tcl_Command,Tcl_Obj **);
TKGPU_COMPAT_API int Tcl_SetEnsembleMappingDict(Tcl_Interp *,Tcl_Command,Tcl_Obj *);
TKGPU_COMPAT_API int Tcl_GetEnsembleNamespace(Tcl_Interp *,Tcl_Command,Tcl_Namespace **);
TKGPU_COMPAT_API int Tcl_GetEnsembleParameterList(Tcl_Interp *,Tcl_Command,Tcl_Obj **);
TKGPU_COMPAT_API int Tcl_SetEnsembleParameterList(Tcl_Interp *,Tcl_Command,Tcl_Obj *);
TKGPU_COMPAT_API int Tcl_GetEnsembleSubcommandList(Tcl_Interp *,Tcl_Command,Tcl_Obj **);
TKGPU_COMPAT_API int Tcl_SetEnsembleSubcommandList(Tcl_Interp *,Tcl_Command,Tcl_Obj *);
TKGPU_COMPAT_API int Tcl_GetEnsembleUnknownHandler(Tcl_Interp *,Tcl_Command,Tcl_Obj **);
TKGPU_COMPAT_API int Tcl_SetEnsembleUnknownHandler(Tcl_Interp *,Tcl_Command,Tcl_Obj *);
TKGPU_COMPAT_API Tcl_Interp *Tcl_CreateSlave(Tcl_Interp *,const char *,int);
TKGPU_COMPAT_API Tcl_Interp *Tcl_GetSlave(Tcl_Interp *,const char *);
TKGPU_COMPAT_API Tcl_Interp *Tcl_GetMaster(Tcl_Interp *);
TKGPU_COMPAT_API int Tcl_GetInterpPath(Tcl_Interp *,Tcl_Interp *);
TKGPU_COMPAT_API int Tcl_IsSafe(Tcl_Interp *);
TKGPU_COMPAT_API int Tcl_MakeSafe(Tcl_Interp *);
TKGPU_COMPAT_API int Tcl_ExposeCommand(Tcl_Interp *,const char *,const char *);
TKGPU_COMPAT_API int Tcl_HideCommand(Tcl_Interp *,const char *,const char *);
TKGPU_COMPAT_API int Tcl_CreateAlias(Tcl_Interp *,const char *,Tcl_Interp *,const char *,int,const char *const *);
TKGPU_COMPAT_API int Tcl_CreateAliasObj(Tcl_Interp *,const char *,Tcl_Interp *,const char *,int,Tcl_Obj *const []);
TKGPU_COMPAT_API int Tcl_GetAlias(Tcl_Interp *,const char *,Tcl_Interp **,const char **,int *,const char ***);
TKGPU_COMPAT_API int Tcl_GetAliasObj(Tcl_Interp *,const char *,Tcl_Interp **,const char **,int *,Tcl_Obj ***);
typedef struct Tcl_Trace_ *Tcl_Trace;
typedef void Tcl_CmdTraceProc(void *,Tcl_Interp *,int,char *,Tcl_CmdProc *,void *,int,const char *[]);
typedef int Tcl_CmdObjTraceProc(void *,Tcl_Interp *,int,const char *,Tcl_Command,int,Tcl_Obj *const []);
typedef void Tcl_CmdObjTraceDeleteProc(void *);
TKGPU_COMPAT_API Tcl_Trace Tcl_CreateTrace(Tcl_Interp *,int,Tcl_CmdTraceProc *,void *);
TKGPU_COMPAT_API Tcl_Trace Tcl_CreateObjTrace(Tcl_Interp *,int,int,Tcl_CmdObjTraceProc *,void *,Tcl_CmdObjTraceDeleteProc *);
TKGPU_COMPAT_API void Tcl_DeleteTrace(Tcl_Interp *,Tcl_Trace);
typedef int Tcl_NRPostProc(void *[],Tcl_Interp *,int);
TKGPU_COMPAT_API Tcl_Command Tcl_NRCreateCommand(Tcl_Interp *,const char *,Tcl_ObjCmdProc *,Tcl_ObjCmdProc *,void *,Tcl_CmdDeleteProc *);
TKGPU_COMPAT_API int Tcl_NREvalObj(Tcl_Interp *,Tcl_Obj *,int);
TKGPU_COMPAT_API int Tcl_NREvalObjv(Tcl_Interp *,int,Tcl_Obj *const [],int);
TKGPU_COMPAT_API int Tcl_NRCmdSwap(Tcl_Interp *,Tcl_Command,int,Tcl_Obj *const [],int);
TKGPU_COMPAT_API void Tcl_NRAddCallback(Tcl_Interp *,Tcl_NRPostProc *,void *,void *,void *,void *);
TKGPU_COMPAT_API int Tcl_NRCallObjProc(Tcl_Interp *,Tcl_ObjCmdProc *,void *,int,Tcl_Obj *const []);
TKGPU_COMPAT_API int Tcl_NRExprObj(Tcl_Interp *,Tcl_Obj *,Tcl_Obj *);
TKGPU_COMPAT_API int Tcl_NRSubstObj(Tcl_Interp *,Tcl_Obj *,int);
TKGPU_COMPAT_API Tcl_Obj *Tcl_SubstObj(Tcl_Interp *,Tcl_Obj *,int);
typedef enum { TKGPU_VAL_INT, TKGPU_VAL_DOUBLE, TKGPU_VAL_EITHER, TKGPU_VAL_WIDE_INT } Tcl_ValueType;
typedef struct Tcl_Value {
    Tcl_ValueType type;
    long intValue;
    double doubleValue;
    long long wideValue;
} Tcl_Value;
typedef int Tcl_MathProc(void *,Tcl_Interp *,Tcl_Value *,Tcl_Value *);
TKGPU_COMPAT_API void Tcl_CreateMathFunc(Tcl_Interp *,const char *,int,Tcl_ValueType *,Tcl_MathProc *,void *);
TKGPU_COMPAT_API int Tcl_GetMathFuncInfo(Tcl_Interp *,const char *,int *,Tcl_ValueType **,Tcl_MathProc **,void **);
TKGPU_COMPAT_API Tcl_Obj *Tcl_ListMathFuncs(Tcl_Interp *,const char *);
typedef struct {
    int type;
    const char *keyStr;
    void *srcPtr;
    void *dstPtr;
    const char *helpStr;
    void *clientData;
} Tcl_ArgvInfo;
TKGPU_COMPAT_API int Tcl_ParseArgsObjv(Tcl_Interp *,const Tcl_ArgvInfo *,int *,Tcl_Obj *const *,Tcl_Obj ***);
typedef struct Tcl_CmdInfo {
    int isNativeObjectProc;
    Tcl_ObjCmdProc *objProc;
    void *objClientData;
    Tcl_CmdProc *proc;
    void *clientData;
    Tcl_CmdDeleteProc *deleteProc;
    void *deleteData;
    Tcl_Namespace *namespacePtr;
} Tcl_CmdInfo;
/* Global native commands only; namespacePtr identifies the global namespace. */
TKGPU_COMPAT_API Tcl_Command Tcl_FindCommand(Tcl_Interp *,const char *,Tcl_Namespace *,int);
TKGPU_COMPAT_API int Tcl_GetCommandInfo(Tcl_Interp *,const char *,Tcl_CmdInfo *);
TKGPU_COMPAT_API int Tcl_GetCommandInfoFromToken(Tcl_Command,Tcl_CmdInfo *);
TKGPU_COMPAT_API int Tcl_SetCommandInfo(Tcl_Interp *,const char *,const Tcl_CmdInfo *);
TKGPU_COMPAT_API int Tcl_SetCommandInfoFromToken(Tcl_Command,const Tcl_CmdInfo *);
TKGPU_COMPAT_API Tcl_Command Tcl_CreateCommand(Tcl_Interp *,const char *,Tcl_CmdProc *,void *,Tcl_CmdDeleteProc *);
TKGPU_COMPAT_API void Tcl_AppendResult(Tcl_Interp *,...);
TKGPU_COMPAT_API void Tcl_AppendResultVA(Tcl_Interp *,va_list);
TKGPU_COMPAT_API void Tcl_AppendElement(Tcl_Interp *,const char *);
typedef struct { unsigned char *pixelPtr; int width,height,pitch,pixelSize,offset[4]; } Tk_PhotoImageBlock;
TKGPU_COMPAT_API void *Tk_FindPhoto(Tcl_Interp *,const char *);
TKGPU_COMPAT_API int Tk_PhotoPutBlock(Tcl_Interp *,void *,Tk_PhotoImageBlock *,int,int,int,int,int);
TKGPU_COMPAT_API int Tk_PhotoGetImage(void *,Tk_PhotoImageBlock *);
TKGPU_COMPAT_API Tcl_Command Tcl_CreateObjCommand(Tcl_Interp *,const char *,Tcl_ObjCmdProc *,void *,Tcl_CmdDeleteProc *);
TKGPU_COMPAT_API int Tcl_DeleteCommand(Tcl_Interp *,const char *);
TKGPU_COMPAT_API const char *Tcl_GetCommandName(Tcl_Interp *,Tcl_Command);
TKGPU_COMPAT_API Tcl_Command Tcl_GetCommandFromObj(Tcl_Interp *,Tcl_Obj *);
TKGPU_COMPAT_API int Tcl_DeleteCommandFromToken(Tcl_Interp *,Tcl_Command);
TKGPU_COMPAT_API void Tcl_GetCommandFullName(Tcl_Interp *,Tcl_Command,Tcl_Obj *);
TKGPU_COMPAT_API int Tcl_EvalFile(Tcl_Interp *,const char *);
TKGPU_COMPAT_API void Tcl_AddErrorInfo(Tcl_Interp *,const char *);
TKGPU_COMPAT_API void Tcl_AddObjErrorInfo(Tcl_Interp *,const char *,int);
TKGPU_COMPAT_API void Tcl_SetObjErrorCode(Tcl_Interp *,Tcl_Obj *);
TKGPU_COMPAT_API void Tcl_SetErrorCode(Tcl_Interp *,...);
TKGPU_COMPAT_API void Tcl_SetErrorCodeVA(Tcl_Interp *,va_list);
TKGPU_COMPAT_API void Tcl_AppendObjToErrorInfo(Tcl_Interp *,Tcl_Obj *);
TKGPU_COMPAT_API void Tcl_WrongNumArgs(Tcl_Interp *,int,Tcl_Obj *const [],const char *);
TKGPU_COMPAT_API int TclBN_mp_init(mp_int *);
TKGPU_COMPAT_API void TclBN_mp_clear(mp_int *);
TKGPU_COMPAT_API int TclBN_mp_read_radix(mp_int *,const char *,int);
TKGPU_COMPAT_API size_t TclBN_mp_unsigned_bin_size(const mp_int *);
TKGPU_COMPAT_API int TclBN_mp_to_unsigned_bin_n(const mp_int *,unsigned char *,unsigned long *);
TKGPU_COMPAT_API Tcl_Obj *Tcl_NewBignumObj(mp_int *);
TKGPU_COMPAT_API int Tcl_GetBignumFromObj(Tcl_Interp *,Tcl_Obj *,mp_int *);
TKGPU_COMPAT_API Tcl_Obj *Tcl_DbNewBignumObj(mp_int *,const char *,int);
TKGPU_COMPAT_API void Tcl_SetBignumObj(Tcl_Obj *,mp_int *);
TKGPU_COMPAT_API int Tcl_TakeBignumFromObj(Tcl_Interp *,Tcl_Obj *,mp_int *);
TKGPU_COMPAT_API int Tcl_InitBignumFromDouble(Tcl_Interp *,double,mp_int *);
TKGPU_COMPAT_API int Tcl_EvalObjv(Tcl_Interp *,int,Tcl_Obj *const [],int);
TKGPU_COMPAT_API int Tcl_GetLongFromObj(Tcl_Interp *,Tcl_Obj *,long *);
TKGPU_COMPAT_API Tcl_Obj *Tcl_NewBooleanObj(int);
TKGPU_COMPAT_API void Tcl_SetBooleanObj(Tcl_Obj *,int);
TKGPU_COMPAT_API void Tcl_SetListObj(Tcl_Obj *,int,Tcl_Obj *const []);
TKGPU_COMPAT_API void Tcl_SetUnicodeObj(Tcl_Obj *,const Tcl_UniChar *,int);
TKGPU_COMPAT_API int Tcl_GetCharLength(Tcl_Obj *);
TKGPU_COMPAT_API Tcl_UniChar Tcl_GetUniChar(Tcl_Obj *,int);
TKGPU_COMPAT_API Tcl_Obj *Tcl_GetRange(Tcl_Obj *,int,int);
TKGPU_COMPAT_API void Tcl_AppendUnicodeToObj(Tcl_Obj *,const Tcl_UniChar *,int);
TKGPU_COMPAT_API int Tcl_GetErrorLine(Tcl_Interp *);
TKGPU_COMPAT_API void Tcl_SetErrorLine(Tcl_Interp *,int);
TKGPU_COMPAT_API void TKGPU_ReleaseObject(Tcl_Obj *);
TKGPU_COMPAT_API Tcl_Obj *TKGPU_RetainObject(Tcl_Obj *);
TKGPU_COMPAT_API Tcl_Obj *TKGPU_CopyResultObject(Tcl_Interp *);
TKGPU_COMPAT_API int TKGPU_SetResultObject(Tcl_Interp *,Tcl_Obj *);
#ifdef __cplusplus
}
#endif
#endif
