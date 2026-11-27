#include "TKGPUCoreInternal.H"
#include "TKGPUExpression.H"
#include "numeric/TKGPURealExpr.H"
#include <inttypes.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
static int add(char *b,size_t *n,const char *s,size_t len) {
    if(len>=MAX_WORD-*n) return 0;
    memcpy(b+*n,s,len); *n+=len; return 1;
}
/* Only numeric scalar substitutions are supported. Never interpret
   a variable's contents as executable Tcl script or expression syntax. */
int tg_eval_condition(tg_interp *i,const char *p,int64_t *value) {
    char b[MAX_WORD]; size_t n=0;
    while(*p) {
        if(*p!='$') {
            if(!add(b,&n,p++,1)) return 0;
            continue;
        }
        p++;
        char key[MAX_WORD]; size_t len=0;
        if(*p=='{') {
            p++;
            while(*p&&*p!='}') {
                if(len>=sizeof(key)-1) return 0;
                key[len++]=*p++;
            }
            if(*p!='}') return 0;
            p++;
        } else {
            while(isalnum((unsigned char)*p)||*p=='_') {
                if(len>=sizeof(key)-1) return 0;
                key[len++]=*p++;
            }
        }
        if(!len) return 0;
        key[len]=0;
        size_t k=tg_var_find(i,key);
        if(k==i->count) return 0;
        const char *v=i->vars[k].value,*q=v;
        if(*q=='+'||*q=='-') q++;
        if(!isdigit((unsigned char)*q)) return 0;
        while(isdigit((unsigned char)*q)) q++;
        if(*q) return 0;
        if(!add(b,&n,"(",1)||!add(b,&n,v,strlen(v))||!add(b,&n,")",1)) return 0;
    }
    b[n]=0;
    return tg_expr_integer(b,value);
}

static const char *real_lookup(void *ctx,const char *name) {
    tg_interp *i=ctx; size_t k=tg_var_find(i,name);
    return k<i->count?i->vars[k].value:NULL;
}
int tg_eval_numeric_result(tg_interp *i,const char *expression) {
    int64_t value; char b[64];
    if(tg_eval_condition(i,expression,&value)) {
        snprintf(b,sizeof b,"%" PRId64,value);
        return tg_set_result_kind(i,b,TG_RESULT_INTEGER);
    }
    double real; int is_real=0;
    if(!tg_expr_real_typed(expression,real_lookup,i,&real,&is_real)||!is_real)
        return tg_set_result(i,"invalid or unsupported numeric expression",TG_ERROR);
    int n=snprintf(b,sizeof b,"%.17g",real);
    if(n<0||n>=(int)sizeof b-2) return tg_set_result(i,"numeric result too long",TG_ERROR);
    if(!strpbrk(b,".eE")) { b[n++]='.'; b[n++]='0'; b[n]=0; }
    return tg_set_result_kind(i,b,TG_RESULT_DOUBLE);
}
