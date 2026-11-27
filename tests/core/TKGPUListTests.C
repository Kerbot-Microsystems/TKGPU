#include "TKGPURuntime.H"
#include "../../src/core/TKGPUList.H"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
#define EVAL(s,v) do { CHECK(tg_eval(i,s)==TG_OK); CHECK(!strcmp(tg_result(i),v)); } while(0)
int main(void) {
    tg_interp *i=tg_create(); CHECK(i);
    EVAL("list alpha {two words} {}","alpha {two words} {}");
    EVAL("llength [list alpha {two words} {}]","3");
    EVAL("lindex [list alpha {two words} {}] 1","two words");
    EVAL("lindex {a b c} end","c");
    EVAL("lindex {a b c} 9","");
    EVAL("lindex {a b c} -1","");
    EVAL("llength {}","0");
    EVAL("lindex [list \"a\\{b\"] 0","a{b");
    CHECK(tg_eval(i,"llength {a {b}c}")==TG_ERROR);
    const char *values[]={"","a","two words","{balanced}","a{b","a}b","x\\","a\nb","$x","[x]","\"quoted\"","a;b"};
    for(size_t k=0;k<sizeof(values)/sizeof(values[0]);k++) {
        char encoded[256]="",decoded[256];
        CHECK(tg_list_append(encoded,sizeof encoded,values[k]));
        const char *p=encoded;
        CHECK(tg_list_next(&p,decoded,sizeof decoded)==1);
        CHECK(!strcmp(decoded,values[k]));
        CHECK(tg_list_next(&p,decoded,sizeof decoded)==0);
    }
    char tiny[4]="x";
    CHECK(!tg_list_append(tiny,sizeof tiny,"very long"));
    CHECK(!strcmp(tiny,"x"));
    tg_destroy(i); puts("list checks passed"); return 0;
}
