#ifndef TKGPU_REAL_EXPR_H
#define TKGPU_REAL_EXPR_H
/* Bounded arithmetic subset, decimal numbers and scalar numeric variables. */
typedef const char *(*tg_real_lookup)(void *,const char *);
int tg_expr_real(const char *s,tg_real_lookup lookup,void *context,double *out);
int tg_expr_real_typed(const char *,tg_real_lookup,void *,double *,int *);
#endif
