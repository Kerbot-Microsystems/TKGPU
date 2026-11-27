#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif
#include <sys/stat.h>
#include <tcl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#define Tcl_GetPathType candidate_GetPathType
#define Tcl_SplitPath candidate_SplitPath
#define Tcl_JoinPath candidate_JoinPath
#include "TKGPUReferencePaths.H"
#define TKGPU_NATIVE_API(n) candidate_##n
#include "TKGPUReferenceNativeFS.H"
#undef Tcl_GetPathType
#undef Tcl_SplitPath
#undef Tcl_JoinPath
#define CHECK(x) do {if(!(x)){fprintf(stderr,"failure line %d\n",__LINE__);return 1;}}while(0)
#define SAME_STAT(a,b) ((a).st_dev==(b).st_dev&&(a).st_ino==(b).st_ino&&(a).st_mode==(b).st_mode&&(a).st_nlink==(b).st_nlink&&(a).st_size==(b).st_size&&(a).st_uid==(b).st_uid&&(a).st_gid==(b).st_gid&&(a).st_mtim.tv_sec==(b).st_mtim.tv_sec&&(a).st_mtim.tv_nsec==(b).st_mtim.tv_nsec)
static uint32_t state=731;
static uint32_t next(void){state^=state<<13;state^=state>>17;state^=state<<5;return state;}
int main(int argc,char **argv) {
    (void)argc;Tcl_FindExecutable(argv[0]);
    char root[]="/tmp/tkgpu-native-XXXXXX";CHECK(mkdtemp(root));
    char paths[12][512];
    const char *parts[]={"file","folder","link","broken","missing","file/","folder//","./file","folder/../file","\xd0\xb6","folder/./~root","folder/~root"};
    for(unsigned k=0;k<12;k++)snprintf(paths[k],sizeof paths[k],"%s/%s",root,parts[k]);
    FILE *fp=fopen(paths[0],"wb");CHECK(fp);CHECK(fputs("hello",fp)>=0&&fclose(fp)==0);
    CHECK(!mkdir(paths[1],0700)&&!symlink("file",paths[2])&&!symlink("missing",paths[3]));
    fp=fopen(paths[9],"wb");CHECK(fp&&fclose(fp)==0);
    CHECK(!mkdir(paths[11],0700));
    const char *extra[]={"~","~root","~/a","~notrealuser9847","","/","///","./~foo","~/~root","~/./~x"};
    for(unsigned i=0;i<20000;i++) {
        unsigned k=next()%22;const char *p=k<12?paths[k]:extra[k-12];
        int mode=(int)(next()%8);
        Tcl_DString a,b;
        char *x=candidate_TranslateFileName(NULL,p,&a),*y=Tcl_TranslateFileName(NULL,p,&b);
        CHECK((!x&&!y)||(x&&y&&!strcmp(x,y)));
        Tcl_DStringFree(&a);Tcl_DStringFree(&b);
        Tcl_Obj *o=Tcl_NewStringObj(p,-1);CHECK(o);Tcl_IncrRefCount(o);
        errno=0;int ra=candidate_Access(p,mode),ea=errno;
        errno=0;int rb=Tcl_Access(p,mode);CHECK(ra==rb&&(ra==0||errno==ea));
        errno=0;ra=candidate_FSAccess(o,mode);ea=errno;
        errno=0;rb=Tcl_FSAccess(o,mode);CHECK(ra==rb&&(ra==0||errno==ea));
        struct stat sa,sb;
        errno=0;ra=candidate_Stat(p,&sa);ea=errno;
        errno=0;rb=Tcl_Stat(p,&sb);CHECK(ra==rb&&(ra==0||errno==ea));if(!ra)CHECK(SAME_STAT(sa,sb));
        Tcl_StatBuf fa,fb;
        errno=0;ra=candidate_FSStat(o,&fa);ea=errno;
        errno=0;rb=Tcl_FSStat(o,&fb);CHECK(ra==rb&&(ra==0||errno==ea));if(!ra)CHECK(SAME_STAT(fa,fb));
        errno=0;ra=candidate_FSLstat(o,&fa);ea=errno;
        errno=0;rb=Tcl_FSLstat(o,&fb);CHECK(ra==rb&&(ra==0||errno==ea));if(!ra)CHECK(SAME_STAT(fa,fb));
        Tcl_DecrRefCount(o);
    }
    CHECK(!unlink(paths[0])&&!unlink(paths[2])&&!unlink(paths[3])&&!unlink(paths[9]));
    CHECK(!rmdir(paths[11])&&!rmdir(paths[1])&&!rmdir(root));
    puts("120000 native path and metadata comparisons passed");return 0;
}
