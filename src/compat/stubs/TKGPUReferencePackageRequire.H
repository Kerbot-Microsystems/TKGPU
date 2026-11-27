/* Explicitly provided packages only; bootstrap is handled by the caller. */
#ifndef PKG_API
#define PKG_API(n) Tcl_##n
#endif
static const char *pkg_require_one(Tcl_Interp *i,const char *name,const char *version,int exact,void *data) {
    if(!i||!name)return NULL;
    tg_pkg_entry *p=pkg_find(pkg_registry(i,0),name);
    int as_exact;size_t n;int match=pkg_satisfies(i,p?p->version:"0",version,exact,&as_exact,&n);
    if(match<0)return NULL;
    if(!p||!match) {
        Tcl_DString d;Tcl_DStringInit(&d);
        if(!p){Tcl_DStringAppend(&d,"can't find package ",-1);Tcl_DStringAppend(&d,name,-1);}
        else {Tcl_DStringAppend(&d,"version conflict for package \"",-1);Tcl_DStringAppend(&d,name,-1);Tcl_DStringAppend(&d,"\": have ",-1);Tcl_DStringAppend(&d,p->version,-1);Tcl_DStringAppend(&d,", need",-1);}
        if(version){Tcl_DStringAppend(&d,as_exact?" exactly ":" ",-1);Tcl_DStringAppend(&d,version,(int)n);}
        Tcl_SetObjResult(i,Tcl_NewStringObj(d.string,d.length));Tcl_DStringFree(&d);return NULL;
    }
    if(data)*(const void **)data=p->data;
    Tcl_ResetResult(i);return p->version;
}
int PKG_API(PkgRequireProc)(Tcl_Interp *i,const char *name,int objc,Tcl_Obj *const objv[],void *data) {
    if(!i||!name||objc<0||(objc&&!objv))return 1;
    tg_pkg_entry *p=pkg_find(pkg_registry(i,0),name);
    Tcl_DString d;Tcl_DStringInit(&d);int matched=objc==0;
    for(int j=0;j<objc;j++) {
        const char *v=Tcl_GetString(objv[j]);int ex;size_t n;
        int rc=pkg_satisfies(i,p?p->version:"0",v,0,&ex,&n);
        if(rc<0){Tcl_DStringFree(&d);return 1;}
        if(rc)matched=1;
        Tcl_DStringAppend(&d,ex?" exactly ":" ",-1);Tcl_DStringAppend(&d,v,(int)n);
    }
    if(p&&matched) {
        if(data)*(const void **)data=p->data;
        Tcl_SetObjResult(i,Tcl_NewStringObj(p->version,-1));Tcl_DStringFree(&d);return 0;
    }
    Tcl_ResetResult(i);
    if(!p)Tcl_AppendResult(i,"can't find package ",name,d.string,(char *)NULL);
    else Tcl_AppendResult(i,"version conflict for package \"",name,"\": have ",p->version,", need",d.string,(char *)NULL);
    Tcl_DStringFree(&d);return 1;
}
#undef PKG_API
