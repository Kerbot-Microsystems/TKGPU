#include "TKGPUCoreInternal.H"
#include "TKGPUList.H"
#include <stdlib.h>
#include <ctype.h>

static int delimiter(char c,int nested) {
    return c==0 || c==';' || c=='\n' || (nested&&c==']') || isspace((unsigned char)c);
}

static int emit(char *b,size_t *n,char c) {
    if(*n>=MAX_WORD-1) return 0;
    b[(*n)++]=c; return 1;
}
static int continuation(const char *p) { return p[0]=='\\'&&p[1]=='\n'; }
static const char *after_continuation(const char *p) {
    p+=2; while(*p==' '||*p=='\t') p++;
    return p;
}
/* Explicit subset: brace groups, quoted and plain words, scalar variables,
   simple escapes and bounded nested command substitution. */
static int word(tg_interp *i,const char **cursor,char *b,int nested) {
    const char *p=*cursor; size_t n=0;
    int brace=*p=='{',quote=*p=='"',depth=brace;
    if(brace||quote) p++;
    while(*p) {
        char c=*p;
        if(brace) {
            if(c=='\\' && p[1]) {
                if(continuation(p)) {
                    p=after_continuation(p);
                    if(!emit(b,&n,' ')) goto limit;
                    continue;
                }
                if(!emit(b,&n,*p++)||!emit(b,&n,*p++)) goto limit;
                continue;
            }
            p++;
            if(c=='{') depth++;
            if(c=='}' && --depth==0) break;
            if(!emit(b,&n,c)) goto limit;
            continue;
        }
        if(quote && c=='"') { p++; quote=0; break; }
        if(!quote && (delimiter(c,nested)||continuation(p))) break;
        p++;
        if(c=='[') {
            if(tg_eval_script(i,&p,1)!=TG_OK) return TG_ERROR;
            for(const char *v=i->result;*v;v++) if(!emit(b,&n,*v)) goto limit;
            continue;
        }
        if(c=='\\') {
            if(!*p) { if(!emit(b,&n,'\\')) goto limit; break; }
            c=*p++;
            if(c=='n') c='\n'; else if(c=='t') c='\t'; else if(c=='r') c='\r';
            else if(c=='a') c='\a'; else if(c=='b') c='\b';
            else if(c=='f') c='\f'; else if(c=='v') c='\v';
            else if(c=='\n') { p=after_continuation(p-2); c=' '; }
            else if(c=='x'||c=='u'||c=='U'||(c>='0'&&c<='7')) {
                if(!tg_numeric_escape(c,&p,b,MAX_WORD,&n))
                    return tg_set_result(i,"unsupported numeric escape or word too long",TG_ERROR);
                continue;
            }
        } else if(c=='$') {
            char key[MAX_WORD]; size_t len=0;
            if(*p=='{') {
                p++; while(*p && *p!='}') {
                    if(!emit(key,&len,*p++)) goto limit;
                }
                if(*p!='}') return tg_set_result(i,"unclosed variable",TG_ERROR);
                p++;
            } else {
                for(;;) {
                    while(isalnum((unsigned char)*p)||*p=='_') {
                        if(!emit(key,&len,*p++)) goto limit;
                    }
                    if(p[0]==':'&&p[1]==':') {
                        if(!emit(key,&len,*p++)||!emit(key,&len,*p++)) goto limit;
                    } else break;
                }
                if(!len) { if(!emit(b,&n,'$')) goto limit; continue; }
                if(*p=='(') {
                    if(!emit(key,&len,*p++)) goto limit;
                    while(*p&&*p!=')') {
                        if(*p=='$'||*p=='['||*p=='\\'||*p=='(')
                            return tg_set_result(i,"array index substitution unsupported",TG_ERROR);
                        if(!emit(key,&len,*p++)) goto limit;
                    }
                    if(*p!=')') return tg_set_result(i,"unclosed array index",TG_ERROR);
                    if(!emit(key,&len,*p++)) goto limit;
                }
            }
            key[len]=0; size_t k=tg_var_find(i,key);
            if(k==i->count||i->vars[k].kind==TG_VAR_ARRAY) return tg_set_result(i,"unknown variable",TG_ERROR);
            for(const char *v=i->vars[k].value;*v;v++) if(!emit(b,&n,*v)) goto limit;
            continue;
        }
        if(!emit(b,&n,c)) goto limit;
    }
    if(depth||quote) return tg_set_result(i,"unterminated word",TG_ERROR);
    if(!delimiter(*p,nested)&&!continuation(p)) return tg_set_result(i,"extra characters after grouped word",TG_ERROR);
    b[n]=0; *cursor=p; return TG_OK;
limit:
    return tg_set_result(i,"word too long",TG_ERROR);
}
int tg_eval_script(tg_interp *i,const char **cursor,int nested) {
    if(i->depth>=i->recursion_limit) return tg_set_result(i,"evaluation depth exceeded",TG_ERROR);
    const char *p=*cursor;
    /* Most GUI callbacks have only a few words. Allocate 32 KiB initially,
       grow geometrically as needed; nested evaluations own separate storage. */
    int outer=i->depth==0;
    size_t capacity=outer&&i->scratch_words?i->scratch_words:8;
    char *storage=outer&&i->scratch?i->scratch:malloc(capacity*MAX_WORD);
    if(!storage) return tg_set_result(i,"out of memory",TG_ERROR);
    if(outer) { i->scratch=storage; i->scratch_words=capacity; }
    i->depth++;
    tg_set_result(i,"",TG_OK);
    char *a[MAX_ARGS]; int status=TG_OK;
    while(*p) {
        while(*p) {
            if(continuation(p)) p=after_continuation(p);
            else if(delimiter(*p,nested)&&!(nested&&*p==']')) p++;
            else break;
        }
        if(!*p || (nested&&*p==']')) break;
        if(*p=='#') {
            while(*p&&*p!='\n') {
                if(continuation(p)) p=after_continuation(p);
                else if(*p=='\\'&&p[1]) p+=2;
                else p++;
            }
            continue;
        }
        if(i->deleted) { status=tg_set_result(i,"attempt to call eval in deleted interpreter",TG_ERROR); goto done; }
        size_t n=0;
        while(*p && *p!=';' && *p!='\n' && !(nested&&*p==']')) {
            if(continuation(p)) { p=after_continuation(p); continue; }
            if(isspace((unsigned char)*p)) { p++; continue; }
            if(n==MAX_ARGS) { status=tg_set_result(i,"argument limit",TG_ERROR); goto done; }
            if(n==capacity) {
                size_t next=capacity*2;
                if(next>MAX_ARGS) next=MAX_ARGS;
                char *grown=realloc(storage,next*MAX_WORD);
                if(!grown) { status=tg_set_result(i,"out of memory",TG_ERROR); goto done; }
                storage=grown; capacity=next;
                if(outer) { i->scratch=storage; i->scratch_words=capacity; }
                for(size_t k=0;k<n;k++) a[k]=storage+k*MAX_WORD;
            }
            a[n]=storage+n*MAX_WORD;
            if((status=word(i,&p,a[n],nested))!=TG_OK) goto done;
            n++;
        }
        if((status=tg_execute(i,n,a))!=TG_OK) goto done;
    }
    if(nested) {
        if(*p!=']') status=tg_set_result(i,"unclosed command substitution",TG_ERROR);
        else p++;
    }
done:
    i->depth--;
    if(!outer) free(storage);
    *cursor=p;
    return status;
}
