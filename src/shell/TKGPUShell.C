#include "TKGPURuntime.H"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv) {
    tg_interp *i=tg_create();
    if(!i) return 1;
    int status=0;
    if(argc==3 && !strcmp(argv[1],"-c")) {
        status=tg_eval(i,argv[2]); fprintf(status?stderr:stdout,"%s\n",tg_result(i));
    } else if(argc==1) {
        char line[8192];
        while(fputs("tkgpu> ",stdout),fflush(stdout),fgets(line,sizeof line,stdin)) {
            if(!strchr(line,'\n')&&!feof(stdin)) {
                int c; while((c=getchar())!='\n'&&c!=EOF) {}
                fputs("input too long\n",stderr); continue;
            }
            status=tg_eval(i,line); fprintf(status?stderr:stdout,"%s\n",tg_result(i));
        }
    } else { fprintf(stderr,"usage: tkgpu_shell [-c script]\n"); status=1; }
    tg_destroy(i); return status;
}
