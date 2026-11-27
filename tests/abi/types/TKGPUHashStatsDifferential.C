#include <tcl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#define TKGPU_HASH_API(name) candidate_##name
#include "TKGPUReferenceHash.H"
#define TKGPU_HASH_STATS_API candidate_HashStats
#include "TKGPUReferenceHashStats.H"
static uint32_t state=101;
static uint32_t next(void){state^=state<<13;state^=state>>17;state^=state<<5;return state;}
int main(void){
    for(int mode=0;mode<2;mode++) {
        Tcl_HashTable t; if(mode)candidate_InitHashTable(&t,0);else Tcl_InitHashTable(&t,0);
        for(int i=0;i<10000;i++){
            char key[40]; snprintf(key,sizeof key,"key-%u",next()%256);
            int fresh;
            if(next()%2) { if(mode)candidate_CreateHashEntry(&t,key,&fresh);else Tcl_CreateHashEntry(&t,key,&fresh); }
            else {Tcl_HashEntry *e=mode?candidate_FindHashEntry(&t,key):Tcl_FindHashEntry(&t,key);if(e){if(mode)candidate_DeleteHashEntry(e);else Tcl_DeleteHashEntry(e);}}
            char *a=candidate_HashStats(&t),*b=Tcl_HashStats(&t);
            if(strcmp(a,b)){fprintf(stderr,"mismatch at %d %d:\n[%s]\n[%s]\n",mode,i,a,b);return 1;}
            Tcl_Free(a);Tcl_Free(b);
        }
        if(mode)candidate_DeleteHashTable(&t);else Tcl_DeleteHashTable(&t);
    }
    puts("20000 same-table statistics comparisons passed"); return 0;
}
