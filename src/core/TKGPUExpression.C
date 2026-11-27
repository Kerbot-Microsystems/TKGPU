#include "TKGPUExpression.H"
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <string.h>

typedef struct { const char *p; unsigned depth; int ok,skip_eval; } parser;
static void space(parser *s) { while(isspace((unsigned char)*s->p)) s->p++; }
static int64_t logical_or(parser *s);
static int64_t factor(parser *s) {
    int64_t n=0;
    space(s);
    if(s->depth>=64) { s->ok=0; return 0; }
    s->depth++;
    if(*s->p=='+'||*s->p=='-'||*s->p=='!') {
        char op=*s->p++;
        n=factor(s);
        if(!s->skip_eval&&s->ok) {
            if(op=='-') {
                if(n==INT64_MIN) s->ok=0;
                else n=-n;
            } else if(op=='!') n=!n;
        }
    } else if(*s->p=='(') {
        s->p++;
        n=logical_or(s); space(s);
        if(*s->p!=')') s->ok=0;
        else s->p++;
    } else if(*s->p>='0'&&*s->p<='9') {
        /* Preserve the explicit decimal-only literal contract. */
        if(*s->p=='0'&&s->p[1]>='0'&&s->p[1]<='9') s->ok=0;
        char *end; errno=0;
        long long v=strtoll(s->p,&end,10);
        if(!s->skip_eval) {
            if(errno||v>INT64_MAX||v<INT64_MIN) s->ok=0;
            else n=(int64_t)v;
        }
        s->p=end;
    } else s->ok=0;
    s->depth--;
    return s->skip_eval?0:n;
}
static int mul_ok(int64_t a,int64_t b) {
    if(!a||!b) return 1;
    if(a>0) return b>0?a<=INT64_MAX/b:b>=INT64_MIN/a;
    return b>0?a>=INT64_MIN/b:a>=INT64_MAX/b;
}
static int64_t term(parser *s) {
    int64_t a=factor(s);
    while(s->ok) {
        space(s); char op=*s->p;
        if(op!='*'&&op!='/'&&op!='%') break;
        s->p++; int64_t b=factor(s);
        if(!s->ok) break;
        if(s->skip_eval) continue;
        if(op=='*') {
            if(!mul_ok(a,b)) { s->ok=0; break; }
            a*=b;
        } else {
            if(!b||(a==INT64_MIN&&b==-1)) { s->ok=0; break; }
            int64_t q=a/b,r=a%b;
            if(r&&((r<0)!=(b<0))) { q--; r+=b; }
            a=op=='/'?q:r;
        }
    }
    return a;
}
static int64_t sum(parser *s) {
    int64_t a=term(s);
    while(s->ok) {
        space(s); char op=*s->p;
        if(op!='+'&&op!='-') break;
        s->p++; int64_t b=term(s);
        if(!s->ok) break;
        if(s->skip_eval) continue;
        if(op=='+') {
            if((b>0&&a>INT64_MAX-b)||(b<0&&a<INT64_MIN-b)) { s->ok=0; break; }
            a+=b;
        } else {
            if((b>0&&a<INT64_MIN+b)||(b<0&&a>INT64_MAX+b)) { s->ok=0; break; }
            a-=b;
        }
    }
    return a;
}
static int64_t relation(parser *s) {
    int64_t a=sum(s);
    while(s->ok) {
        space(s); char op=*s->p;
        if(op!='<'&&op!='>') break;
        s->p++; int equal=*s->p=='=';
        if(equal) s->p++;
        int64_t b=sum(s);
        if(s->ok&&!s->skip_eval) a=op=='<'?(equal?a<=b:a<b):(equal?a>=b:a>b);
    }
    return a;
}
static int64_t equality(parser *s) {
    int64_t a=relation(s);
    while(s->ok) {
        space(s);
        int equal=!strncmp(s->p,"==",2);
        if(!equal&&strncmp(s->p,"!=",2)) break;
        s->p+=2;
        int64_t b=relation(s);
        if(s->ok&&!s->skip_eval) a=equal?a==b:a!=b;
    }
    return a;
}
static int64_t logical_and(parser *s) {
    int64_t a=equality(s);
    while(s->ok) {
        space(s); if(strncmp(s->p,"&&",2)) break;
        s->p+=2;
        int previous=s->skip_eval;
        s->skip_eval=previous||!a;
        int64_t b=equality(s);
        s->skip_eval=previous;
        if(s->ok&&!previous) a=a&&b;
    }
    return a;
}
static int64_t logical_or(parser *s) {
    int64_t a=logical_and(s);
    while(s->ok) {
        space(s); if(strncmp(s->p,"||",2)) break;
        s->p+=2;
        int previous=s->skip_eval;
        s->skip_eval=previous||a!=0;
        int64_t b=logical_and(s);
        s->skip_eval=previous;
        if(s->ok&&!previous) a=a||b;
    }
    return a;
}
int tg_expr_integer(const char *source,int64_t *value) {
    if(!source||!value) return 0;
    parser s={source,0,1,0};
    int64_t n=logical_or(&s); space(&s);
    if(!s.ok||*s.p) return 0;
    *value=n; return 1;
}
