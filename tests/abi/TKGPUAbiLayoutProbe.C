#include <stddef.h>
#include <stdio.h>
#ifdef TKGPU_REFERENCE_LAYOUT
#include <tcl.h>
typedef Tcl_Obj Probe;
#else
#include "TKGPUAbiObject.H"
typedef TKGPUAbiObjectPrefix Probe;
#endif
int main(void) {
    printf("%zu %zu %zu %zu %zu %zu %zu\n",
        sizeof(Probe),offsetof(Probe,refCount),offsetof(Probe,bytes),
        offsetof(Probe,length),offsetof(Probe,typePtr),offsetof(Probe,internalRep),
        sizeof(((Probe *)0)->internalRep));
    return 0;
}
