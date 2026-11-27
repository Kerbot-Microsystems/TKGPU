#include "TKGPUList.H"
#include "TKGPUUtf8.H"
#include <string.h>
static int ws(char c) { return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\v'||c=='\f'; }
static int put(char *out,size_t cap,size_t *n,char c) {
    if(*n>=cap-1) return 0;
    out[(*n)++]=c; return 1;
}
static int hex_digit(unsigned char c) {
    if(c>='0'&&c<='9') return c-'0';
    if(c>='a'&&c<='f') return c-'a'+10;
    if(c>='A'&&c<='F') return c-'A'+10;
    return -1;
}
/* Decode numeric escapes in unbraced list elements. Keep the core's
   strict UTF-8/NUL-terminated contract: modified UTF-8 NUL and isolated
   UTF-16 surrogate values cannot be represented by this interface. */
int tg_numeric_escape(char c,const char **cursor,char *out,size_t cap,size_t *n) {
    const char *p=*cursor;
    uint32_t value=0; unsigned digits=0,limit=0;
    if(c>='0'&&c<='7') {
        value=(uint32_t)(c-'0'); digits=1;
        limit=value<4?3:2;
        while(digits<limit&&*p>='0'&&*p<='7') {
            value=value*8u+(uint32_t)(*p++-'0'); digits++;
        }
    } else {
        limit=c=='x'?2:c=='u'?4:8;
        int d;
        while(digits<limit&&(d=hex_digit((unsigned char)*p))>=0) {
            value=value*16u+(uint32_t)d; p++; digits++;
        }
        if(!digits) return put(out,cap,n,c);
    }
    /* Tcl 8.6's BMP-oriented escape semantics are qualified only in this
       range. Reject rather than silently change unsupported code points. */
    if(!value||value>0xffffu||(value>=0xd800u&&value<=0xdfffu)) return 0;
    unsigned char bytes[4]; size_t used=0;
    if(tg_utf8_encode(value,bytes,sizeof bytes,&used)!=TG_OK||used>cap-1-*n) return 0;
    memcpy(out+*n,bytes,used); *n+=used; *cursor=p;
    return 1;
}
int tg_list_next(const char **cursor,char *out,size_t cap) {
    if(!cursor||!*cursor||!out||!cap) return -1;
    const char *p=*cursor; size_t n=0,depth=0;
    while(ws(*p)) p++;
    if(!*p) { *cursor=p; out[0]=0; return 0; }
    int brace=*p=='{',quote=*p=='"';
    if(brace||quote) p++;
    if(brace) depth=1;
    while(*p) {
        char c=*p++;
        if(brace) {
            if(c=='\\'&&*p) {
                if(!put(out,cap,&n,c)||!put(out,cap,&n,*p++)) return -1;
                continue;
            }
            if(c=='{') depth++;
            else if(c=='}'&&!--depth) { brace=0; break; }
        } else {
            if(quote&&c=='"') { quote=0; break; }
            if(!quote&&ws(c)) { p--; break; }
            if(c=='\\'&&*p) {
                c=*p++;
                switch(c) {
                case 'a': c='\a'; break; case 'b': c='\b'; break;
                case 'f': c='\f'; break; case 'n': c='\n'; break;
                case 'r': c='\r'; break; case 't': c='\t'; break;
                case 'v': c='\v'; break;
                case '\n': while(*p==' '||*p=='\t') p++; c=' '; break;
                case 'x': case 'u': case 'U':
                    if(!tg_numeric_escape(c,&p,out,cap,&n)) return -1;
                    continue;
                default: if(c>='0'&&c<='7') {
                    if(!tg_numeric_escape(c,&p,out,cap,&n)) return -1;
                    continue;
                }
                }
            }
        }
        if(!put(out,cap,&n,c)) return -1;
    }
    if(brace||quote||(*p&&!ws(*p))) return -1;
    out[n]=0; *cursor=p; return 1;
}
