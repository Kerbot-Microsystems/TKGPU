#include "TKGPUCompat.H"
#include <stdint.h>
#include <stdlib.h>

/* Completeness is lexical, not evaluation or syntax validation. An invalid
   completed word is complete even if text following the syntax error is not.
   Use a heap stack so untrusted deeply nested input never recurses in C. */
enum { SCRIPT,WORD,QUOTE,BRACE,VARIABLE,ARRAY };
typedef struct { int kind; unsigned state; } complete_frame;
typedef struct { complete_frame *items; size_t count,capacity; } complete_stack;
static void push(complete_stack *s,int kind,unsigned state) {
    if(s->count==s->capacity) {
        size_t next=s->capacity?s->capacity*2:32;
        if(next<s->capacity||next>SIZE_MAX/sizeof *s->items) Tcl_Panic("completeness stack overflow");
        complete_frame *items=realloc(s->items,next*sizeof *items);
        if(!items) Tcl_Panic("cannot allocate completeness stack");
        s->items=items; s->capacity=next;
    }
    s->items[s->count++]=(complete_frame){kind,state};
}
static int space(unsigned char c) { return c==' '||c=='\t'||c=='\r'||c=='\v'||c=='\f'||c=='\n'; }
static int name_char(unsigned char c) {
    return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_';
}
static const unsigned char *variable(complete_stack *s,const unsigned char *p) {
    ++p;
    if(*p=='{') { push(s,VARIABLE,0); return p+1; }
    for(;;) {
        if(name_char(*p)) ++p;
        else if(p[0]==':'&&p[1]==':') { do { ++p; } while(*p==':'); }
        else break;
    }
    if(*p=='(') { push(s,ARRAY,0); ++p; }
    return p;
}
int Tcl_CommandComplete(const char *text) {
    if(!text) return 1;
    const unsigned char *p=(const unsigned char *)text;
    complete_stack s={0};
    int result=1;
    push(&s,SCRIPT,1);
    while(s.count) {
        complete_frame *f=&s.items[s.count-1];
        if(!*p) {
            if(f->kind==WORD) { --s.count; continue; }
            result=f->kind==SCRIPT&&s.count==1;
            break;
        }
        if(f->kind==VARIABLE) {
            if(*p++=='}') --s.count;
            continue;
        }
        if(f->kind==WORD&&f->state) {
            int nested=s.count>2&&s.items[s.count-2].kind==SCRIPT;
            if(!space(*p)&&*p!=';'&&!(*p==']'&&nested)&&!(p[0]=='\\'&&p[1]=='\n')) {
                result=1; break;
            }
        }
        if(f->kind==SCRIPT&&p[0]=='\\'&&p[1]!='\n') {
            f->state=0; push(&s,WORD,0); continue;
        }
        if(*p=='\\') {
            if(p[1]=='\n') {
                if(f->kind==WORD) --s.count;
                p+=2;
                if(!*p) { result=0; break; }
                while(*p==' '||*p=='\t') ++p;
            } else p+=p[1]?2:1;
            continue;
        }
        if(f->kind==BRACE) {
            if(*p=='{') ++f->state;
            else if(*p=='}'&&!--f->state) {
                --s.count;
                s.items[s.count-1].state=1;
            }
            ++p; continue;
        }
        if(f->kind==SCRIPT) {
            if(space(*p)||*p==';') {
                if(*p=='\n'||*p==';') f->state=1;
                ++p; continue;
            }
            if(*p==']'&&s.count>1) { --s.count; ++p; continue; }
            if(*p=='#'&&f->state) {
                while(*p&&*p!='\n') {
                    if(p[0]=='\\'&&p[1]) {
                        if(p[1]=='\n') {
                            p+=2;
                            if(!*p) { result=0; goto done; }
                            while(*p==' '||*p=='\t') ++p;
                        } else p+=2;
                    } else ++p;
                }
                continue;
            }
            f->state=0;
            push(&s,WORD,0);
            if(*p=='{') { push(&s,BRACE,1); ++p; }
            else if(*p=='"') { push(&s,QUOTE,0); ++p; }
            continue;
        }
        if(f->kind==WORD) {
            int nested=s.count>2&&s.items[s.count-2].kind==SCRIPT;
            int delimiter=space(*p)||*p==';'||(*p==']'&&nested);
            if(delimiter) { --s.count; continue; }
            if(f->state) { result=1; break; }
        } else if(f->kind==QUOTE&&*p=='"') {
            --s.count; s.items[s.count-1].state=1; ++p; continue;
        } else if(f->kind==ARRAY&&*p==')') {
            --s.count; ++p; continue;
        }
        if(*p=='[') { push(&s,SCRIPT,1); ++p; }
        else if(*p=='$') p=variable(&s,p);
        else ++p;
    }
done:
    free(s.items);
    return result;
}
