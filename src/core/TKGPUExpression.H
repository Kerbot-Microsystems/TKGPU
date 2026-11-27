#ifndef TKGPU_EXPRESSION_H
#define TKGPU_EXPRESSION_H
#include <stdint.h>
/* Bounded decimal integer expression subset. Returns 1 on success. */
int tg_expr_integer(const char *source,int64_t *value);
#endif
