#include "TKGPUUtf8.H"
int tg_utf8_encode(uint32_t v,unsigned char *out,size_t cap,size_t *used) {
    if(!out||!used||v>0x10ffff||(v>=0xd800&&v<=0xdfff)) return TG_ERROR;
    size_t n=v<0x80?1:v<0x800?2:v<0x10000?3:4;
    if(cap<n) return TG_ERROR;
    if(n==1) out[0]=(unsigned char)v;
    else {
        for(size_t k=n-1;k>0;k--) { out[k]=(unsigned char)(0x80u|(v&63u)); v>>=6; }
        out[0]=(unsigned char)((n==2?0xc0u:n==3?0xe0u:0xf0u)|v);
    }
    *used=n; return TG_OK;
}
