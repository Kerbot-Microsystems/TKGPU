#include <tcl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define TKGPU_PARSE_API(n) candidate_##n
#include "TKGPUReferenceParse.H"
static uint32_t state=813;
static uint32_t next(void){state^=state<<13;state^=state>>17;state^=state<<5;return state;}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"failure line %d case %d\n",__LINE__,k);return 1;}}while(0)
int main(int argc,char **argv){
    (void)argc; Tcl_FindExecutable(argv[0]);
    static const unsigned char alphabet[]="{}abc\\\n\t \0\xc3\xa9";
    for(int k=0;k<150000;k++){
        char b[140];int off=k>=100000?3:0;
        if(off)memcpy(b,"{x}",3);
        b[off]='{';int n=off+1+(int)(next()%128);
        for(int j=off+1;j<n;j++)b[j]=(char)alphabet[next()%(sizeof alphabet-1)];
        if(next()%2)b[n++]='}';
        b[n]=0;Tcl_Parse p,q;const char *u=NULL,*v=NULL;
        int a=candidate_ParseBraces(NULL,b,n,&p,0,&u);
        int c=Tcl_ParseBraces(NULL,b,n,&q,0,&v);
        if(off){CHECK(a==TCL_OK&&c==TCL_OK&&u==b+off&&v==u);
            a=candidate_ParseBraces(NULL,b+off,n-off,&p,1,&u);
            c=Tcl_ParseBraces(NULL,b+off,n-off,&q,1,&v);}
        CHECK(a==c&&u==v&&p.numTokens==q.numTokens&&p.errorType==q.errorType&&p.incomplete==q.incomplete&&p.term==q.term&&p.end==q.end&&p.string==q.string);
        if(a==TCL_OK)for(int j=0;j<p.numTokens;j++){
            Tcl_Token *x=p.tokenPtr+j,*y=q.tokenPtr+j;
            CHECK(x->type==y->type&&x->size==y->size&&x->start==y->start&&x->numComponents==y->numComponents);
        }
        candidate_FreeParse(&p);Tcl_FreeParse(&q);
    }
    for(int k=0;k<100000;k++){
        int n=1+(int)(next()%256);char *b=malloc((size_t)n);CHECK(b);
        b[0]='{';for(int j=1;j<n;j++)b[j]=(char)alphabet[next()%(sizeof alphabet-1)];
        Tcl_Parse p;const char *end=NULL;
        if(candidate_ParseBraces(NULL,b,n,&p,0,&end)==TCL_OK){
            CHECK(end>b&&end<=b+n);
            for(int j=0;j<p.numTokens;j++){Tcl_Token *t=p.tokenPtr+j;CHECK(t->start>=b&&t->size>=0&&t->start+t->size<=b+n);}
        }
        candidate_FreeParse(&p);free(b);
    }
    puts("150000 brace differential cases and 100000 exact-sized buffers passed");return 0;
}
