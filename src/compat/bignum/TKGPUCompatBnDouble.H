/* Convert a finite binary double to a truncated integer in base 2^28.
   Tcl 8.6 maps NaN to zero and rejects either infinity. */
#ifndef BN_DOUBLE_API
#define BN_DOUBLE_API Tcl_InitBignumFromDouble
#endif
int BN_DOUBLE_API(Tcl_Interp *i,double value,mp_int *out) {
    if(!out) return TG_ERROR;
    if(isinf(value)) {
        if(i) Tcl_SetObjResult(i,Tcl_NewStringObj("integer value too large to represent",-1));
        return TG_ERROR;
    }
    mp_int a;
    if(TclBN_mp_init(&a)!=TKGPU_MP_OKAY) return TG_ERROR;
    double remaining=isnan(value)?0.0:floor(fabs(value));
    const double base=268435456.0;
    while(remaining>=1.0) {
        if(tg_bn_reserve(&a,a.used+1)!=TKGPU_MP_OKAY) {
            TclBN_mp_clear(&a);return TG_ERROR;
        }
        a.dp[a.used++]=(uint32_t)fmod(remaining,base);
        remaining=floor(remaining/base);
    }
    a.sign=a.used&&signbit(value)?1:0;
    *out=a;return TG_OK;
}
#undef BN_DOUBLE_API
