#include "../../src/core/TKGPUExpression.H"
#include <stdio.h>
#include <stdint.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); return 1; } } while(0)
#define EXPR(s,v) do { CHECK(tg_expr_integer(s,&n)); CHECK(n==(v)); } while(0)
int main(void) {
    int64_t n=0;
    EXPR("2+3*4",14); EXPR("(2+3)*4",20); EXPR("-7/3",-3);
    EXPR("7/-3",-3); EXPR("-7%3",2); EXPR("7%-3",-2);
    EXPR("0*-4",0); EXPR("9223372036854775807",INT64_MAX);
    EXPR("-9223372036854775807-1",INT64_MIN);
    CHECK(!tg_expr_integer("9223372036854775807+1",&n));
    CHECK(!tg_expr_integer("(-9223372036854775807-1)/-1",&n));
    CHECK(!tg_expr_integer("(-9223372036854775807-1)*-1",&n));
    CHECK(!tg_expr_integer("(-9223372036854775807-1)-1",&n));
    CHECK(!tg_expr_integer("3037000500*3037000500",&n));
    CHECK(!tg_expr_integer("1/0",&n));
    CHECK(!tg_expr_integer("1+",&n));
    CHECK(!tg_expr_integer("1 2",&n));
    CHECK(!tg_expr_integer("01",&n));
    CHECK(!tg_expr_integer("((1)",&n));
    CHECK(!tg_expr_integer("$x",&n));
    EXPR("2==2",1); EXPR("2!=2",0); EXPR("1+2<2*2",1);
    EXPR("2>=2 && 5>4",1); EXPR("!0 + 2",3);
    EXPR("0 && (1/0)",0); EXPR("1 || (1/0)",1);
    EXPR("1 || 0 && (1/0)",1); EXPR("(0 || 1) && (2 == 2)",1);
    CHECK(!tg_expr_integer("0 && (1/)",&n));
    CHECK(!tg_expr_integer("1 && (1/0)",&n));
    CHECK(!tg_expr_integer("0 || (1/0)",&n));
    puts("expression checks passed"); return 0;
}
