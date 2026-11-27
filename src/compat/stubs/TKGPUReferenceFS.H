/* Native Linux path objects; no virtual filesystem registration. */
#ifndef TKGPU_FS_API
#define TKGPU_FS_API(name) Tcl_##name
#endif
Tcl_PathType TKGPU_FS_API(FSGetPathType)(Tcl_Obj *path) {
    return Tcl_GetPathType(Tcl_GetString(path));
}
Tcl_Obj *TKGPU_FS_API(FSSplitPath)(Tcl_Obj *path,int *length) {
    const char *s=Tcl_GetString(path);
    if(!s) return NULL;
    int count=0; const char **parts=NULL;
    Tcl_SplitPath(s,&count,&parts);
    Tcl_Obj *result=Tcl_NewListObj(0,NULL);
    if(!result) { Tcl_Free((char *)parts); return NULL; }
    for(int k=0;k<count;k++) {
        Tcl_Obj *part=Tcl_NewStringObj(parts[k],-1);
        if(!part) goto fail;
        Tcl_IncrRefCount(part);
        int status=Tcl_ListObjAppendElement(NULL,result,part);
        Tcl_DecrRefCount(part);
        if(status!=TCL_OK) goto fail;
    }
    Tcl_Free((char *)parts);
    if(length) *length=count;
    return result;
fail:
    Tcl_Free((char *)parts); Tcl_DecrRefCount(result); return NULL;
}
Tcl_Obj *TKGPU_FS_API(FSPathSeparator)(Tcl_Obj *path) {
    const char *s=Tcl_GetString(path);
    return s&&*s?Tcl_NewStringObj("/",1):NULL;
}
Tcl_Obj *TKGPU_FS_API(FSFileSystemInfo)(Tcl_Obj *path) {
    const char *s=Tcl_GetString(path);
    if(!s||!*s) return NULL;
    Tcl_Obj *name=Tcl_NewStringObj("native",6);
    if(!name) return NULL;
    Tcl_IncrRefCount(name);
    Tcl_Obj *result=Tcl_NewListObj(1,&name);
    Tcl_DecrRefCount(name); return result;
}
Tcl_Obj *TKGPU_FS_API(FSListVolumes)(void) {
    Tcl_Obj *root=Tcl_NewStringObj("/",1);
    if(!root) return NULL;
    Tcl_IncrRefCount(root);
    Tcl_Obj *result=Tcl_NewListObj(1,&root);
    Tcl_DecrRefCount(root); return result;
}
static Tcl_Obj *fs_join_objects(Tcl_Obj *base,int count,Tcl_Obj *const items[]) {
    if(count<0||(count&&!items)||count==INT_MAX) return NULL;
    int total=count+(base!=NULL);
    if(!total) return Tcl_NewObj();
    Tcl_Obj *last=count?items[count-1]:base;
    const char *lastString=Tcl_GetString(last);
    if(total==1 || (lastString&&(*lastString=='/'||*lastString=='~'))) {
        Tcl_Obj *only=last;
        const char *s=Tcl_GetString(only);
        if(!s) return NULL;
        size_t n=strlen(s);
        if(!strstr(s,"//")&&(n<=1||s[n-1]!='/')) return only;
    }
    if((size_t)total>UINT_MAX/sizeof(char *)) return NULL;
    const char **parts=(const char **)Tcl_Alloc((unsigned)((size_t)total*sizeof *parts));
    int at=0;
    if(base) parts[at++]=Tcl_GetString(base);
    for(int k=0;k<count;k++) parts[at++]=Tcl_GetString(items[k]);
    for(int k=0;k<total;k++) if(!parts[k]) {Tcl_Free((char *)parts);return NULL;}
    Tcl_DString joined; Tcl_DStringInit(&joined);
    Tcl_JoinPath(total,parts,&joined);
    Tcl_Free((char *)parts);
    Tcl_Obj *result=Tcl_NewStringObj(joined.string,joined.length);
    Tcl_DStringFree(&joined); return result;
}
Tcl_Obj *TKGPU_FS_API(FSJoinPath)(Tcl_Obj *list,int elements) {
    int count=0; Tcl_Obj **items=NULL;
    if(Tcl_ListObjGetElements(NULL,list,&count,&items)!=TCL_OK) return NULL;
    if(elements<0) elements=count;
    if(elements>count) return NULL;
    return fs_join_objects(NULL,elements,items);
}
Tcl_Obj *TKGPU_FS_API(FSJoinToPath)(Tcl_Obj *base,int count,Tcl_Obj *const items[]) {
    return fs_join_objects(base,count,items);
}
#undef TKGPU_FS_API
