#include "TKGPUBuffer.H"
#include "TKGPUChecked.H"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(void) {
    size_t n=42;
    CHECK(tg_size_add(SIZE_MAX,1,&n)==TG_ERROR&&n==42);
    CHECK(tg_size_mul(SIZE_MAX,2,&n)==TG_ERROR&&n==42);
    CHECK(tg_size_mul(SIZE_MAX,0,&n)==TG_OK&&n==0);
    CHECK(tg_size_grow(0,100,99,&n)==TG_ERROR);
    CHECK(tg_size_grow(0,100,100,&n)==TG_OK&&n==100);
    tg_buffer b; tg_buffer_init(&b);
    const unsigned char bytes[]={1,0,2,3};
    CHECK(tg_buffer_append(&b,bytes,sizeof bytes)==TG_OK);
    for(int k=0;k<256;k++) CHECK(tg_buffer_append(&b,"a",1)==TG_OK);
    CHECK(b.length==260&&b.data[260]==0&&!memcmp(b.data,bytes,sizeof bytes));
    CHECK(tg_buffer_reserve(&b,SIZE_MAX)==TG_ERROR&&b.length==260);
    tg_buffer_clear(&b); CHECK(b.length==0&&b.data[0]==0);
    tg_buffer_destroy(&b); CHECK(!b.data&&!b.capacity&&!b.length);
    puts("checked memory and buffer checks passed"); return 0;
}
