#include "TKGPUCoreInternal.H"
#include <stdlib.h>
#include <string.h>
int tg_command_register(tg_interp *i,const char *name,tg_command_fn fn,void *context,tg_command_dispose_fn dispose) {
    if(!i||(i->depth&&!i->compat_callback_depth)||i->disposing||!name||!*name||strlen(name)>=MAX_WORD||!fn||
       i->command_count==MAX_COMMANDS||tg_command_lookup(i,name)<i->command_count) return TG_ERROR;
    char *copy=tg_copy_string(name);
    if(!copy) return TG_ERROR;
    if(i->command_count==i->command_capacity) {
        size_t cap=i->command_capacity?i->command_capacity*2:16;
        if(cap>MAX_COMMANDS) cap=MAX_COMMANDS;
        tg_registered_command *next=realloc(i->commands,cap*sizeof *next);
        if(!next) { free(copy); return TG_ERROR; }
        i->commands=next; i->command_capacity=cap;
    }
    i->commands[i->command_count++]=(tg_registered_command){copy,fn,context,dispose,NULL};
    tg_command_index_added(i);
    return TG_OK;
}
int tg_command_unregister(tg_interp *i,const char *name) {
    if(!i||(i->depth&&!i->compat_callback_depth)||i->disposing||!name) return TG_ERROR;
    size_t k=tg_command_lookup(i,name);
    if(k==i->command_count) return TG_ERROR;
    tg_registered_command command=i->commands[k];
    tg_command_index_remove(i,k);
    i->commands[k]=i->commands[--i->command_count];
    i->disposing=1;
    if(command.dispose) command.dispose(command.context);
    i->disposing=0; free(command.name);
    return TG_OK;
}
int tg_command_result(tg_interp *i,const char *value) {
    if(!i||!value||i->disposing) return TG_ERROR;
    return tg_set_result(i,value,TG_OK);
}
int tg_command_dispatch(tg_interp *i,size_t n,char **a) {
    size_t k=tg_command_lookup(i,a[0]);
    if(k==i->command_count) return -1;
    tg_registered_command *command=&i->commands[k];
    const char *argv[MAX_ARGS];
    for(size_t j=0;j<n;j++) argv[j]=a[j];
    i->result[0]=0;
    int rc=command->fn(command->context,i,n,argv);
    if(rc!=TG_OK&&rc!=TG_ERROR&&rc!=TG_BREAK&&rc!=TG_CONTINUE) return tg_set_result(i,"unsupported callback status",TG_ERROR);
    return rc;
}
void tg_command_clear(tg_interp *i) {
    i->disposing=1;
    tg_command_index_clear(i);
    while(i->command_count) {
        tg_registered_command command=i->commands[--i->command_count];
        if(command.dispose) command.dispose(command.context);
        free(command.name);
    }
}
