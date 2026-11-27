#include "TKGPUCompat.H"
#include "../../core/TKGPUCoreInternal.H"
#include <stdlib.h>
#include <string.h>

typedef struct {
    int has_return;
    int return_code;
    int return_level;
} return_state;

static const char return_key[]="TKGPU::return-options";

static void dispose_return(void *data,Tcl_Interp *i) {
    (void)i;
    free(data);
}

static return_state *get_return(Tcl_Interp *i,int create) {
    if(!i||i->disposing||i->destroying) return NULL;
    return_state *s=Tcl_GetAssocData(i,return_key,NULL);
    if(!s&&create) {
        s=calloc(1,sizeof *s);
        if(!s) Tcl_Panic("cannot allocate return options state");
        Tcl_SetAssocData(i,return_key,dispose_return,s);
    }
    return s;
}

static void put_pair(Tcl_Interp *i,Tcl_Obj *dict,const char *k,Tcl_Obj *val) {
    Tcl_Obj *key=Tcl_NewStringObj(k,-1);
    TKGPU_RetainObject(key);
    TKGPU_RetainObject(val);
    (void)Tcl_DictObjPut(i,dict,key,val);
    TKGPU_ReleaseObject(val);
    TKGPU_ReleaseObject(key);
}

enum { CODE_OK=0, CODE_ERROR=1, CODE_RETURN=2, CODE_BREAK=3, CODE_CONTINUE=4 };

static int parse_code(Tcl_Interp *i,Tcl_Obj *val,int *out) {
    const char *s=Tcl_GetString(val);
    if(!s) return CODE_ERROR;
    if(!strcmp(s,"ok")) { *out=CODE_OK; return CODE_OK; }
    if(!strcmp(s,"error")) { *out=CODE_ERROR; return CODE_OK; }
    if(!strcmp(s,"return")) { *out=CODE_RETURN; return CODE_OK; }
    if(!strcmp(s,"break")) { *out=CODE_BREAK; return CODE_OK; }
    if(!strcmp(s,"continue")) { *out=CODE_CONTINUE; return CODE_OK; }
    int n=0;
    if(Tcl_GetIntFromObj(NULL,val,&n)==CODE_OK) { *out=n; return CODE_OK; }
    Tcl_SetObjResult(i,Tcl_ObjPrintf(
        "bad completion code \"%s\": must be ok, error, return, break, continue, or an integer",s));
    return CODE_ERROR;
}

Tcl_Obj *Tcl_GetReturnOptions(Tcl_Interp *i,int result) {
    Tcl_Obj *dict=Tcl_NewDictObj();
    if(!dict||!i) return dict;
    return_state *s=get_return(i,0);
    int code=result,level=0;
    if(result==CODE_RETURN&&s&&s->has_return) {
        code=s->return_code;
        level=s->return_level;
    }
    put_pair(i,dict,"-code",Tcl_NewIntObj(code));
    put_pair(i,dict,"-level",Tcl_NewIntObj(level));
    if(result==CODE_ERROR) {
        const char *ec=Tcl_GetVar(i,"errorCode",TKGPU_GLOBAL_ONLY);
        put_pair(i,dict,"-errorcode",Tcl_NewStringObj(ec?ec:"NONE",-1));
        const char *ei=Tcl_GetVar(i,"errorInfo",TKGPU_GLOBAL_ONLY);
        if(!ei||!*ei) ei=Tcl_GetStringResult(i);
        put_pair(i,dict,"-errorinfo",Tcl_NewStringObj(ei?ei:"",-1));
        int line=Tcl_GetErrorLine(i);
        put_pair(i,dict,"-errorline",Tcl_NewIntObj(line>0?line:1));
    }
    return dict;
}

int Tcl_SetReturnOptions(Tcl_Interp *i,Tcl_Obj *options) {
    if(!i||i->disposing||!options) return CODE_ERROR;
    TKGPU_RetainObject(options);
    int size=0;
    if(Tcl_DictObjSize(NULL,options,&size)!=CODE_OK) {
        const char *raw=Tcl_GetString(options);
        Tcl_SetObjResult(i,Tcl_ObjPrintf("expected dict but got \"%s\"",raw?raw:""));
        TKGPU_ReleaseObject(options);
        return CODE_ERROR;
    }
    int code=CODE_OK,level=1;
    Tcl_Obj *kCode=Tcl_NewStringObj("-code",-1);
    Tcl_Obj *kLevel=Tcl_NewStringObj("-level",-1);
    Tcl_Obj *kErrCode=Tcl_NewStringObj("-errorcode",-1);
    Tcl_Obj *kErrInfo=Tcl_NewStringObj("-errorinfo",-1);
    Tcl_Obj *kErrLine=Tcl_NewStringObj("-errorline",-1);
    TKGPU_RetainObject(kCode); TKGPU_RetainObject(kLevel);
    TKGPU_RetainObject(kErrCode); TKGPU_RetainObject(kErrInfo); TKGPU_RetainObject(kErrLine);
    Tcl_Obj *val=NULL;
    int status=CODE_OK;
    if(Tcl_DictObjGet(i,options,kCode,&val)==CODE_OK&&val) {
        if(parse_code(i,val,&code)!=CODE_OK) status=CODE_ERROR;
    }
    if(status==CODE_OK&&Tcl_DictObjGet(i,options,kLevel,&val)==CODE_OK&&val) {
        int lv=0;
        if(Tcl_GetIntFromObj(NULL,val,&lv)!=CODE_OK||lv<0) {
            const char *raw=Tcl_GetString(val);
            Tcl_SetObjResult(i,Tcl_ObjPrintf(
                "bad -level value: expected non-negative integer but got \"%s\"",raw?raw:""));
            status=CODE_ERROR;
        } else {
            level=lv;
        }
    }
    if(status==CODE_OK&&Tcl_DictObjGet(i,options,kErrCode,&val)==CODE_OK&&val) {
        Tcl_SetObjErrorCode(i,val);
    }
    if(status==CODE_OK&&Tcl_DictObjGet(i,options,kErrInfo,&val)==CODE_OK&&val) {
        const char *ei=Tcl_GetString(val);
        (void)Tcl_SetVar(i,"errorInfo",ei?ei:"",TKGPU_GLOBAL_ONLY);
    }
    if(status==CODE_OK&&Tcl_DictObjGet(i,options,kErrLine,&val)==CODE_OK&&val) {
        int line=0;
        if(Tcl_GetIntFromObj(NULL,val,&line)==CODE_OK) Tcl_SetErrorLine(i,line);
    }
    TKGPU_ReleaseObject(kCode); TKGPU_ReleaseObject(kLevel);
    TKGPU_ReleaseObject(kErrCode); TKGPU_ReleaseObject(kErrInfo); TKGPU_ReleaseObject(kErrLine);
    TKGPU_ReleaseObject(options);
    if(status!=CODE_OK) return CODE_ERROR;
    return_state *s=get_return(i,1);
    if(s) {
        s->has_return=(level>0);
        s->return_code=code;
        s->return_level=level;
    }
    return level>0?CODE_RETURN:code;
}
