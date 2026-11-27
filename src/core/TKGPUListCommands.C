#include "TKGPUCoreInternal.H"
#include "TKGPUList.H"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
int tg_list_command(tg_interp *i,size_t n,char **a) {
    char b[MAX_WORD]="",item[MAX_WORD],selected[MAX_WORD]="";
    if(!strcmp(a[0],"list")) {
        for(size_t k=1;k<n;k++) if(!tg_list_append(b,sizeof b,a[k]))
            return tg_set_result(i,"list too long",TG_ERROR);
        return tg_set_result_kind(i,b,TG_RESULT_LIST);
    }
    int index=!strcmp(a[0],"lindex"),length=!strcmp(a[0],"llength");
    if(!index&&!length) return -1;
    if((length&&n!=2)||(index&&n!=3)) return tg_set_result(i,"wrong # args",TG_ERROR);
    long wanted=-1; int last=0;
    if(index) {
        if(!strcmp(a[2],"end")) last=1;
        else {
            char *end; errno=0; wanted=strtol(a[2],&end,10);
            if(errno||end==a[2]||*end) return tg_set_result(i,"unsupported list index",TG_ERROR);
        }
    }
    const char *p=a[1]; long count=0; int rc;
    while((rc=tg_list_next(&p,item,sizeof item))>0) {
        if(last||count==wanted) strcpy(selected,item);
        count++;
    }
    if(rc<0) return tg_set_result(i,"invalid or unsupported list",TG_ERROR);
    if(index) return tg_set_result(i,selected,TG_OK);
    snprintf(b,sizeof b,"%ld",count); return tg_set_result_kind(i,b,TG_RESULT_INTEGER);
}
