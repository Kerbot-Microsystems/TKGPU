#include "TKGPURealExpr.H"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <locale.h>
#define EXACT_MAX 9007199254740991.0
#define TEXT_MAX 4096
#define DEPTH_MAX 32
typedef struct { double v; int real; } number;
typedef struct { const char *p; tg_real_lookup lookup; void *context; unsigned depth; } parser;
static void skip(parser *p) { while(isspace((unsigned char)*p->p)) p->p++; }
static int digit(char c) { return c>='0'&&c<='9'; }
static int sum(parser *,number *);
static int literal(const char **cursor,number *out) {
    const char *start=*cursor,*p=start; int digits=0,real=0;
    while(digit(*p)) { p++; digits++; }
    if(*p=='.') { real=1; p++; while(digit(*p)) { p++; digits++; } }
    if(!digits) return 0;
    if(*p=='e'||*p=='E') {
        real=1; p++; if(*p=='+'||*p=='-') p++;
        if(!digit(*p)) return 0;
        while(digit(*p)) p++;
    }
    /* Leading-zero integers may denote octal; do not silently parse as decimal. */
    if(!real&&start[0]=='0'&&p-start>1) return 0;
    char *end; errno=0; double v=strtod(start,&end);
    if(end!=p||errno||!isfinite(v)||(!real&&v>EXACT_MAX)) return 0;
    *out=(number){v,real}; *cursor=p; return 1;
}
static int scalar(const char *s,number *out) {
    if(!s||strlen(s)>=TEXT_MAX) return 0;
    while(isspace((unsigned char)*s)) s++;
    int neg=*s=='-'; if(*s=='+'||*s=='-') s++;
    if(!literal(&s,out)) return 0;
    while(isspace((unsigned char)*s)) s++;
    if(*s) return 0;
    if(neg) out->v=-out->v;
    return 1;
}
static int atom(parser *p,number *out) {
    skip(p);
    if(p->depth++>=DEPTH_MAX) return 0;
    int ok=0;
    if(*p->p=='+'||*p->p=='-') {
        int neg=*p->p++=='-'; ok=atom(p,out);
        if(ok&&neg) out->v=-out->v;
    } else if(*p->p=='(') {
        p->p++; ok=sum(p,out); skip(p);
        if(*p->p!=')') ok=0; else p->p++;
    } else if(*p->p=='$'&&p->lookup) {
        p->p++; char name[128]; size_t n=0;
        int braces=*p->p=='{'; if(braces) p->p++;
        while(*p->p&&(braces?*p->p!='}':isalnum((unsigned char)*p->p)||*p->p=='_')) {
            if(n==sizeof name-1) goto done;
            name[n++]=*p->p++;
        }
        if(braces) { if(*p->p!='}') goto done; p->p++; }
        if(n) { name[n]=0; ok=scalar(p->lookup(p->context,name),out); }
    } else ok=literal(&p->p,out);
done:
    p->depth--; return ok;
}
static int combine(number *a,number b,char op) {
    int real=a->real||b.real; double v;
    if(op=='+') v=a->v+b.v;
    else if(op=='-') v=a->v-b.v;
    else if(op=='*') v=a->v*b.v;
    else {
        if(b.v==0.0) return 0;
        if(real) v=a->v/b.v;
        else {
            int64_t x=(int64_t)a->v,y=(int64_t)b.v,q=x/y,r=x%y;
            if(r&&((x<0)!=(y<0))) q--;
            v=(double)q;
        }
    }
    if(!isfinite(v)||(!real&&(v>EXACT_MAX||v<-EXACT_MAX))) return 0;
    *a=(number){v,real}; return 1;
}
static int product(parser *p,number *out) {
    if(!atom(p,out)) return 0;
    for(;;) {
        skip(p); char op=*p->p;
        if(op!='*'&&op!='/') return 1;
        p->p++; number b;
        if(!atom(p,&b)||!combine(out,b,op)) return 0;
    }
}
static int sum(parser *p,number *out) {
    if(!product(p,out)) return 0;
    for(;;) {
        skip(p); char op=*p->p;
        if(op!='+'&&op!='-') return 1;
        p->p++; number b;
        if(!product(p,&b)||!combine(out,b,op)) return 0;
    }
}
int tg_expr_real_typed(const char *s,tg_real_lookup lookup,void *context,double *out,int *is_real) {
    if(!s||!out||strlen(s)>=TEXT_MAX||strcmp(localeconv()->decimal_point,".")) return 0;
    parser p={s,lookup,context,0}; number n;
    if(!sum(&p,&n)) return 0;
    skip(&p); if(*p.p) return 0;
    *out=n.v; if(is_real) *is_real=n.real; return 1;
}
int tg_expr_real(const char *s,tg_real_lookup lookup,void *context,double *out) {
    return tg_expr_real_typed(s,lookup,context,out,NULL);
}
