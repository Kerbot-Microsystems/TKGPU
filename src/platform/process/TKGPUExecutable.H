#ifndef TKGPU_EXECUTABLE_H
#define TKGPU_EXECUTABLE_H
#include <stddef.h>
/* Returns nonzero only when a complete native executable path is obtained. */
int tg_executable_path(char *,size_t);
#endif
