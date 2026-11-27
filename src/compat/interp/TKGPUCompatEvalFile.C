#include "TKGPUCompat.H"
#include "TKGPUBuffer.H"
#include "../../core/TKGPUCoreInternal.H"
#include <stdio.h>
#include <string.h>
#define SCRIPT_LIMIT (16u*1024u*1024u)
int Tcl_EvalFile(Tcl_Interp *i,const char *path) {
    if(!i||i->depth||i->disposing||!path) return TG_ERROR;
    FILE *f=fopen(path,"rb");
    if(!f) return tg_set_result(i,"cannot open script file",TG_ERROR);
    tg_buffer b; tg_buffer_init(&b);
    unsigned char chunk[4096]; size_t n; int rc=TG_ERROR;
    while((n=fread(chunk,1,sizeof chunk,f))!=0) {
        if(n>SCRIPT_LIMIT-b.length||memchr(chunk,0,n)||
           tg_buffer_append(&b,chunk,n)!=TG_OK) goto done;
    }
    if(ferror(f)) goto done;
    rc=TG_OK;
done:
    if(fclose(f)!=0) rc=TG_ERROR;
    if(rc==TG_OK) rc=Tcl_Eval(i,b.data?(const char *)b.data:"");
    else tg_set_result(i,"cannot read supported script file",TG_ERROR);
    tg_buffer_destroy(&b); return rc;
}
