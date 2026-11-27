#include "TKGPUUtf8.H"
int tg_utf8_decode(const unsigned char *s,size_t n,uint32_t *scalar,size_t *used) {
    if(!s||!n||!scalar||!used) return TG_ERROR;
    unsigned c=s[0]; uint32_t v; size_t width;
    if(c<0x80) { v=c; width=1; }
    else if(c>=0xc2&&c<=0xdf) { v=c&31u; width=2; }
    else if(c>=0xe0&&c<=0xef) { v=c&15u; width=3; }
    else if(c>=0xf0&&c<=0xf4) { v=c&7u; width=4; }
    else return TG_ERROR;
    if(n<width) return TG_ERROR;
    for(size_t k=1;k<width;k++) {
        if((s[k]&0xc0u)!=0x80u) return TG_ERROR;
        v=(v<<6)|(s[k]&63u);
    }
    if((width==2&&v<0x80)||(width==3&&v<0x800)||(width==4&&v<0x10000)||
       (v>=0xd800&&v<=0xdfff)||v>0x10ffff) return TG_ERROR;
    *scalar=v; *used=width; return TG_OK;
}
int tg_utf8_count(const unsigned char *s,size_t n,size_t *count) {
    if((!s&&n)||!count) return TG_ERROR;
    size_t k=0,total=0;
    while(k<n) {
        uint32_t v; size_t width;
        if(tg_utf8_decode(s+k,n-k,&v,&width)!=TG_OK) return TG_ERROR;
        k+=width; total++;
    }
    *count=total; return TG_OK;
}

int tg_utf16_feed(uint16_t *pending,uint16_t unit,uint32_t *scalar) {
    if(!pending||!scalar) return -1;
    uint16_t high=*pending;
    *pending=0;
    if(unit>=0xd800&&unit<=0xdbff) { *pending=unit; return 0; }
    if(unit>=0xdc00&&unit<=0xdfff) {
        if(high<0xd800||high>0xdbff) return -1;
        *scalar=0x10000u+((uint32_t)(high-0xd800u)<<10)+(unit-0xdc00u);
    } else *scalar=unit;
    return 1;
}
