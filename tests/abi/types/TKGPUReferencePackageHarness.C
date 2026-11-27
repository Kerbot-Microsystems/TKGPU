#define _DEFAULT_SOURCE
#include <tcl.h>
#define PKG_API(n) candidate_##n
#include "../../../src/compat/stubs/TKGPUReferencePackage.H"
#define PKG_API(n) candidate_##n
#include "../../../src/compat/stubs/TKGPUReferencePackageRequire.H"
const char *candidate_PkgRequireEx(Tcl_Interp *i,const char *name,const char *version,int exact,void *data){return pkg_require_one(i,name,version,exact,data);}
const char *candidate_PkgRequire(Tcl_Interp *i,const char *name,const char *version,int exact){return pkg_require_one(i,name,version,exact,NULL);}
