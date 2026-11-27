#include "TKGPUUtf8.H"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(void) {
    const uint32_t points[]={0,0x7f,0x80,0x7ff,0x800,0x44f,0xd7ff,0xe000,0xffff,0x10000,0x1f600,0x10ffff};
    for(size_t k=0;k<sizeof(points)/sizeof(points[0]);k++) {
        unsigned char b[4]; size_t n=0,used=0; uint32_t v=0;
        CHECK(tg_utf8_encode(points[k],b,sizeof b,&n)==TG_OK);
        CHECK(tg_utf8_decode(b,n,&v,&used)==TG_OK&&v==points[k]&&used==n);
        if(n>1) CHECK(tg_utf8_decode(b,n-1,&v,&used)==TG_ERROR);
    }
    const unsigned char invalid[][4]={{0xc0,0x80},{0xed,0xa0,0x80},{0xf4,0x90,0x80,0x80},{0xe0,0x80,0x80},{0x80},{0xc2,0x41}};
    uint32_t v=123; size_t n=456;
    for(size_t k=0;k<sizeof(invalid)/sizeof(invalid[0]);k++)
        CHECK(tg_utf8_decode(invalid[k],4,&v,&n)==TG_ERROR&&v==123&&n==456);
    unsigned char out[4]={0};
    CHECK(tg_utf8_encode(0xd800,out,4,&n)==TG_ERROR);
    CHECK(tg_utf8_encode(0x110000,out,4,&n)==TG_ERROR);
    CHECK(tg_utf8_encode(0x1f600,out,3,&n)==TG_ERROR);
    const unsigned char text[]={0x41,0,0xd0,0xaf,0xf0,0x9f,0x98,0x80};
    CHECK(tg_utf8_count(text,sizeof text,&n)==TG_OK&&n==4);
    CHECK(tg_utf8_count(NULL,0,&n)==TG_OK&&n==0);
    uint16_t high=0; uint32_t scalar=0;
    CHECK(tg_utf16_feed(&high,0x0416,&scalar)==1&&scalar==0x0416&&!high);
    CHECK(tg_utf16_feed(&high,0xd83d,&scalar)==0&&high==0xd83d);
    CHECK(tg_utf16_feed(&high,0xde00,&scalar)==1&&scalar==0x1f600&&!high);
    CHECK(tg_utf16_feed(&high,0xdc00,&scalar)==-1&&!high);
    CHECK(tg_utf16_feed(&high,0xd800,&scalar)==0);
    CHECK(tg_utf16_feed(&high,0x0041,&scalar)==1&&scalar==0x41&&!high);
    puts("UTF-8 checks passed"); return 0;
}
