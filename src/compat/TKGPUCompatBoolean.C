#include "TKGPUCompat.H"
#include "../core/TKGPUCoreInternal.H"
#include <ctype.h>
#include <string.h>
int Tcl_GetBoolean(Tcl_Interp *i,const char *s,int *value) {
    if(!s||!value) return TG_ERROR;
    static const char *words[]={"yes","no","true","false","on","off"};
    size_t len=strlen(s); int match=-1;
    if(len==1&&(s[0]=='0'||s[0]=='1')) { *value=s[0]=='1'; return TG_OK; }
    if(len) for(int k=0;k<6;k++) {
        if(len>strlen(words[k])) continue;
        size_t j=0;
        while(j<len&&tolower((unsigned char)s[j])==words[k][j]) j++;
        if(j==len) {
            if(match!=-1) { match=-2; break; }
            match=k;
        }
    }
    if(match>=0) { *value=(match%2)==0; return TG_OK; }
    if(i) tg_set_result(i,"expected boolean value",TG_ERROR);
    return TG_ERROR;
}
