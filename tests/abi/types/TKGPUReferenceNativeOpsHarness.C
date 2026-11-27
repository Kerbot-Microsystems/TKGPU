/* Isolated differential build: stock object allocation, independent path and cwd algorithms. */
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
#define TKGPU_SYSTEM_API(n) candidate_##n
#include "TKGPUReferenceSystem.H"
#define Tcl_GetCwd candidate_GetCwd
#define TKGPU_NATIVE_OP(n) candidate_##n
#include "TKGPUReferenceNativeOps.H"
