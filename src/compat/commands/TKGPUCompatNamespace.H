/* Public namespace prefix, global namespace only. All state belongs to an
   interpreter; the core frees it after its command/delete hooks. */
#include <stdlib.h>
#include <string.h>
#ifndef NS_API
#define NS_API(n) Tcl_##n
Tcl_Namespace *tg_compat_find_child_namespace(Tcl_Interp *,const char *,Tcl_Namespace *);
#define TKGPU_NS_HAS_CHILDREN 1
#endif
Tcl_Namespace *NS_API(GetGlobalNamespace)(Tcl_Interp *i) {
    if(!i)return NULL;
    if(!NS_GLOBAL(i)) {
        Tcl_Namespace *ns=calloc(1,sizeof *ns+4);
        if(!ns)Tcl_Panic("namespace allocation failed");
        ns->name=(char *)(ns+1);ns->fullName=ns->name+1;
        memcpy(ns->fullName,"::",3);NS_GLOBAL(i)=ns;
    }
    return NS_GLOBAL(i);
}
Tcl_Namespace *NS_API(GetCurrentNamespace)(Tcl_Interp *i) {return NS_API(GetGlobalNamespace)(i);}
Tcl_Namespace *NS_API(FindNamespace)(Tcl_Interp *i,const char *name,Tcl_Namespace *context,int flags) {
    if(!i||!name)return NULL;
    Tcl_Namespace *global=NS_API(GetGlobalNamespace)(i);
    int absolute=name[0]==':'&&name[1]==':';
    if(absolute) {const char *p=name;while(*p==':')++p;if(!*p)return global;}
    else if(!*name&&(!context||context==global||(flags&1)))return global;
#ifdef TKGPU_NS_HAS_CHILDREN
    {
        Tcl_Namespace *child=tg_compat_find_child_namespace(i,name,context);
        if(child) return child;
    }
#endif
    if(flags&512) {Tcl_ResetResult(i);Tcl_AppendResult(i,"unknown namespace \"",name,"\"",(char *)NULL);}
    return NULL;
}
static int ns_is_global(Tcl_Interp *i,Tcl_Namespace *ns) {return i&&(!ns||ns==NS_API(GetGlobalNamespace)(i));}
int NS_API(Export)(Tcl_Interp *i,Tcl_Namespace *ns,const char *pattern,int reset) {
    if(!ns_is_global(i,ns))return 1;
    if(reset&&NS_EXPORTS(i)){Tcl_DecrRefCount(NS_EXPORTS(i));NS_EXPORTS(i)=NULL;}
    if(!pattern)return 1;
    if(strstr(pattern,"::")) {
        Tcl_ResetResult(i);Tcl_AppendResult(i,"invalid export pattern \"",pattern,"\": pattern can't specify a namespace",(char *)NULL);return 1;
    }
    if(!NS_EXPORTS(i)) {NS_EXPORTS(i)=Tcl_NewListObj(0,NULL);if(!NS_EXPORTS(i))return 1;Tcl_IncrRefCount(NS_EXPORTS(i));}
    int n;Tcl_Obj **values;
    if(Tcl_ListObjGetElements(i,NS_EXPORTS(i),&n,&values))return 1;
    for(int k=0;k<n;k++)if(!strcmp(Tcl_GetString(values[k]),pattern))return 0;
    Tcl_Obj *o=Tcl_NewStringObj(pattern,-1);if(!o)return 1;Tcl_IncrRefCount(o);
    int rc=Tcl_ListObjAppendElement(i,NS_EXPORTS(i),o);Tcl_DecrRefCount(o);return rc;
}
int NS_API(AppendExportList)(Tcl_Interp *i,Tcl_Namespace *ns,Tcl_Obj *out) {
    if(!ns_is_global(i,ns)||!out)return 1;
    return NS_EXPORTS(i)?Tcl_ListObjAppendList(i,out,NS_EXPORTS(i)):0;
}
#undef NS_API
