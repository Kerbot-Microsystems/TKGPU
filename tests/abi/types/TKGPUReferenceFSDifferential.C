#include <tcl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#define Tcl_GetPathType candidate_GetPathType
#define Tcl_SplitPath candidate_SplitPath
#define Tcl_JoinPath candidate_JoinPath
#include "TKGPUReferencePaths.H"
#define TKGPU_FS_API(name) candidate_##name
#include "TKGPUReferenceFS.H"
#undef Tcl_GetPathType
#undef Tcl_SplitPath
#undef Tcl_JoinPath
#define CHECK(x) do {if(!(x)){fprintf(stderr,"failure at line %d case %u\n",__LINE__,i);return 1;}}while(0)
static uint32_t state=7195;
static uint32_t next(void){state^=state<<13;state^=state>>17;state^=state<<5;return state;}
static int same(Tcl_Obj *a,Tcl_Obj *b) {
    if(!a||!b) return a==b;
    int an,bn;const char *as=Tcl_GetStringFromObj(a,&an),*bs=Tcl_GetStringFromObj(b,&bn);
    return an==bn&&!memcmp(as,bs,(size_t)an);
}
static int same_list(Tcl_Obj *a,Tcl_Obj *b) {
    if(!a||!b) return a==b;
    int an,bn;Tcl_Obj **av,**bv;
    if(Tcl_ListObjGetElements(NULL,a,&an,&av)!=TCL_OK||Tcl_ListObjGetElements(NULL,b,&bn,&bv)!=TCL_OK||an!=bn)return 0;
    for(int k=0;k<an;k++)if(!same(av[k],bv[k]))return 0;
    return 1;
}
static void release_pair(Tcl_Obj *a,Tcl_Obj *b) {if(a)Tcl_DecrRefCount(a);if(b)Tcl_DecrRefCount(b);}
int main(int argc,char **argv) {
    (void)argc;Tcl_FindExecutable(argv[0]);
    static const char *const parts[]={"","/","//","a",".","..","~","~x","./~x","a/~b","~x//","a//b","\\","\xd0\xb6","\xff","\xc0\x80",NULL};
    for(unsigned i=0;i<50000;i++) {
        char bytes[256];int used=0;unsigned count=next()%16;
        for(unsigned k=0;k<count;k++){
            const char *p=parts[next()%(sizeof parts/sizeof parts[0])];
            if(!p){bytes[used++]=0;continue;}
            int n=(int)strlen(p);memcpy(bytes+used,p,(size_t)n);used+=n;
        }
        Tcl_Obj *o=Tcl_NewStringObj(bytes,used);CHECK(o);Tcl_IncrRefCount(o);
        CHECK(candidate_FSGetPathType(o)==Tcl_FSGetPathType(o));
        int an=-1,bn=-1;Tcl_Obj *a=candidate_FSSplitPath(o,&an),*b=Tcl_FSSplitPath(o,&bn);
        CHECK(a&&b&&an==bn&&a->refCount==0&&b->refCount==0&&same_list(a,b));release_pair(a,b);
        a=candidate_FSPathSeparator(o);b=Tcl_FSPathSeparator(o);
        CHECK(same(a,b)&&(!a||(a->refCount==0&&b->refCount==0)));release_pair(a,b);
        a=candidate_FSFileSystemInfo(o);b=Tcl_FSFileSystemInfo(o);
        CHECK(same(a,b)&&(!a||(a->refCount==0&&b->refCount==0)));release_pair(a,b);
        a=candidate_FSListVolumes();b=Tcl_FSListVolumes();
        CHECK(a&&b&&same_list(a,b)&&a->refCount==0&&b->refCount==0);release_pair(a,b);
        Tcl_DecrRefCount(o);
    }
    for(unsigned i=0;i<50000;i++){
        int count=(int)(next()%7);Tcl_Obj *items[7];
        for(int k=0;k<=count;k++){
            char bytes[128];int used=0;unsigned n=next()%8;
            for(unsigned j=0;j<n;j++){
                const char *p=parts[next()%(sizeof parts/sizeof parts[0])];
                if(!p){bytes[used++]=0;continue;}
                size_t len=strlen(p);memcpy(bytes+used,p,len);used+=(int)len;
            }
            items[k]=Tcl_NewStringObj(bytes,used);CHECK(items[k]);Tcl_IncrRefCount(items[k]);
        }
        Tcl_Obj *list=Tcl_NewListObj(count,items);CHECK(list);Tcl_IncrRefCount(list);
        int n=(int)(next()%(unsigned)(count+2))-1;
        Tcl_Obj *a=candidate_FSJoinPath(list,n),*b=Tcl_FSJoinPath(list,n);
        CHECK(a&&b&&same(a,b)&&a->refCount==b->refCount&&(a->refCount==0||a==b));
        Tcl_IncrRefCount(a);Tcl_DecrRefCount(a);Tcl_IncrRefCount(b);Tcl_DecrRefCount(b);
        a=candidate_FSJoinToPath(items[count],count,items);b=Tcl_FSJoinToPath(items[count],count,items);
        CHECK(a&&b&&same(a,b)&&a->refCount==b->refCount&&(a->refCount==0||a==b));
        Tcl_IncrRefCount(a);Tcl_DecrRefCount(a);Tcl_IncrRefCount(b);Tcl_DecrRefCount(b);
        Tcl_DecrRefCount(list);for(int k=0;k<=count;k++)Tcl_DecrRefCount(items[k]);
    }
    puts("350000 filesystem object comparisons passed");return 0;
}
