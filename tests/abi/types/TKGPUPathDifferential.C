/* Compile the independent path code with renamed symbols, alongside stock Tcl. */
#include <tcl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define Tcl_GetPathType candidate_GetPathType
#define Tcl_SplitPath candidate_SplitPath
#define Tcl_JoinPath candidate_JoinPath
#include "TKGPUReferencePaths.H"
#undef Tcl_GetPathType
#undef Tcl_SplitPath
#undef Tcl_JoinPath
static uint32_t state=7613;
static uint32_t random_word(void) {
    state^=state<<13; state^=state>>17; state^=state<<5; return state;
}
int main(int argc,char **argv) {
    (void)argc; Tcl_FindExecutable(argv[0]);
    static const char *const parts[]={"","/","//","a",".","..","~","~x","./~x","a/~b","~x//","a//b","\\","\xd0\xb6","\xff"};
    for(unsigned i=0;i<50000;i++) {
        char path[256]=""; size_t used=0;
        unsigned count=random_word()%16;
        for(unsigned k=0;k<count;k++) {
            const char *p=parts[random_word()%(sizeof parts/sizeof parts[0])];
            size_t n=strlen(p); memcpy(path+used,p,n+1); used+=n;
        }
        int na=-1,nb=-1; const char **a=NULL,**b=NULL;
        int ok=candidate_GetPathType(path)==Tcl_GetPathType(path);
        candidate_SplitPath(path,&na,&a); Tcl_SplitPath(path,&nb,&b);
        ok=ok&&na==nb&&a&&b&&a[na]==NULL&&b[nb]==NULL;
        if(ok) for(int k=0;k<na;k++) if(strcmp(a[k],b[k])) { ok=0; break; }
        Tcl_Free((char *)a); Tcl_Free((char *)b);
        if(!ok) { fprintf(stderr,"path comparison failed at case %u\n",i); return 1; }
    }
    for(unsigned i=0;i<100000;i++) {
        const char *args[8]; char items[8][128];
        unsigned count=random_word()%8;
        for(unsigned k=0;k<count;k++) {
            size_t used=0; items[k][0]=0;
            unsigned n=random_word()%12;
            for(unsigned j=0;j<n;j++) {
                const char *part=parts[random_word()%(sizeof parts/sizeof parts[0])];
                size_t len=strlen(part); memcpy(items[k]+used,part,len+1); used+=len;
            }
            args[k]=items[k];
        }
        Tcl_DString a,b; Tcl_DStringInit(&a); Tcl_DStringInit(&b);
        const char *prefix=parts[random_word()%(sizeof parts/sizeof parts[0])];
        Tcl_DStringAppend(&a,prefix,-1); Tcl_DStringAppend(&b,prefix,-1);
        char *x=candidate_JoinPath((int)count,args,&a);
        char *y=Tcl_JoinPath((int)count,args,&b);
        int ok=x==Tcl_DStringValue(&a)&&y==Tcl_DStringValue(&b)&&
            Tcl_DStringLength(&a)==Tcl_DStringLength(&b)&&!strcmp(x,y);
        Tcl_DStringFree(&a); Tcl_DStringFree(&b);
        if(!ok) { fprintf(stderr,"JoinPath mismatch at %u\n",i); return 1; }
    }
    puts("200000 path comparisons passed");
    return 0;
}
