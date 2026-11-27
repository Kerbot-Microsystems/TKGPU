#include <tcl.h>
#define DICT_API(n) candidate_##n
#define DICT_TYPE candidate_type
#define DICT_REP(o) ((o)->internalRep.twoPtrValue.ptr1)
#define DICT_CLEAR(o) do { if((o)->typePtr&&(o)->typePtr->freeIntRepProc)(o)->typePtr->freeIntRepProc(o);(o)->typePtr=NULL; }while(0)
#include "../../../src/compat/types/TKGPUCompatDict.H"
