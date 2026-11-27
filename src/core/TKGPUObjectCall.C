#include "TKGPUCoreInternal.H"
#include <stdlib.h>
#include <string.h>
/* Direct argument-vector execution: argument values never pass through
   the script tokenizer. Copies protect borrowed result pointers. */
int tg_call(tg_interp *i,size_t argc,const char *const argv[]) {
    if(i&&i->disposing) return tg_set_result(i,"interpreter is disposing a command",TG_ERROR);
    if(!i||i->depth>=i->recursion_limit||(i->depth&&!i->compat_callback_depth)||i->disposing||!argc||argc>MAX_ARGS||!argv) return TG_ERROR;
    char *args[MAX_ARGS]; size_t lengths[MAX_ARGS],total=0;
    for(size_t k=0;k<argc;k++) {
        if(!argv[k]||(lengths[k]=strlen(argv[k]))>=1024u*1024u)
            return tg_set_result(i,"invalid argument",TG_ERROR);
        total+=lengths[k]+1;
    }
    /* Small calls (the common Tkinter widget operation) allocate nothing.
       Each nested call owns a separate buffer, protecting borrowed results. */
    char local[1024]; char *storage=total<=sizeof local?local:malloc(total);
    if(!storage) return tg_set_result(i,"out of memory",TG_ERROR);
    char *p=storage;
    for(size_t k=0;k<argc;k++) {
        args[k]=p; memcpy(p,argv[k],lengths[k]+1); p+=lengths[k]+1;
    }
    /* tg_execute resets the result after all borrowed arguments are copied. */
    i->depth++;
    int rc=tg_execute(i,argc,args);
    i->depth--;
    if(storage!=local) free(storage);
    return rc;
}
