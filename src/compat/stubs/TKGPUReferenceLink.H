/* Native Linux links. A read result is owned; successful creation returns
   the caller's target object without changing its reference count. */
#ifndef TKGPU_LINK_API
#define TKGPU_LINK_API(n) Tcl_##n
#endif
Tcl_Obj *TKGPU_LINK_API(FSLink)(Tcl_Obj *path,Tcl_Obj *target,int action) {
    const char *raw=native_object_path(path);
    Tcl_DString d,t,check; Tcl_DStringInit(&t); Tcl_DStringInit(&check);
    if(!raw) return NULL;
    char *p=native_path(raw,&d); Tcl_Obj *result=NULL;
    if(!p||!*p) { if(p)errno=ENOENT; goto done; }
    if(!target) {
        size_t cap=256; char *buffer=NULL;
        for(;;) {
            char *next=realloc(buffer,cap); if(!next){errno=ENOMEM;break;} buffer=next;
            ssize_t n=readlink(p,buffer,cap);
            if(n<0) break;
            if((size_t)n<cap) {
                buffer[n]=0;
                if(!native_valid_path(buffer)) {errno=EILSEQ;break;}
                result=Tcl_NewStringObj(buffer,(int)n);
                if(result) Tcl_IncrRefCount(result);
                break;
            }
            if(cap>=(size_t)INT_MAX/2){errno=ENAMETOOLONG;break;}
            cap*=2;
        }
        int err=errno; free(buffer);errno=err;goto done;
    }
    struct stat st;
    const char *traw=native_object_path(target);
    if(!traw)goto done;
    char *dest=native_path(traw,&t);
    if(!dest||!*dest){errno=ENOENT;goto done;}
    int symbolic=(action&1)!=0;
    const char *test=dest;
    if(symbolic&&*dest!='/') {
        const char *slash=strrchr(p,'/');
        if(slash)Tcl_DStringAppend(&check,p,(int)(slash-p+1));
        Tcl_DStringAppend(&check,dest,-1);test=check.string;
    }
    if(stat(test,&st)!=0)goto done;
    if(lstat(p,&st)==0){errno=EEXIST;goto done;}
    if(errno!=ENOENT) goto done;
    if(symbolic) {
        /* Native translation collapses repeated separators and expands tilde. */
        const char *linktext=dest;
        if(symlink(linktext,p)==0)result=target;
    } else if(action&2) {
        if(link(dest,p)==0)result=target;
    } else errno=ENODEV;
done:
    {int err=errno;Tcl_DStringFree(&check);Tcl_DStringFree(&t);Tcl_DStringFree(&d);errno=err;}
    return result;
}
#undef TKGPU_LINK_API
