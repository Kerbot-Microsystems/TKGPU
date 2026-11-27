/* Statistics describe the current table, not reference-specific bucket order. */
#ifndef TKGPU_HASH_STATS_API
#define TKGPU_HASH_STATS_API Tcl_HashStats
#endif
char *TKGPU_HASH_STATS_API(Tcl_HashTable *t) {
    size_t histogram[11]={0}; double distance=0.0;
    for(int k=0;k<t->numBuckets;k++) {
        size_t n=0;
        for(Tcl_HashEntry *e=t->buckets[k];e;e=e->nextPtr) ++n;
        ++histogram[n<10?n:10];
        if(t->numEntries) distance+=((double)n/t->numEntries)*((double)n+1.0)*0.5;
    }
    char out[1024]; int used=snprintf(out,sizeof out,"%d entries in table, %d buckets\n",t->numEntries,t->numBuckets);
    if(used<0||(size_t)used>=sizeof out) Tcl_Panic("HashStats formatting failed");
    for(int k=0;k<10;k++) {
        int n=snprintf(out+used,sizeof out-(size_t)used,"number of buckets with %d entries: %lu\n",k,(unsigned long)histogram[k]);
        if(n<0||(size_t)n>=sizeof out-(size_t)used) Tcl_Panic("HashStats formatting failed");
        used+=n;
    }
    int n=snprintf(out+used,sizeof out-(size_t)used,"number of buckets with 10 or more entries: %lu\naverage search distance for entry: %.1f",(unsigned long)histogram[10],distance);
    if(n<0||(size_t)n>=sizeof out-(size_t)used) Tcl_Panic("HashStats formatting failed");
    used+=n;
    char *result=Tcl_Alloc((unsigned)used+1u); memcpy(result,out,(size_t)used+1u); return result;
}
#undef TKGPU_HASH_STATS_API
