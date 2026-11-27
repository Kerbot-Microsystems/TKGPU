#include "TKGPUCompat.H"
#include "../../core/TKGPUCoreInternal.H"
#include "../../core/TKGPUList.H"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* The bounded facade publishes diagnostic variables immediately, preserving
   the current result through the existing variable API. Full Tcl exception
   return options and error stack propagation are separate facilities. */
void Tcl_AddObjErrorInfo(Tcl_Interp *i,const char *message,int length) {
    if(!i||i->disposing||!message||length< -1) return;
    size_t m=length<0?strlen(message):(size_t)length;
    if(m>=MAX_WORD||memchr(message,0,m)) return;
    char info[MAX_WORD];
    size_t k=tg_var_find(i,"errorInfo");
    const char *old=k<i->count?i->vars[k].value:tg_result(i);
    size_t n=strlen(old);
    if(n>=sizeof info||m>=sizeof info-n) return;
    memcpy(info,old,n); memcpy(info+n,message,m); info[n+m]=0;
    (void)Tcl_SetVar2(i,"errorInfo",NULL,info,0);
}
void Tcl_AddErrorInfo(Tcl_Interp *i,const char *message) {
    Tcl_AddObjErrorInfo(i,message,-1);
}
void Tcl_SetObjErrorCode(Tcl_Interp *i,Tcl_Obj *code) {
    if(!i||i->disposing||!code) return;
    /* Keep caller ownership, including when the input is already the
       interpreter result. Reject embedded NUL via the variable facade. */
    (void)Tcl_SetVar2Ex(i,"errorCode",NULL,code,0);
}
void Tcl_SetErrorCodeVA(Tcl_Interp *i,va_list source) {
    if(!i||i->disposing) return;
    char code[MAX_WORD]="";
    va_list args; va_copy(args,source);
    const char *part;
    while((part=va_arg(args,const char *))!=NULL) {
        if(!tg_list_append(code,sizeof code,part)) { va_end(args); return; }
    }
    va_end(args);
    (void)Tcl_SetVar2(i,"errorCode",NULL,code,0);
}
void Tcl_SetErrorCode(Tcl_Interp *i,...) {
    va_list args; va_start(args,i);
    Tcl_SetErrorCodeVA(i,args); va_end(args);
}
void Tcl_AppendObjToErrorInfo(Tcl_Interp *i,Tcl_Obj *obj) {
    if(!i||i->disposing||!obj) return;
    int n=0; const char *s=Tcl_GetStringFromObj(obj,&n);
    if(s) Tcl_AddObjErrorInfo(i,s,n);
}
void Tcl_WrongNumArgs(Tcl_Interp *i,int count,Tcl_Obj *const argv[],const char *message) {
    if(!i||i->disposing) return;
    char prefix[MAX_WORD]="",result[MAX_WORD];
    if(count<0||(count&&!argv)) goto fail;
    for(int k=0;k<count;k++) {
        size_t n; const char *word=tg_object_bytes(argv[k],&n);
        if(!word||n>=MAX_WORD||memchr(word,0,n)||!tg_list_append(prefix,sizeof prefix,word)) goto fail;
    }
    int n=snprintf(result,sizeof result,"wrong # args: should be \"%s%s%s\"",
        prefix,message&&*message&&count?" ":"",message?message:"");
    if(n<0||(size_t)n>=sizeof result) goto fail;
    tg_set_result(i,result,TG_ERROR); return;
fail:
    tg_set_result(i,"wrong # args",TG_ERROR);
}
