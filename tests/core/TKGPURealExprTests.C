#include "../../src/core/numeric/TKGPURealExpr.H"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
static const char *lookup(void *ctx,const char *name) { (void)ctx; return !strcmp(name,"x")?"2.5":NULL; }
int main(void) {
    double v;
    CHECK(tg_expr_real("(2.5+1)*2",lookup,NULL,&v)&&v==7.0);
    CHECK(tg_expr_real("3/2+0.5",NULL,NULL,&v)&&v==1.5);
    CHECK(tg_expr_real("-3/2+0.5",NULL,NULL,&v)&&v==-1.5);
    CHECK(tg_expr_real("$x/2+1e-1",lookup,NULL,&v)&&v==1.35);
    CHECK(tg_expr_real(".25 * -4",NULL,NULL,&v)&&v==-1.0);
    CHECK(!tg_expr_real("2.0/0",NULL,NULL,&v));
    CHECK(!tg_expr_real("2.0 + 077",NULL,NULL,&v));
    CHECK(!tg_expr_real("1e+",NULL,NULL,&v));
    CHECK(!tg_expr_real("2.0 + [set x 1]",NULL,NULL,&v));
    CHECK(!tg_expr_real("$bad+1.0",lookup,NULL,&v));
    puts("real expression subset passed"); return 0;
}
