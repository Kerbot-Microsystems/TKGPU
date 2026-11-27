/* Native Linux filesystem operations. No VFS, recursive removal or copying. */
#include <utime.h>
#ifndef TKGPU_NATIVE_OP
#define TKGPU_NATIVE_OP(n) Tcl_##n
#endif
static char *native_mutation_path(Tcl_Obj *o,Tcl_DString *d) {
    const char *s=native_object_path(o);
    if(!s) {Tcl_DStringInit(d);return NULL;}
    char *p=native_path(s,d);
    if(!p&&errno!=EILSEQ) errno=EFAULT;
    return p;
}
static int native_finish(Tcl_DString *d,int result) {
    int err=errno; Tcl_DStringFree(d); errno=err; return result;
}
int TKGPU_NATIVE_OP(Chdir)(const char *s) {
    Tcl_DString d; char *p=native_path(s,&d);
    if(!p&&errno!=EILSEQ) errno=ENOENT;
    return native_finish(&d,p?chdir(p):-1);
}
int TKGPU_NATIVE_OP(FSChdir)(Tcl_Obj *o) {
    const char *s=native_object_path(o);
    return s?TKGPU_NATIVE_OP(Chdir)(s):-1;
}
Tcl_Obj *TKGPU_NATIVE_OP(FSGetCwd)(Tcl_Interp *i) {
    Tcl_DString d;
    if(!Tcl_GetCwd(i,&d)) {Tcl_DStringFree(&d);return NULL;}
    Tcl_Obj *o=Tcl_NewStringObj(d.string,d.length);
    Tcl_DStringFree(&d);
    if(o) Tcl_IncrRefCount(o);
    return o;
}
int TKGPU_NATIVE_OP(FSCreateDirectory)(Tcl_Obj *o) {
    Tcl_DString d;char *p=native_mutation_path(o,&d);
    int rc=TCL_ERROR;
    if(p&&!*p) {errno=ENOENT;rc=-1;}
    else if(p&&mkdir(p,0777)==0) rc=TCL_OK;
    return native_finish(&d,rc);
}
int TKGPU_NATIVE_OP(FSDeleteFile)(Tcl_Obj *o) {
    Tcl_DString d;char *p=native_mutation_path(o,&d);
    int rc=TCL_ERROR;
    if(p&&!*p) {errno=ENOENT;rc=-1;}
    else if(p&&unlink(p)==0) rc=TCL_OK;
    return native_finish(&d,rc);
}
int TKGPU_NATIVE_OP(FSRenameFile)(Tcl_Obj *src,Tcl_Obj *dst) {
    Tcl_DString a,b;char *x=native_mutation_path(src,&a);
    if(!x||!*x) {if(x)errno=ENOENT;return native_finish(&a,TCL_ERROR);}
    char *y=native_mutation_path(dst,&b);
    int rc=TCL_ERROR;
    if(y&&!*y) errno=ENOENT;
    else if(y&&rename(x,y)==0) rc=TCL_OK;
    else if(y&&errno==ENOTEMPTY) errno=EEXIST;
    native_finish(&b,rc);return native_finish(&a,rc);
}
int TKGPU_NATIVE_OP(FSUtime)(Tcl_Obj *o,struct utimbuf *times) {
    Tcl_DString d;char *p=native_mutation_path(o,&d);
    int rc=p&&*p?utime(p,times):-1;
    return native_finish(&d,rc);
}
#undef TKGPU_NATIVE_OP
