#include "TKGPUCompat.H"
#include "TKGPUHost.H"
#include <string.h>
#include "../object/TKGPUObjectInternal.H"
#include "../core/TKGPUCoreInternal.H"
#include <limits.h>
#include "types/TKGPUCompatTypes.H"
static void mark_integer(Tcl_Obj *o,int64_t value) {
    tg_object_clear_type(o);
    if(value>=LONG_MIN&&value<=LONG_MAX) {
        o->typePtr=&tg_type_int; o->internalRep.integer=(long)value;
    } else {
        o->typePtr=&tg_type_wide; o->internalRep.wide=value;
    }
}
static Tcl_Obj *integer_object(int64_t value) {
    Tcl_Obj *o=tg_object_integer(value);
    if(!o) return NULL;
    mark_integer(o,value); return tg_compat_new_object(o);
}
static int read_integer(Tcl_Obj *o,int64_t *out) {
    if(!o||!out) return TG_ERROR;
    if(o->typePtr==&tg_type_int) { *out=(int64_t)o->internalRep.integer; return TG_OK; }
    if(o->typePtr==&tg_type_wide) { *out=o->internalRep.wide; return TG_OK; }
    size_t n; const char *s=tg_object_bytes(o,&n);
    if(!s||memchr(s,0,n)||TKGPU_HostInteger(s,out)!=TKGPU_HOST_OK) return TG_ERROR;
    mark_integer(o,*out); return TG_OK;
}
Tcl_Obj *Tcl_NewIntObj(int value) { return integer_object(value); }
Tcl_Obj *Tcl_NewLongObj(long value) { return integer_object((int64_t)value); }
Tcl_Obj *Tcl_NewWideIntObj(long long value) { return integer_object((int64_t)value); }
int Tcl_GetWideIntFromObj(Tcl_Interp *i,Tcl_Obj *o,long long *out) {
    int64_t n;
    if(!out||read_integer(o,&n)!=TG_OK) {
        if(i) tg_set_result(i,"expected signed integer",TG_ERROR);
        return TG_ERROR;
    }
    *out=(long long)n; return TG_OK;
}
int Tcl_GetIntFromObj(Tcl_Interp *i,Tcl_Obj *o,int *out) {
    int64_t n;
    if(!out||read_integer(o,&n)!=TG_OK||n<INT_MIN||n>INT_MAX) {
        if(i) tg_set_result(i,"integer out of range or invalid",TG_ERROR);
        return TG_ERROR;
    }
    *out=(int)n; return TG_OK;
}

int Tcl_GetLongFromObj(Tcl_Interp *i,Tcl_Obj *o,long *out) {
    int64_t n;
    if(!out||read_integer(o,&n)!=TG_OK||n<LONG_MIN||n>LONG_MAX) {
        if(i) tg_set_result(i,"long integer out of range or invalid",TG_ERROR);
        return TG_ERROR;
    }
    *out=(long)n; return TG_OK;
}
Tcl_Obj *Tcl_NewBooleanObj(int value) { return Tcl_NewIntObj(value!=0); }

int Tcl_GetInt(Tcl_Interp *i,const char *s,int *out) {
    Tcl_Obj *o=s?Tcl_NewStringObj(s,-1):NULL;
    int rc=Tcl_GetIntFromObj(i,o,out);
    TKGPU_ReleaseObject(o); return rc;
}

/* The table is borrowed only during this call. No cached index may outlive
   a caller-owned table or survive its mutation. memcpy permits packed rows. */
static const char *index_word(const void *table,size_t stride,size_t k) {
    const char *word=NULL;
    memcpy(&word,(const unsigned char *)table+k*stride,sizeof word);
    return word;
}
static int index_append(char *buf,size_t *used,const char *s) {
    size_t n=strlen(s);
    if(n>=MAX_WORD-*used) return 0;
    memcpy(buf+*used,s,n+1); *used+=n; return 1;
}
int Tcl_GetIndexFromObjStruct(Tcl_Interp *i,Tcl_Obj *o,const void *table,int offset,
                             const char *msg,int flags,int *out) {
    size_t n=0; const char *key=tg_object_bytes(o,&n);
    if(!key||memchr(key,0,n)||!table||offset<(int)sizeof(char *)||!out)
        return i?tg_set_result(i,"invalid index lookup",TG_ERROR):TG_ERROR;
    size_t stride=(size_t)offset,count=0;
    int matches=0,found=-1;
    for(;count<(size_t)INT_MAX&&count<=(SIZE_MAX-sizeof(char *))/stride;count++) {
        const char *word=index_word(table,stride,count);
        if(!word) break;
        if(!strcmp(key,word)) { *out=(int)count; return TG_OK; }
        if(!(flags&1)&&!strncmp(word,key,n)) {
            found=(int)count;
            if(matches<2) ++matches;
        }
    }
    if(n&&matches==1) { *out=found; return TG_OK; }
    if(!i) return TG_ERROR;
    char result[MAX_WORD]=""; size_t used=0;
#define ADD(x) do { if(!index_append(result,&used,(x))) goto too_long; } while(0)
    ADD(matches>1?"ambiguous ":"bad ");
    if(msg) { ADD(msg); ADD(" \""); ADD(key); }
    ADD("\": ");
    size_t nonempty=0;
    for(size_t k=0;k<count;k++) if(*index_word(table,stride,k)) nonempty++;
    if(!nonempty) ADD("no valid options");
    else {
        ADD("must be ");
        /* Reference Tcl omits interior empty names but displays a final
           empty name when there is at least one nonempty option. */
        size_t shown=nonempty+(!*index_word(table,stride,count-1));
        size_t printed=0;
        for(size_t k=0;k<count;k++) {
            const char *word=index_word(table,stride,k);
            if(!*word&&k+1!=count) continue;
            if(printed) ADD(printed+1==shown?(shown==2?" or ":", or "):", ");
            ADD(word); printed++;
        }
    }
#undef ADD
    return tg_set_result(i,result,TG_ERROR);
too_long:
    return tg_set_result(i,"invalid or ambiguous option",TG_ERROR);
}
int Tcl_GetIndexFromObj(Tcl_Interp *i,Tcl_Obj *o,const char *const table[],
                       const char *msg,int flags,int *out) {
    return Tcl_GetIndexFromObjStruct(i,o,table,(int)sizeof *table,msg,flags,out);
}
