/* Independent chained hash tables using the public Tcl 8.6 record layout.
   Built-in and custom keys share bucket ownership and entry lifetimes. */
#include <stdlib.h>
#ifdef TCL_HASH_KEY_DIRECT_COMPARE
#define TG_HASH_DIRECT_COMPARE TCL_HASH_KEY_DIRECT_COMPARE
#else
/* Older 8.6 headers have no pointer fast-path flag. The comparison
   callback remains authoritative on those targets. */
#define TG_HASH_DIRECT_COMPARE 0
#endif
#ifndef TKGPU_HASH_API
#define TKGPU_HASH_API(name) Tcl_##name
#endif
static size_t hash_key_bytes(Tcl_HashTable *t,const void *key) {
    if(t->keyType==TCL_STRING_KEYS) return strlen((const char *)key)+1;
    if(t->keyType==TCL_ONE_WORD_KEYS) return sizeof(void *);
    return (size_t)t->keyType*sizeof(int);
}
static int hash_custom(const Tcl_HashTable *t) { return t->keyType<0; }
static uint32_t hash_mix(uint32_t h) {
    h^=h>>16; h*=0x85ebca6bu; h^=h>>13; return h;
}
static uint32_t hash_key(Tcl_HashTable *t,const void *key) {
    if(hash_custom(t)) {
        const Tcl_HashKeyType *type=t->typePtr;
        uint32_t h=type->hashKeyProc?type->hashKeyProc(t,(void *)key):(uint32_t)(uintptr_t)key;
        return type->flags&TCL_HASH_KEY_RANDOMIZE_HASH?hash_mix(h):h;
    }
    uintptr_t ptr=(uintptr_t)key;
    const unsigned char *p=t->keyType==TCL_ONE_WORD_KEYS?(const unsigned char *)&ptr:key;
    size_t n=hash_key_bytes(t,key);
    uint32_t h=2166136261u;
    while(n--) { h^=*p++; h*=16777619u; }
    return hash_mix(h);
}
#if !TCL_HASH_KEY_STORE_HASH
static const void *hash_entry_key(Tcl_HashEntry *e) {
    return (e->tablePtr->keyType==TCL_ONE_WORD_KEYS||e->tablePtr->keyType==TCL_CUSTOM_PTR_KEYS)?(const void *)e->key.oneWordValue:(const void *)&e->key;
}
#endif
static int hash_equal(Tcl_HashTable *t,Tcl_HashEntry *e,const void *key) {
    if(hash_custom(t)) {
        const Tcl_HashKeyType *type=t->typePtr;
        if((type->flags&TG_HASH_DIRECT_COMPARE)&&e->key.oneWordValue==key) return 1;
        return type->compareKeysProc?type->compareKeysProc((void *)key,e):e->key.oneWordValue==key;
    }
    if(t->keyType==TCL_ONE_WORD_KEYS) return e->key.oneWordValue==key;
    if(t->keyType==TCL_STRING_KEYS) return !strcmp(e->key.string,key);
    return !memcmp(e->key.words,key,hash_key_bytes(t,key));
}
static Tcl_HashEntry *hash_find(Tcl_HashTable *t,const char *key) {
    if(!t||!t->buckets) return NULL;
    unsigned b=hash_key(t,key)&(unsigned)t->mask;
    for(Tcl_HashEntry *e=t->buckets[b];e;e=e->nextPtr)
        if(hash_equal(t,e,key)) return e;
    return NULL;
}
static void hash_link(Tcl_HashTable *t,Tcl_HashEntry *e,uint32_t h) {
    Tcl_HashEntry **b=&t->buckets[h&(unsigned)t->mask];
#if TCL_HASH_KEY_STORE_HASH
    e->hash=(void *)(uintptr_t)h;
#else
    e->bucketPtr=b;
#endif
    e->nextPtr=*b; *b=e;
}
static int hash_system(const Tcl_HashTable *t) {
    return hash_custom(t)&&(t->typePtr->flags&TCL_HASH_KEY_SYSTEM_HASH);
}
static void hash_free_buckets(Tcl_HashTable *t,Tcl_HashEntry **p) {
    if(hash_system(t)) free(p); else Tcl_Free((char *)p);
}
static uint32_t hash_stored(Tcl_HashEntry *e) {
#if TCL_HASH_KEY_STORE_HASH
    return (uint32_t)(uintptr_t)e->hash;
#else
    return hash_key(e->tablePtr,hash_entry_key(e));
#endif
}
static void hash_grow(Tcl_HashTable *t) {
    if(t->numBuckets>INT_MAX/6 || (size_t)t->numBuckets>UINT_MAX/(2*sizeof *t->buckets)) {
        t->rebuildSize=INT_MAX; return;
    }
    int oldCount=t->numBuckets,newCount=oldCount*2;
    Tcl_HashEntry **old=t->buckets;
    size_t bytes=(size_t)newCount*sizeof *t->buckets;
    t->buckets=hash_system(t)?malloc(bytes):(Tcl_HashEntry **)Tcl_Alloc((unsigned)bytes);
    if(!t->buckets) Tcl_Panic("hash bucket allocation failed");
    memset(t->buckets,0,(size_t)newCount*sizeof *t->buckets);
    t->numBuckets=newCount; t->mask=newCount-1; t->rebuildSize=newCount*3;
    for(int k=0;k<oldCount;k++) {
        Tcl_HashEntry *e=old[k];
        while(e) { Tcl_HashEntry *next=e->nextPtr; hash_link(t,e,hash_stored(e)); e=next; }
    }
    if(old!=t->staticBuckets) hash_free_buckets(t,old);
}
static Tcl_HashEntry *hash_create(Tcl_HashTable *t,const char *key,int *isNew) {
    if(!t||!t->buckets||!isNew) Tcl_Panic("invalid hash create");
    Tcl_HashEntry *e=hash_find(t,key);
    if(e) { *isNew=0; return e; }
    if(t->numEntries==INT_MAX) Tcl_Panic("hash entry overflow");
    uint32_t h=hash_key(t,key);
    if(hash_custom(t)&&t->typePtr->allocEntryProc) {
        e=t->typePtr->allocEntryProc(t,(void *)key);
        if(!e) Tcl_Panic("hash entry allocation failed");
    } else {
        size_t n=hash_custom(t)?sizeof(void *):hash_key_bytes(t,key),base=offsetof(Tcl_HashEntry,key);
        if(n>UINT_MAX-base) Tcl_Panic("hash entry overflow");
        size_t bytes=base+n; if(bytes<sizeof *e) bytes=sizeof *e;
        e=(Tcl_HashEntry *)Tcl_Alloc((unsigned)bytes); memset(e,0,bytes);
        if(hash_custom(t)||t->keyType==TCL_ONE_WORD_KEYS) e->key.oneWordValue=(char *)key;
        else memcpy(&e->key,key,n);
    }
    e->tablePtr=t; e->clientData=NULL;
    hash_link(t,e,h); ++t->numEntries; *isNew=1;
    if(t->numEntries>=t->rebuildSize) hash_grow(t);
    return e;
}
void TKGPU_HASH_API(InitCustomHashTable)(Tcl_HashTable *t,int keyType,const Tcl_HashKeyType *type) {
    if(!t||(keyType<0&&(keyType!=TCL_CUSTOM_TYPE_KEYS&&keyType!=TCL_CUSTOM_PTR_KEYS))||
       (keyType<0&&(!type||type->version!=TCL_HASH_KEY_TYPE_VERSION))||
       (keyType>=0&&(size_t)keyType>UINT_MAX/sizeof(int))) Tcl_Panic("unsupported hash key type");
    memset(t,0,sizeof *t); t->buckets=t->staticBuckets;
    t->numBuckets=TCL_SMALL_HASH_TABLE; t->mask=t->numBuckets-1;
    t->rebuildSize=t->numBuckets*3; t->keyType=keyType;
    t->findProc=hash_find; t->createProc=hash_create; t->typePtr=keyType<0?type:NULL;
}
void TKGPU_HASH_API(InitHashTable)(Tcl_HashTable *t,int keyType) {
    TKGPU_HASH_API(InitCustomHashTable)(t,keyType,NULL);
}
static unsigned object_hash(Tcl_HashTable *t,void *key) {
    (void)t; int n=0; const unsigned char *p=(const unsigned char *)Tcl_GetStringFromObj(key,&n);
    uint32_t h=2166136261u; while(n-->0) {h^=*p++; h*=16777619u;} return hash_mix(h);
}
static int object_equal(void *key,Tcl_HashEntry *e) {
    int a=0,b=0; const char *p=Tcl_GetStringFromObj(key,&a),*q=Tcl_GetStringFromObj(e->key.objPtr,&b);
    return a==b&&!memcmp(p,q,(size_t)a);
}
static Tcl_HashEntry *object_entry(Tcl_HashTable *t,void *key) {
    (void)t; Tcl_HashEntry *e=(Tcl_HashEntry *)Tcl_Alloc(sizeof *e);
    memset(e,0,sizeof *e); e->key.objPtr=key; Tcl_IncrRefCount(e->key.objPtr); return e;
}
static void object_free(Tcl_HashEntry *e) { Tcl_DecrRefCount(e->key.objPtr); Tcl_Free((char *)e); }
void TKGPU_HASH_API(InitObjHashTable)(Tcl_HashTable *t) {
    static const Tcl_HashKeyType type={TCL_HASH_KEY_TYPE_VERSION,TG_HASH_DIRECT_COMPARE,object_hash,object_equal,object_entry,object_free};
    TKGPU_HASH_API(InitCustomHashTable)(t,TCL_CUSTOM_PTR_KEYS,&type);
}
static void hash_free_entry(Tcl_HashEntry *e) {
    Tcl_HashTable *t=e->tablePtr;
    if(hash_custom(t)&&t->typePtr->freeEntryProc) t->typePtr->freeEntryProc(e);
    else Tcl_Free((char *)e);
}
Tcl_HashEntry *TKGPU_HASH_API(FindHashEntry)(Tcl_HashTable *t,const void *key) {
    return t&&t->findProc?t->findProc(t,key):NULL;
}
Tcl_HashEntry *TKGPU_HASH_API(CreateHashEntry)(Tcl_HashTable *t,const void *key,int *isNew) {
    if(!t||!t->createProc) Tcl_Panic("invalid hash table");
    return t->createProc(t,key,isNew);
}
void TKGPU_HASH_API(DeleteHashEntry)(Tcl_HashEntry *e) {
    if(!e) return;
    Tcl_HashTable *t=e->tablePtr;
    unsigned b=hash_stored(e)&(unsigned)t->mask;
    Tcl_HashEntry **p=&t->buckets[b];
    while(*p&&*p!=e) p=&(*p)->nextPtr;
    if(!*p) Tcl_Panic("hash entry not found");
    *p=e->nextPtr; --t->numEntries; hash_free_entry(e);
}
void TKGPU_HASH_API(DeleteHashTable)(Tcl_HashTable *t) {
    if(!t||!t->buckets) return;
    for(int k=0;k<t->numBuckets;k++) {
        Tcl_HashEntry *e=t->buckets[k];
        while(e) { Tcl_HashEntry *next=e->nextPtr; hash_free_entry(e); e=next; }
    }
    if(t->buckets!=t->staticBuckets) hash_free_buckets(t,t->buckets);
    memset(t,0,sizeof *t);
}
Tcl_HashEntry *TKGPU_HASH_API(NextHashEntry)(Tcl_HashSearch *s) {
    Tcl_HashTable *t=s->tablePtr;
    while(!s->nextEntryPtr) {
        if(!t||s->nextIndex>=t->numBuckets) return NULL;
        s->nextEntryPtr=t->buckets[s->nextIndex++];
    }
    Tcl_HashEntry *e=s->nextEntryPtr; s->nextEntryPtr=e->nextPtr; return e;
}
Tcl_HashEntry *TKGPU_HASH_API(FirstHashEntry)(Tcl_HashTable *t,Tcl_HashSearch *s) {
    s->tablePtr=t; s->nextIndex=0; s->nextEntryPtr=NULL;
    return TKGPU_HASH_API(NextHashEntry)(s);
}
#undef TKGPU_HASH_API

#undef TG_HASH_DIRECT_COMPARE
