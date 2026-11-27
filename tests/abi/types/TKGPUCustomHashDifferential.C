#include <tcl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define TKGPU_HASH_API(name) candidate_##name
#include "TKGPUReferenceHash.H"
#define CHECK(x) do { if(!(x)) {fprintf(stderr,"failure line %d\n",__LINE__);return 1;} } while(0)
#ifdef TCL_HASH_KEY_DIRECT_COMPARE
#define TEST_DIRECT_COMPARE TCL_HASH_KEY_DIRECT_COMPARE
#else
#define TEST_DIRECT_COMPARE 0
#endif
static uint32_t state=123947;
static uint32_t next(void) {state^=state<<13;state^=state>>17;state^=state<<5;return state;}
static unsigned allocs,frees;
static unsigned key_hash(Tcl_HashTable *t,void *p) {(void)t;return *(unsigned *)p%17;}
static int key_equal(void *p,Tcl_HashEntry *e) {return *(unsigned *)p==(unsigned)e->key.words[0];}
static Tcl_HashEntry *key_alloc(Tcl_HashTable *t,void *p) {(void)t;Tcl_HashEntry *e=(Tcl_HashEntry *)Tcl_Alloc(sizeof *e);e->key.words[0]=(int)*(unsigned *)p;++allocs;return e;}
static void key_free(Tcl_HashEntry *e) {++frees;Tcl_Free((char *)e);}
static unsigned ptr_hash(Tcl_HashTable *t,void *p) {(void)t;return *(unsigned *)p%17;}
static int ptr_equal(void *p,Tcl_HashEntry *e) {return *(unsigned *)p==*(unsigned *)e->key.oneWordValue;}
int main(int argc,char **argv) {
    (void)argc; Tcl_FindExecutable(argv[0]);
    static unsigned keys[512],equivalent[512];
    for(unsigned k=0;k<512;k++) keys[k]=equivalent[k]=k;
    static const Tcl_HashKeyType types[]={
      {TCL_HASH_KEY_TYPE_VERSION,TCL_HASH_KEY_SYSTEM_HASH,key_hash,key_equal,key_alloc,key_free},
      {TCL_HASH_KEY_TYPE_VERSION,TEST_DIRECT_COMPARE|TCL_HASH_KEY_RANDOMIZE_HASH,ptr_hash,ptr_equal,NULL,NULL},
      {TCL_HASH_KEY_TYPE_VERSION,TCL_HASH_KEY_RANDOMIZE_HASH,NULL,NULL,NULL,NULL}
    };
    for(int mode=0;mode<4;mode++) {
      Tcl_HashTable a,b; allocs=frees=0;
      if(mode==3) {candidate_InitObjHashTable(&a);Tcl_InitObjHashTable(&b);}
      else {int kind=mode==0?TCL_CUSTOM_TYPE_KEYS:TCL_CUSTOM_PTR_KEYS;candidate_InitCustomHashTable(&a,kind,&types[mode]);Tcl_InitCustomHashTable(&b,kind,&types[mode]);}
      for(unsigned i=0;i<50000;i++) {
        unsigned id=next()%512; unsigned *k=next()%2?&keys[id]:&equivalent[id];
        void *ka=k,*kb=k;Tcl_Obj *oa=NULL,*ob=NULL;
        if(mode==3){char bin[4]={(char)(id&255),0,(char)(id>>8),'x'};oa=Tcl_NewStringObj(bin,4);ob=Tcl_NewStringObj(bin,4);Tcl_IncrRefCount(oa);Tcl_IncrRefCount(ob);ka=oa;kb=ob;}
        Tcl_HashEntry *ea=candidate_FindHashEntry(&a,ka),*eb=Tcl_FindHashEntry(&b,kb);
        CHECK(!!ea==!!eb);if(ea)CHECK(ea->clientData==eb->clientData);
        switch(next()%3){
        case 0:{int na=-1,nb=-1;ea=candidate_CreateHashEntry(&a,ka,&na);eb=Tcl_CreateHashEntry(&b,kb,&nb);CHECK(ea&&eb&&na==nb);ea->clientData=eb->clientData=(void *)(uintptr_t)(i+1);if(mode==3)CHECK(oa->refCount==ob->refCount&&oa->refCount==1+na);break;}
        case 1:if(ea){candidate_DeleteHashEntry(ea);Tcl_DeleteHashEntry(eb);}break;
        default:break;
        }
        if(mode==3){Tcl_DecrRefCount(oa);Tcl_DecrRefCount(ob);}
        CHECK(a.numEntries==b.numEntries);
      }
      Tcl_HashSearch iter;int count=0;for(Tcl_HashEntry *e=candidate_FirstHashEntry(&a,&iter);e;e=candidate_NextHashEntry(&iter)){++count;candidate_DeleteHashEntry(e);}CHECK(count==b.numEntries&&a.numEntries==0);
      candidate_DeleteHashTable(&a);Tcl_DeleteHashTable(&b);if(mode==0)CHECK(allocs==frees&&allocs>0);
      printf("mode %d: 50000 sequences passed\n",mode);
    }
    puts("200000 custom/object hash sequences passed");return 0;
}
