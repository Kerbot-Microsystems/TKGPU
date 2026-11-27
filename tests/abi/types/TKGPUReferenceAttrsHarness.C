#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif
#include <sys/stat.h>
#include <tcl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#define Tcl_GetPathType candidate_GetPathType
#define Tcl_SplitPath candidate_SplitPath
#define Tcl_JoinPath candidate_JoinPath
#include "TKGPUReferencePaths.H"
#define TKGPU_NATIVE_API(n) candidate_##n
#include "TKGPUReferenceNativeFS.H"
#define Tcl_FSStat candidate_FSStat
#define TKGPU_ATTR_API(n) candidate_##n
#include "TKGPUReferenceAttrs.H"
