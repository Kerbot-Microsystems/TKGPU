#ifndef TKGPU_LIST_H
#define TKGPU_LIST_H
#include <stddef.h>
/* Numeric backslash escapes; cursor starts after the initial escape letter. */
int tg_numeric_escape(char,const char **,char *,size_t,size_t *);
int tg_list_next(const char **cursor,char *out,size_t capacity);
int tg_list_append(char *out,size_t capacity,const char *element);
#endif
