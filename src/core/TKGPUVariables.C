#include "TKGPUCoreInternal.H"
#include "../widgets/ttk/TKGPUToggle.H"
#include "../widgets/ttk/TKGPUExtra.H"
#include "../widgets/controls/TKGPUControls.H"
#include "../widgets/TKGPUWidgetInternal.H"
#include <stdlib.h>
#include <string.h>

static const char *global_key(const char *key) {
    return key[0]==':'&&key[1]==':'?key+2:key;
}
static size_t array_base(const char *key) {
    size_t n=strlen(key);
    const char *p=strchr(key,'(');
    return p&&n&&key[n-1]==')'?(size_t)(p-key):n;
}
static int array_member(const char *base,const char *key) {
    size_t n=strlen(base);
    return array_base(key)==n&&strlen(key)>n&&!strncmp(base,key,n);
}
static void notify_waits(tg_interp *i,const char *key) {
    for(tg_var_wait *w=i->var_waits;w;w=w->next)
        if(!strcmp(w->key,key)||array_member(w->key,key)) w->changed=1;
}
size_t tg_var_find(tg_interp *i,const char *key) {
    key=global_key(key);
    size_t k; for(k=0;k<i->count;k++) if(!strcmp(i->vars[k].key,key)) break; return k;
}
int tg_var_store(tg_interp *i,const char *key,const char *value) {
    return tg_var_store_object(i,key,value,NULL);
}
static int store(tg_interp *i,const char *key,const char *value,tg_object *object,int kind,int result);
int tg_var_store_object(tg_interp *i,const char *key,const char *value,tg_object *object) {
    return store(i,key,value,object,TG_RESULT_STRING,1);
}
int tg_var_store_kind(tg_interp *i,const char *key,const char *value,int kind) {
    return store(i,key,value,NULL,kind,1);
}
int tg_var_assign_object(tg_interp *i,const char *key,const char *value,tg_object *object) {
    return store(i,key,value,object,TG_RESULT_STRING,0);
}
static int store(tg_interp *i,const char *key,const char *value,tg_object *object,int kind,int result) {
    key=global_key(key);
    if(!*key||strlen(key)>=MAX_WORD) return tg_set_result(i,"unsupported variable name",TG_ERROR);
    size_t k=tg_var_find(i,key);
    size_t n=strlen(key),base=array_base(key);
    /* Only insertion can introduce a scalar/array conflict. Existing keys
       have already passed this validation. */
    if(k==i->count) for(size_t j=0;j<i->count;j++) {
        const char *other=i->vars[j].key;
        if(base<n&&strlen(other)==base&&!strncmp(other,key,base)&&i->vars[j].kind!=TG_VAR_ARRAY)
            return tg_set_result(i,"variable is not an array",TG_ERROR);
        if(base==n&&array_member(key,other))
            return tg_set_result(i,"variable is an array",TG_ERROR);
    }
    if(k<i->count&&i->vars[k].kind==TG_VAR_ARRAY)
        return tg_set_result(i,"variable is an array",TG_ERROR);
    size_t object_length=0;
    const char *object_text=object?tg_object_bytes(object,&object_length):NULL;
    int binary=object_text&&memchr(object_text,0,object_length)!=NULL;
    int validate=0,bound=0;
    for(size_t j=1;j<i->ui->count;j++) {
        tg_widget *w=&i->ui->items[j];
        if(!tg_textvar_matches(w,key)) continue;
        if(binary||(!bound&&!tg_textvar_valid(value)))
            return tg_set_result(i,"bound text exceeds widget capacity or is invalid UTF-8",TG_ERROR);
        bound=1;
        if(!w->themed&&tg_textvar_editable(w->kind)&&w->validate_mode&&w->validate_command[0]&&strcmp(w->text,value))
            validate=1;
    }
    char trace_key[MAX_WORD],parent[MAX_WORD]; strcpy(trace_key,key);
    memcpy(parent,key,base); parent[base]=0;
    int insert=k==i->count;
    int insert_parent=base<n&&tg_var_find(i,parent)==i->count;
    size_t needed=i->count+(size_t)insert+(size_t)insert_parent;
    if(needed>MAX_VARS) return tg_set_result(i,"variable limit",TG_ERROR);
    char *v=tg_copy_string(value),*name=insert?tg_copy_string(key):NULL;
    char *parent_key=insert_parent?tg_copy_string(parent):NULL;
    char *empty=insert_parent?tg_copy_string(""):NULL;
    if(!v||(insert&&!name)||(insert_parent&&(!parent_key||!empty))) goto allocation_failed;
    if(needed>i->var_capacity) {
        size_t capacity=i->var_capacity?i->var_capacity*2:16;
        if(capacity<needed) capacity=needed;
        if(capacity>MAX_VARS) capacity=MAX_VARS;
        var *grown=realloc(i->vars,capacity*sizeof *grown);
        if(!grown) goto allocation_failed;
        i->vars=grown; i->var_capacity=capacity;
    }
    if(object&&!tg_object_retain(object)) goto allocation_failed;
    if(insert) { k=i->count++; i->vars[k]=(var){name,NULL,NULL,TG_RESULT_STRING}; }
    if(insert_parent) i->vars[i->count++]=(var){parent_key,empty,NULL,TG_VAR_ARRAY};
    char *previous_value=i->vars[k].value;
    tg_object *previous_object=i->vars[k].object;
    /* Commit every field before invoking an object's destruction callback. */
    i->vars[k].value=v; i->vars[k].object=object; i->vars[k].kind=kind;
    free(previous_value); tg_object_release(previous_object);
    k=tg_var_find(i,trace_key);
    if(k==i->count) return result?tg_set_result(i,"",TG_OK):TG_OK;
    notify_waits(i,trace_key);
    if(validate) {
        tg_entry_validation_variable(i,trace_key);
        /* The callback may have replaced or unset the variable. */
        k=tg_var_find(i,trace_key);
        if(k==i->count) return result?tg_set_result(i,"",TG_OK):TG_OK;
    }
    int changed=tg_ui_toggle_sync(i->ui,trace_key,i->vars[k].value);
    changed|=tg_scale_sync(i->ui,trace_key,i->vars[k].value);
    changed|=tg_textvar_sync(i->ui,trace_key,i->vars[k].value);
    if(changed&&i->tk_invalidate) i->tk_invalidate(i->tk_host);
    if(tg_trace_notify(i,trace_key,1)!=TG_OK) return TG_ERROR;
    k=tg_var_find(i,trace_key);
    if(!result) return TG_OK;
    return k<i->count?tg_set_result_kind(i,i->vars[k].value,i->vars[k].kind):tg_set_result(i,"",TG_OK);
allocation_failed:
    free(v); free(name); free(parent_key); free(empty);
    return tg_set_result(i,"out of memory",TG_ERROR);
}
int tg_var_unset(tg_interp *i,const char *key) {
    key=global_key(key);
    if(!*key||strlen(key)>=MAX_WORD) return tg_set_result(i,"unsupported variable name",TG_ERROR);
    char trace_key[MAX_WORD]; strcpy(trace_key,key);
    size_t k=tg_var_find(i,trace_key);
    int whole_array=k==i->count||i->vars[k].kind==TG_VAR_ARRAY;
    size_t count=0;
    for(size_t j=0;j<i->count;j++)
        if(!strcmp(trace_key,i->vars[j].key)||(whole_array&&array_member(trace_key,i->vars[j].key))) count++;
    if(!count) return tg_set_result(i,"unknown variable",TG_ERROR);
    var *removed=malloc(count*sizeof *removed);
    if(!removed) return tg_set_result(i,"out of memory",TG_ERROR);
    size_t taken=0;
    /* Detach the complete old array before destructors can recreate it. */
    for(size_t j=0;j<i->count;) {
        if(strcmp(trace_key,i->vars[j].key)&&!(whole_array&&array_member(trace_key,i->vars[j].key))) { j++; continue; }
        removed[taken++]=i->vars[j];
        i->vars[j]=i->vars[--i->count];
        i->vars[i->count]=(var){NULL,NULL,NULL,TG_RESULT_STRING};
    }
    for(size_t j=0;j<taken;j++) {
        if(tg_ui_toggle_sync(i->ui,removed[j].key,NULL)&&i->tk_invalidate) i->tk_invalidate(i->tk_host);
        notify_waits(i,removed[j].key);
        free(removed[j].key); free(removed[j].value); tg_object_release(removed[j].object);
    }
    free(removed);
    if(tg_trace_notify(i,trace_key,2)!=TG_OK) return TG_ERROR;
    return tg_set_result(i,"",TG_OK);
}
