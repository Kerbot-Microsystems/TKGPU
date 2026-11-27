#include "TKGPUCoreInternal.H"
#include "TKGPUExpression.H"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static int eval_body(tg_interp *i,const char *body) {
    const char *p=body;
    return tg_eval_script(i,&p,0);
}
/* Quoted command-result comparison used by Tkinter's bind wrapper. */
static int quoted_command_equal(tg_interp *i,const char *s,int64_t *value) {
    if(s[0]!='"'||s[1]!='[') return -1;
    const char *tail=strrchr(s,']');
    if(!tail||tail[1]!='"') return -1;
    const char *p=tail+2;
    while(*p==' '||*p=='\t') p++;
    int equal;
    if(!strncmp(p,"==",2)||!strncmp(p,"eq",2)) equal=1;
    else if(!strncmp(p,"!=",2)||!strncmp(p,"ne",2)) equal=0;
    else return -1;
    p+=2; while(*p==' '||*p=='\t') p++;
    if(*p++!='"') return -1;
    const char *end=strchr(p,'"');
    if(!end||end[1]||memchr(p,'\\',(size_t)(end-p))||memchr(p,'$',(size_t)(end-p))||
       memchr(p,'[',(size_t)(end-p))) return -1;
    const char *command=s+2;
    if(tg_eval_script(i,&command,1)!=TG_OK||command!=tail+1) return 0;
    size_t len=(size_t)(end-p);
    int same=strlen(i->result)==len&&!memcmp(i->result,p,len);
    *value=equal?same:!same; return 1;
}
int tg_control_command(tg_interp *i,size_t n,char **a) {
    if(!strcmp(a[0],"break")||!strcmp(a[0],"continue")) {
        if(n!=1) return tg_set_result(i,"wrong # args for loop control",TG_ERROR);
        return tg_set_result(i,"",!strcmp(a[0],"break")?TG_BREAK:TG_CONTINUE);
    }
    if(!strcmp(a[0],"eval")) {
        if(n!=2) return tg_set_result(i,"unsupported eval argument count",TG_ERROR);
        return eval_body(i,a[1]);
    }
    if(!strcmp(a[0],"catch")) {
        if(n<2||n>3) return tg_set_result(i,"wrong # args: catch script ?resultVarName?",TG_ERROR);
        int rc=eval_body(i,a[1]);
        if(n==3) {
            char *saved=tg_copy_string(i->result);
            if(!saved) return tg_set_result(i,"out of memory",TG_ERROR);
            int store_rc=tg_var_store(i,a[2],saved);
            free(saved);
            if(store_rc!=TG_OK) return TG_ERROR;
        }
        char code[16]; snprintf(code,sizeof code,"%d",rc);
        return tg_set_result_kind(i,code,TG_RESULT_INTEGER);
    }
    if(!strcmp(a[0],"if")) {
        if(n!=3&&(n!=5||strcmp(a[3],"else")))
            return tg_set_result(i,"unsupported if syntax",TG_ERROR);
        int64_t value;
        int comparison=quoted_command_equal(i,a[1],&value);
        if(comparison==0||(comparison<0&&!tg_eval_condition(i,a[1],&value)))
            return tg_set_result(i,"invalid or unsupported condition",TG_ERROR);
        if(value) return eval_body(i,a[2]);
        if(n==5) return eval_body(i,a[4]);
        return tg_set_result(i,"",TG_OK);
    }
    return -1;
}
