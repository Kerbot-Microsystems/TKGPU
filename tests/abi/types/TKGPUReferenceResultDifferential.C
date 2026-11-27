/* Same public result operations on stock Tcl, isolating the wrappers. */
#include <tcl.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#define TKGPU_RESULT_API(name) candidate_##name
#include "TKGPUReferenceResult.H"
#define CHECK(x) do {if(!(x)){fprintf(stderr,"failure at %d\n",__LINE__);return 1;}}while(0)
static uint32_t state=127;
static uint32_t next(void){state^=state<<13;state^=state>>17;state^=state<<5;return state;}
int main(int argc,char **argv) {
    (void)argc; Tcl_FindExecutable(argv[0]);
    for(int mode=0;mode<2;mode++) {
        Tcl_Interp *i=Tcl_CreateInterp();
        CHECK(i);
        for(int n=0;n<20000;n++) {
            Tcl_SavedResult saved[2]; Tcl_Obj *old[2];
            for(int k=0;k<2;k++) {
                char buf[64]; int len=(int)(next()%64);
                for(int j=0;j<len;j++)buf[j]=(char)('a'+next()%26);
                old[k]=Tcl_NewStringObj(buf,len);
                Tcl_SetObjResult(i,old[k]);int refs=old[k]->refCount;
                if(mode)candidate_SaveResult(i,&saved[k]);else Tcl_SaveResult(i,&saved[k]);
                CHECK(!*Tcl_GetStringResult(i)&&old[k]->refCount==refs);
            }
            Tcl_SetObjResult(i,Tcl_NewStringObj("temporary",-1));
            if(n%2) {
                if(mode)candidate_DiscardResult(&saved[1]);else Tcl_DiscardResult(&saved[1]);
                CHECK(!strcmp(Tcl_GetStringResult(i),"temporary"));
            } else {
                if(mode)candidate_RestoreResult(i,&saved[1]);else Tcl_RestoreResult(i,&saved[1]);
                CHECK(Tcl_GetObjResult(i)==old[1]);
            }
            if(mode)candidate_RestoreResult(i,&saved[0]);else Tcl_RestoreResult(i,&saved[0]);
            CHECK(Tcl_GetObjResult(i)==old[0]);
        }
        Tcl_DeleteInterp(i);
    }
    puts("80000 operations per implementation passed");return 0;
}
