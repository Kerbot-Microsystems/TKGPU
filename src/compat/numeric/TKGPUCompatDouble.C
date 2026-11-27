#include "TKGPUCompat.H"
#include "../../object/TKGPUObjectInternal.H"
#include "../../core/TKGPUCoreInternal.H"
#include "../types/TKGPUCompatTypes.H"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <locale.h>
#include <math.h>
#include <ctype.h>
static int decimal_locale(void) {
    return !strcmp(localeconv()->decimal_point,".");
}
int Tcl_GetDouble(Tcl_Interp *i,const char *s,double *out) {
    if(s&&out&&decimal_locale()) {
        char *end; errno=0; double v=strtod(s,&end);
        if(end!=s) {
            while(isspace((unsigned char)*end)) end++;
            if(!*end&&!errno&&isfinite(v)) { *out=v; return TG_OK; }
        }
    }
    if(i) tg_set_result(i,"unsupported or invalid finite double",TG_ERROR);
    return TG_ERROR;
}
int Tcl_GetDoubleFromObj(Tcl_Interp *i,Tcl_Obj *o,double *out) {
    if(o&&out&&o->typePtr==&tg_type_double) { *out=o->internalRep.real; return TG_OK; }
    size_t n; const char *s=tg_object_bytes(o,&n);
    if(!s||memchr(s,0,n)) return TG_ERROR;
    int rc=Tcl_GetDouble(i,s,out);
    if(rc==TG_OK) { tg_object_clear_type(o); o->typePtr=&tg_type_double; o->internalRep.real=*out; }
    return rc;
}
Tcl_Obj *Tcl_NewDoubleObj(double v) {
    if(!isfinite(v)||!decimal_locale()) return NULL;
    char b[64]; int n=snprintf(b,sizeof b,"%.17g",v);
    if(n<0||n>=(int)sizeof b-2) return NULL;
    if(!strpbrk(b,".eE")) { b[n++]='.'; b[n++]='0'; b[n]=0; }
    Tcl_Obj *o=tg_object_string(b,(size_t)n);
    if(!o) return NULL;
    o->typePtr=&tg_type_double; o->internalRep.real=v;
    return tg_compat_new_object(o);
}
