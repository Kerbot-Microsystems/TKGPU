#include "TKGPUReferencePreserveHarness.C"
#include <stdio.h>
static int count;
static char a,b;
static void inner(char *p){if(p!=&b)abort();++count;}
static void outer(char *p){if(p!=&a)abort();++count;candidate_Preserve(&b);candidate_EventuallyFree(&b,inner);candidate_Release(&b);}
int main(void){for(int k=0;k<10000;k++){candidate_Preserve(&a);candidate_EventuallyFree(&a,outer);candidate_Release(&a);}if(count!=20000)return 1;puts("10000 reentrant free scenarios passed");return 0;}
