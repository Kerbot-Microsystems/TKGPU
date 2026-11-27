/* Remaining Tcl 8.6 VFS / Filesystem slots (Linux reference headers). */
#include <dirent.h>
#include <unistd.h>

typedef struct vfs_node {
    const Tcl_Filesystem *fs;
    ClientData data;
    struct vfs_node *next;
} vfs_node;

static Tcl_Mutex vfs_mutex;
static vfs_node *vfs_list;
static unsigned long vfs_epoch;
static const Tcl_Filesystem native_vfs_sentinel={
    .typeName="native",
    .structureLength=sizeof(Tcl_Filesystem),
    .version=TCL_FILESYSTEM_VERSION_1
};

int Tcl_FSRegister(ClientData clientData,const Tcl_Filesystem *fsPtr) {
    if(!fsPtr) return TCL_ERROR;
    Tcl_MutexLock(&vfs_mutex);
    for(vfs_node *n=vfs_list;n;n=n->next) {
        if(n->fs==fsPtr) { n->data=clientData; Tcl_MutexUnlock(&vfs_mutex); return TCL_OK; }
    }
    vfs_node *n=(vfs_node *)Tcl_Alloc((unsigned)sizeof *n);
    n->fs=fsPtr; n->data=clientData; n->next=vfs_list; vfs_list=n;
    ++vfs_epoch;
    Tcl_MutexUnlock(&vfs_mutex);
    return TCL_OK;
}

int Tcl_FSUnregister(const Tcl_Filesystem *fsPtr) {
    if(!fsPtr) return TCL_ERROR;
    Tcl_MutexLock(&vfs_mutex);
    vfs_node **link=&vfs_list;
    while(*link) {
        vfs_node *n=*link;
        if(n->fs==fsPtr) {
            *link=n->next;
            Tcl_Free((char *)n);
            ++vfs_epoch;
            Tcl_MutexUnlock(&vfs_mutex);
            return TCL_OK;
        }
        link=&n->next;
    }
    Tcl_MutexUnlock(&vfs_mutex);
    return TCL_ERROR;
}

ClientData Tcl_FSData(const Tcl_Filesystem *fsPtr) {
    if(!fsPtr) return NULL;
    Tcl_MutexLock(&vfs_mutex);
    ClientData out=NULL;
    for(vfs_node *n=vfs_list;n;n=n->next) {
        if(n->fs==fsPtr) { out=n->data; break; }
    }
    Tcl_MutexUnlock(&vfs_mutex);
    return out;
}

void Tcl_FSMountsChanged(const Tcl_Filesystem *fsPtr) {
    (void)fsPtr;
    Tcl_MutexLock(&vfs_mutex);
    ++vfs_epoch;
    Tcl_MutexUnlock(&vfs_mutex);
}

CONST86 Tcl_Filesystem *Tcl_FSGetFileSystemForPath(Tcl_Obj *pathPtr) {
    if(!pathPtr) return NULL;
    Tcl_MutexLock(&vfs_mutex);
    for(vfs_node *n=vfs_list;n;n=n->next) {
        if(n->fs->pathInFilesystemProc&&n->fs->pathInFilesystemProc(pathPtr,NULL)==TCL_OK) {
            const Tcl_Filesystem *found=n->fs;
            Tcl_MutexUnlock(&vfs_mutex);
            return found;
        }
    }
    Tcl_MutexUnlock(&vfs_mutex);
    return &native_vfs_sentinel;
}

ClientData Tcl_FSGetInternalRep(Tcl_Obj *pathPtr,const Tcl_Filesystem *fsPtr) {
    (void)fsPtr;
    return pathPtr?(ClientData)Tcl_GetString(pathPtr):NULL;
}

const void *Tcl_FSGetNativePath(Tcl_Obj *pathPtr) {
    return pathPtr?(const void *)Tcl_GetString(pathPtr):NULL;
}

Tcl_Obj *Tcl_FSNewNativePath(const Tcl_Filesystem *fromFilesystem,ClientData clientData) {
    (void)fromFilesystem;
    if(!clientData) return NULL;
    return Tcl_NewStringObj((const char *)clientData,-1);
}

int Tcl_FSConvertToPathType(Tcl_Interp *i,Tcl_Obj *pathPtr) {
    (void)i;
    return pathPtr?TCL_OK:TCL_ERROR;
}

Tcl_Obj *Tcl_FSGetTranslatedPath(Tcl_Interp *i,Tcl_Obj *pathPtr) {
    if(!pathPtr) return NULL;
    Tcl_DString ds;
    const char *t=Tcl_TranslateFileName(i,Tcl_GetString(pathPtr),&ds);
    if(!t) return NULL;
    Tcl_Obj *res=Tcl_NewStringObj(t,-1);
    Tcl_DStringFree(&ds);
    return res;
}

const char *Tcl_FSGetTranslatedStringPath(Tcl_Interp *i,Tcl_Obj *pathPtr) {
    if(!pathPtr) return NULL;
    Tcl_DString ds;
    const char *t=Tcl_TranslateFileName(i,Tcl_GetString(pathPtr),&ds);
    if(!t) return NULL;
    size_t n=strlen(t);
    char *copy=Tcl_Alloc((unsigned)(n+1));
    memcpy(copy,t,n+1);
    Tcl_DStringFree(&ds);
    return copy;
}

Tcl_Obj *Tcl_FSGetNormalizedPath(Tcl_Interp *i,Tcl_Obj *pathPtr) {
    if(!pathPtr) return NULL;
    Tcl_DString ds;
    const char *t=Tcl_TranslateFileName(i,Tcl_GetString(pathPtr),&ds);
    if(!t) return NULL;
    char resolved[4096];
    const char *out=realpath(t,resolved);
    Tcl_Obj *res=NULL;
    if(out) {
        res=Tcl_NewStringObj(out,-1);
    } else if(t[0]=='/') {
        res=Tcl_NewStringObj(t,-1);
    } else {
        Tcl_DString cwd;
        if(Tcl_GetCwd(i,&cwd)) {
            res=Tcl_ObjPrintf("%s/%s",Tcl_DStringValue(&cwd),t);
            Tcl_DStringFree(&cwd);
        } else {
            res=Tcl_NewStringObj(t,-1);
        }
    }
    Tcl_DStringFree(&ds);
    return res;
}

int Tcl_FSEqualPaths(Tcl_Obj *firstPtr,Tcl_Obj *secondPtr) {
    if(!firstPtr||!secondPtr) return 0;
    if(firstPtr==secondPtr) return 1;
    const char *a=Tcl_GetString(firstPtr),*b=Tcl_GetString(secondPtr);
    return a&&b&&strcmp(a,b)==0;
}

int Tcl_FSCopyFile(Tcl_Obj *srcPathPtr,Tcl_Obj *destPathPtr) {
    if(!srcPathPtr||!destPathPtr) return -1;
    FILE *in=fopen(Tcl_GetString(srcPathPtr),"rb");
    if(!in) return -1;
    FILE *out=fopen(Tcl_GetString(destPathPtr),"wb");
    if(!out) { fclose(in); return -1; }
    char buf[4096];
    size_t n;
    int err=0;
    while((n=fread(buf,1,sizeof buf,in))>0) {
        if(fwrite(buf,1,n,out)!=n) { err=1; break; }
    }
    if(ferror(in)) err=1;
    fclose(in);
    if(fclose(out)!=0) err=1;
    return err?-1:0;
}

int Tcl_FSCopyDirectory(Tcl_Obj *srcPathPtr,Tcl_Obj *destPathPtr,Tcl_Obj **errorPtr) {
    if(!srcPathPtr||!destPathPtr) return -1;
    if(Tcl_FSCreateDirectory(destPathPtr)!=0) {
        if(errorPtr) *errorPtr=destPathPtr;
        return -1;
    }
    const char *src=Tcl_GetString(srcPathPtr),*dst=Tcl_GetString(destPathPtr);
    DIR *d=opendir(src);
    if(!d) { if(errorPtr) *errorPtr=srcPathPtr; return -1; }
    struct dirent *ent;
    int rc=0;
    while((ent=readdir(d))!=NULL) {
        if(!strcmp(ent->d_name,".")||!strcmp(ent->d_name,"..")) continue;
        Tcl_Obj *s=Tcl_ObjPrintf("%s/%s",src,ent->d_name);
        Tcl_Obj *t=Tcl_ObjPrintf("%s/%s",dst,ent->d_name);
        Tcl_IncrRefCount(s); Tcl_IncrRefCount(t);
        struct stat st;
        if(lstat(Tcl_GetString(s),&st)==0&&S_ISDIR(st.st_mode)) {
            if(Tcl_FSCopyDirectory(s,t,errorPtr)!=0) rc=-1;
        } else {
            if(Tcl_FSCopyFile(s,t)!=0) { if(errorPtr) *errorPtr=s; rc=-1; }
        }
        Tcl_DecrRefCount(s); Tcl_DecrRefCount(t);
        if(rc!=0) break;
    }
    closedir(d);
    return rc;
}

int Tcl_FSRemoveDirectory(Tcl_Obj *pathPtr,int recursive,Tcl_Obj **errorPtr) {
    if(!pathPtr) return -1;
    const char *path=Tcl_GetString(pathPtr);
    if(recursive) {
        DIR *d=opendir(path);
        if(d) {
            struct dirent *ent;
            while((ent=readdir(d))!=NULL) {
                if(!strcmp(ent->d_name,".")||!strcmp(ent->d_name,"..")) continue;
                Tcl_Obj *child=Tcl_ObjPrintf("%s/%s",path,ent->d_name);
                Tcl_IncrRefCount(child);
                struct stat st;
                if(lstat(Tcl_GetString(child),&st)==0&&S_ISDIR(st.st_mode)) {
                    (void)Tcl_FSRemoveDirectory(child,1,errorPtr);
                } else {
                    (void)unlink(Tcl_GetString(child));
                }
                Tcl_DecrRefCount(child);
            }
            closedir(d);
        }
    }
    if(rmdir(path)!=0) {
        if(errorPtr) *errorPtr=pathPtr;
        return -1;
    }
    return 0;
}

int Tcl_FSMatchInDirectory(Tcl_Interp *i,Tcl_Obj *result,Tcl_Obj *pathPtr,
                           const char *pattern,Tcl_GlobTypeData *types) {
    (void)types;
    if(!result||!pathPtr) return TCL_ERROR;
    const char *dir=Tcl_GetString(pathPtr);
    if(!pattern||!*pattern) {
        if(access(dir,F_OK)==0) Tcl_ListObjAppendElement(i,result,pathPtr);
        return TCL_OK;
    }
    DIR *d=opendir(dir&&*dir?dir:".");
    if(!d) return TCL_OK;
    struct dirent *ent;
    while((ent=readdir(d))!=NULL) {
        if(!strcmp(ent->d_name,".")||!strcmp(ent->d_name,"..")) continue;
        if(Tcl_StringMatch(ent->d_name,pattern)) {
            Tcl_Obj *full=(dir&&*dir)?Tcl_ObjPrintf("%s/%s",dir,ent->d_name):Tcl_NewStringObj(ent->d_name,-1);
            Tcl_ListObjAppendElement(i,result,full);
        }
    }
    closedir(d);
    return TCL_OK;
}

int Tcl_FSEvalFileEx(Tcl_Interp *i,Tcl_Obj *fileName,const char *encodingName) {
    (void)encodingName;
    if(!i||!fileName) return TCL_ERROR;
    return Tcl_EvalFile(i,Tcl_GetString(fileName));
}

int Tcl_FSEvalFile(Tcl_Interp *i,Tcl_Obj *fileName) {
    return Tcl_FSEvalFileEx(i,fileName,NULL);
}

Tcl_Channel Tcl_FSOpenFileChannel(Tcl_Interp *i,Tcl_Obj *pathPtr,const char *modeString,int permissions) {
    if(!pathPtr) return NULL;
    return Tcl_OpenFileChannel(i,Tcl_GetString(pathPtr),modeString,permissions);
}
