#include "TKGPUCompat.H"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct Tcl_Encoding_ {
    char *name;
    int ref_count;
    int kind; /* 0=utf8/identity, 1=iso8859-1, 2=ascii, 3=custom */
    Tcl_EncodingType custom;
    struct Tcl_Encoding_ *next;
};

static Tcl_Mutex enc_mutex;
static struct Tcl_Encoding_ *enc_list;
static struct Tcl_Encoding_ *sys_encoding;
static char *default_enc_dir;
static Tcl_Obj *enc_search_path;

static char *enc_dup(const char *s) {
    size_t n=s?strlen(s):0;
    char *p=malloc(n+1);
    if(!p) Tcl_Panic("cannot copy encoding name");
    if(n) memcpy(p,s,n);
    p[n]=0;
    return p;
}

static void init_builtin_encodings_locked(void) {
    if(enc_list) return;
    static const struct { const char *name; int kind; } builtins[]={
        {"utf-8",0}, {"identity",0}, {"binary",1}, {"iso8859-1",1}, {"latin1",1}, {"ascii",2}
    };
    for(size_t k=0;k<sizeof builtins/sizeof builtins[0];k++) {
        struct Tcl_Encoding_ *e=calloc(1,sizeof *e);
        if(!e) Tcl_Panic("cannot allocate builtin encoding");
        e->name=enc_dup(builtins[k].name);
        e->kind=builtins[k].kind;
        e->ref_count=1;
        e->next=enc_list;
        enc_list=e;
        if(!strcmp(e->name,"utf-8")) sys_encoding=e;
    }
}

static struct Tcl_Encoding_ *find_enc_locked(const char *name) {
    init_builtin_encodings_locked();
    if(!name||!*name) return sys_encoding;
    for(struct Tcl_Encoding_ *e=enc_list;e;e=e->next) {
        if(!strcmp(e->name,name)) return e;
    }
    return NULL;
}

Tcl_Encoding Tcl_GetEncoding(Tcl_Interp *i,const char *name) {
    Tcl_MutexLock(&enc_mutex);
    struct Tcl_Encoding_ *e=find_enc_locked(name);
    if(e) ++e->ref_count;
    Tcl_MutexUnlock(&enc_mutex);
    if(!e&&i) Tcl_SetObjResult(i,Tcl_ObjPrintf("unknown encoding \"%s\"",name?name:""));
    return e;
}

void Tcl_FreeEncoding(Tcl_Encoding encoding) {
    if(!encoding) return;
    Tcl_MutexLock(&enc_mutex);
    if(encoding->ref_count>1) --encoding->ref_count;
    Tcl_MutexUnlock(&enc_mutex);
}

int Tcl_GetEncodingFromObj(Tcl_Interp *i,Tcl_Obj *objPtr,Tcl_Encoding *encodingPtr) {
    const char *name=objPtr?Tcl_GetString(objPtr):NULL;
    Tcl_Encoding enc=Tcl_GetEncoding(i,name);
    if(!enc) return 1;
    if(encodingPtr) *encodingPtr=enc;
    else Tcl_FreeEncoding(enc);
    return 0;
}

const char *Tcl_GetEncodingName(Tcl_Encoding encoding) {
    if(!encoding) {
        Tcl_MutexLock(&enc_mutex);
        init_builtin_encodings_locked();
        const char *n=sys_encoding?sys_encoding->name:"utf-8";
        Tcl_MutexUnlock(&enc_mutex);
        return n;
    }
    return encoding->name;
}

void Tcl_GetEncodingNames(Tcl_Interp *i) {
    if(!i) return;
    Tcl_Obj *list=Tcl_NewListObj(0,NULL);
    TKGPU_RetainObject(list);
    Tcl_MutexLock(&enc_mutex);
    init_builtin_encodings_locked();
    for(struct Tcl_Encoding_ *e=enc_list;e;e=e->next) {
        Tcl_ListObjAppendElement(i,list,Tcl_NewStringObj(e->name,-1));
    }
    Tcl_MutexUnlock(&enc_mutex);
    Tcl_SetObjResult(i,list);
    TKGPU_ReleaseObject(list);
}

const char *Tcl_GetEncodingNameFromEnvironment(Tcl_DString *bufPtr) {
    const char *name="utf-8";
    if(bufPtr) {
        Tcl_DStringInit(bufPtr);
        Tcl_DStringAppend(bufPtr,name,-1);
        return Tcl_DStringValue(bufPtr);
    }
    return name;
}

int Tcl_SetSystemEncoding(Tcl_Interp *i,const char *name) {
    Tcl_MutexLock(&enc_mutex);
    struct Tcl_Encoding_ *e=find_enc_locked(name&&*name?name:"utf-8");
    if(e) sys_encoding=e;
    Tcl_MutexUnlock(&enc_mutex);
    if(!e) {
        if(i) Tcl_SetObjResult(i,Tcl_ObjPrintf("unknown encoding \"%s\"",name?name:""));
        return 1;
    }
    return 0;
}

Tcl_Encoding Tcl_CreateEncoding(const Tcl_EncodingType *typePtr) {
    if(!typePtr||!typePtr->encodingName) return NULL;
    Tcl_MutexLock(&enc_mutex);
    init_builtin_encodings_locked();
    struct Tcl_Encoding_ *e=calloc(1,sizeof *e);
    if(!e) { Tcl_MutexUnlock(&enc_mutex); Tcl_Panic("cannot allocate encoding"); }
    e->name=enc_dup(typePtr->encodingName);
    e->kind=3;
    e->custom=*typePtr;
    e->ref_count=2;
    e->next=enc_list;
    enc_list=e;
    Tcl_MutexUnlock(&enc_mutex);
    return e;
}

int Tcl_ExternalToUtf(Tcl_Interp *i,Tcl_Encoding encoding,const char *src,int srcLen,int flags,
                      Tcl_EncodingState *statePtr,char *dst,int dstLen,
                      int *srcReadPtr,int *dstWrotePtr,int *dstCharsPtr) {
    (void)i;
    if(!src) src="";
    if(srcLen<0) srcLen=(int)strlen(src);
    struct Tcl_Encoding_ *e=encoding;
    if(!e) {
        Tcl_MutexLock(&enc_mutex);
        init_builtin_encodings_locked();
        e=sys_encoding;
        Tcl_MutexUnlock(&enc_mutex);
    }
    if(e->kind==3&&e->custom.toUtfProc) {
        return e->custom.toUtfProc(e->custom.clientData,src,srcLen,flags,statePtr,dst,dstLen,
                                   srcReadPtr,dstWrotePtr,dstCharsPtr);
    }
    int sr=0,dw=0,dc=0;
    if(e->kind==1) { /* iso8859-1 -> UTF-8 */
        while(sr<srcLen) {
            unsigned char b=(unsigned char)src[sr];
            int need=b<0x80?1:2;
            if(dw+need>dstLen) break;
            if(b<0x80) dst[dw++]=(char)b;
            else { dst[dw++]=(char)(0xC0|(b>>6)); dst[dw++]=(char)(0x80|(b&0x3F)); }
            ++sr; ++dc;
        }
    } else { /* utf-8 / ascii / identity */
        int n=srcLen<dstLen?srcLen:dstLen;
        if(n>0) memcpy(dst,src,(size_t)n);
        sr=dw=n;
        dc=Tcl_NumUtfChars(dst,dw);
    }
    if(srcReadPtr) *srcReadPtr=sr;
    if(dstWrotePtr) *dstWrotePtr=dw;
    if(dstCharsPtr) *dstCharsPtr=dc;
    return sr<srcLen?1:0;
}

char *Tcl_ExternalToUtfDString(Tcl_Encoding encoding,const char *src,int srcLen,Tcl_DString *dsPtr) {
    if(!dsPtr) return NULL;
    Tcl_DStringInit(dsPtr);
    if(!src) return Tcl_DStringValue(dsPtr);
    if(srcLen<0) srcLen=(int)strlen(src);
    int cap=srcLen*3+16;
    char *buf=malloc((size_t)cap);
    if(!buf) Tcl_Panic("cannot allocate ExternalToUtfDString buffer");
    int sr=0,dw=0,dc=0;
    Tcl_EncodingState st=NULL;
    (void)Tcl_ExternalToUtf(NULL,encoding,src,srcLen,3,&st,buf,cap,&sr,&dw,&dc);
    Tcl_DStringAppend(dsPtr,buf,dw);
    free(buf);
    return Tcl_DStringValue(dsPtr);
}

int Tcl_UtfToExternal(Tcl_Interp *i,Tcl_Encoding encoding,const char *src,int srcLen,int flags,
                      Tcl_EncodingState *statePtr,char *dst,int dstLen,
                      int *srcReadPtr,int *dstWrotePtr,int *dstCharsPtr) {
    (void)i;
    if(!src) src="";
    if(srcLen<0) srcLen=(int)strlen(src);
    struct Tcl_Encoding_ *e=encoding;
    if(!e) {
        Tcl_MutexLock(&enc_mutex);
        init_builtin_encodings_locked();
        e=sys_encoding;
        Tcl_MutexUnlock(&enc_mutex);
    }
    if(e->kind==3&&e->custom.fromUtfProc) {
        return e->custom.fromUtfProc(e->custom.clientData,src,srcLen,flags,statePtr,dst,dstLen,
                                     srcReadPtr,dstWrotePtr,dstCharsPtr);
    }
    int sr=0,dw=0,dc=0;
    if(e->kind==1) { /* UTF-8 -> iso8859-1 */
        while(sr<srcLen&&dw<dstLen) {
            Tcl_UniChar ch=0;
            int step=Tcl_UtfToUniChar(src+sr,&ch);
            if(step<=0||sr+step>srcLen) break;
            dst[dw++]=(char)(ch<=0xFF?ch:'?');
            sr+=step; ++dc;
        }
    } else {
        int n=srcLen<dstLen?srcLen:dstLen;
        if(n>0) memcpy(dst,src,(size_t)n);
        sr=dw=n;
        dc=Tcl_NumUtfChars(src,sr);
    }
    if(srcReadPtr) *srcReadPtr=sr;
    if(dstWrotePtr) *dstWrotePtr=dw;
    if(dstCharsPtr) *dstCharsPtr=dc;
    return sr<srcLen?1:0;
}

char *Tcl_UtfToExternalDString(Tcl_Encoding encoding,const char *src,int srcLen,Tcl_DString *dsPtr) {
    if(!dsPtr) return NULL;
    Tcl_DStringInit(dsPtr);
    if(!src) return Tcl_DStringValue(dsPtr);
    if(srcLen<0) srcLen=(int)strlen(src);
    int cap=srcLen*2+16;
    char *buf=malloc((size_t)cap);
    if(!buf) Tcl_Panic("cannot allocate UtfToExternalDString buffer");
    int sr=0,dw=0,dc=0;
    Tcl_EncodingState st=NULL;
    (void)Tcl_UtfToExternal(NULL,encoding,src,srcLen,3,&st,buf,cap,&sr,&dw,&dc);
    Tcl_DStringAppend(dsPtr,buf,dw);
    free(buf);
    return Tcl_DStringValue(dsPtr);
}

const char *Tcl_GetDefaultEncodingDir(void) {
    Tcl_MutexLock(&enc_mutex);
    const char *res=default_enc_dir;
    Tcl_MutexUnlock(&enc_mutex);
    return res;
}

void Tcl_SetDefaultEncodingDir(const char *path) {
    Tcl_MutexLock(&enc_mutex);
    free(default_enc_dir);
    default_enc_dir=path?enc_dup(path):NULL;
    Tcl_MutexUnlock(&enc_mutex);
}

Tcl_Obj *Tcl_GetEncodingSearchPath(void) {
    Tcl_MutexLock(&enc_mutex);
    if(!enc_search_path) {
        enc_search_path=Tcl_NewListObj(0,NULL);
        if(enc_search_path) TKGPU_RetainObject(enc_search_path);
    }
    Tcl_Obj *res=enc_search_path;
    Tcl_MutexUnlock(&enc_mutex);
    return res;
}

int Tcl_SetEncodingSearchPath(Tcl_Obj *searchPath) {
    int len=0;
    if(!searchPath||Tcl_ListObjLength(NULL,searchPath,&len)!=0) return 0;
    Tcl_MutexLock(&enc_mutex);
    TKGPU_RetainObject(searchPath);
    if(enc_search_path) TKGPU_ReleaseObject(enc_search_path);
    enc_search_path=searchPath;
    Tcl_MutexUnlock(&enc_mutex);
    return 1;
}
