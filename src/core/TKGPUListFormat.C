#include "TKGPUList.H"
#include <string.h>
static int special(char c) {
    return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\v'||c=='\f'||strchr("{}[]$;\\\"",c)!=NULL;
}
static int put(char *out,size_t cap,size_t *n,char c) {
    if(*n>=cap-1) return 0;
    out[(*n)++]=c; return 1;
}
int tg_list_append(char *out,size_t cap,const char *e) {
    if(!out||!cap||!e) return 0;
    size_t n=0; while(n<cap&&out[n]) n++;
    if(n==cap) return 0;
    size_t start=n,depth=0; int needs=!*e||*e=='#',balanced=1;
    for(const char *p=e;*p;p++) {
        if(special(*p)) needs=1;
        if(*p=='\\') {
            if(!p[1]||p[1]=='\n') balanced=0;
            if(p[1]) p++;
        } else if(*p=='{') depth++;
        else if(*p=='}') { if(!depth) balanced=0; else depth--; }
    }
    if(depth) balanced=0;
    if(n&&!put(out,cap,&n,' ')) return 0;
    if(needs&&balanced) {
        if(!put(out,cap,&n,'{')) goto fail;
        for(const char *p=e;*p;p++) if(!put(out,cap,&n,*p)) goto fail;
        if(!put(out,cap,&n,'}')) goto fail;
    } else for(const char *p=e;*p;p++) {
        char c=*p,esc=0;
        if(c=='\n') esc='n'; else if(c=='\t') esc='t'; else if(c=='\r') esc='r';
        else if(c=='\v') esc='v'; else if(c=='\f') esc='f';
        if(esc) {
            if(!put(out,cap,&n,'\\')||!put(out,cap,&n,esc)) goto fail;
        } else {
            if((special(c)||(p==e&&c=='#'))&&!put(out,cap,&n,'\\')) goto fail;
            if(!put(out,cap,&n,c)) goto fail;
        }
    }
    out[n]=0; return 1;
fail:
    out[start]=0; return 0;
}
