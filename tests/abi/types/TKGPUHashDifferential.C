/* Independent implementation and stock Tcl run side by side. */
#include <tcl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#define TKGPU_HASH_API(name) candidate_##name
#include "TKGPUReferenceHash.H"
#define CHECK(x) do { if(!(x)) {fprintf(stderr,"failed line %d\n",__LINE__); return 1;} } while(0)
static uint32_t state=7613;
static uint32_t next(void){state^=state<<13;state^=state>>17;state^=state<<5;return state;}
static unsigned scan(Tcl_HashTable *t,int mode){Tcl_HashSearch search; unsigned count=0; for(Tcl_HashEntry *e=mode?candidate_FirstHashEntry(t,&search):Tcl_FirstHashEntry(t,&search);e;e=mode?candidate_NextHashEntry(&search):Tcl_NextHashEntry(&search)) {++count; if(count>8192) abort();}return count;}
int main(void){
 for(int type=0;type<3;type++){
  Tcl_HashTable a,b;candidate_InitHashTable(&a,type==2?3:type);Tcl_InitHashTable(&b,type==2?3:type);
  for(unsigned i=0;i<50000;i++){
   unsigned id=next()%4096; char str[96]; snprintf(str,sizeof str,"key-%u-%08x",id,id*13u); int words[3]={(int)id,(int)(id*13u),-(int)id};
   const void *key=type==0?(void*)str:type==1?(void*)(uintptr_t)id:(void*)words;
   Tcl_HashEntry *ea=candidate_FindHashEntry(&a,key),*eb=Tcl_FindHashEntry(&b,key);
   CHECK(!!ea==!!eb);if(ea) CHECK(ea->clientData==eb->clientData);
   switch(next()%3){case 0:{int na=-1,nb=-1;ea=candidate_CreateHashEntry(&a,key,&na);eb=Tcl_CreateHashEntry(&b,key,&nb);CHECK(ea&&eb&&na==nb);ea->clientData=eb->clientData=(void*)(uintptr_t)(i+1);break;}case 1:if(ea){candidate_DeleteHashEntry(ea);Tcl_DeleteHashEntry(eb);}break;default:break;}
   CHECK(a.numEntries==b.numEntries);if(i%257==0)CHECK(scan(&a,1)==scan(&b,0));
  }
  CHECK(scan(&a,1)==(unsigned)a.numEntries);CHECK(scan(&b,0)==(unsigned)b.numEntries);
  Tcl_HashSearch search;for(Tcl_HashEntry *e=candidate_FirstHashEntry(&a,&search);e;e=candidate_NextHashEntry(&search))candidate_DeleteHashEntry(e);
  CHECK(a.numEntries==0);candidate_DeleteHashTable(&a);Tcl_DeleteHashTable(&b);
  printf("key type %d: 50000 operations passed\n",type);
 }
 puts("150000 randomized operation sequences passed");return 0;
}
