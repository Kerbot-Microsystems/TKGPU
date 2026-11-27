/* Linux native path syntax only; no virtual filesystem or tilde expansion. */
Tcl_PathType Tcl_GetPathType(const char *path) {
    return path&&(*path=='/'||*path=='~')?TCL_PATH_ABSOLUTE:TCL_PATH_RELATIVE;
}
void Tcl_SplitPath(const char *path,int *argc,const char ***argv) {
    if(!path||!argc||!argv) Tcl_Panic("invalid SplitPath arguments");
    size_t count=0,bytes=0;
    for(int pass=0;pass<2;pass++) {
        const char *p=path;
        size_t k=0,used=0;
        char *out=pass?(char *)(*argv+count+1):NULL;
        if(*p=='/') {
            if(pass) { (*argv)[k]=out+used; memcpy(out+used,"/",2); }
            ++k; used+=2;
        }
        while(*p) {
            while(*p=='/') ++p;
            if(!*p) break;
            const char *start=p;
            while(*p&&*p!='/') ++p;
            size_t n=(size_t)(p-start),prefix=k&&*start=='~'?2:0;
            if(k>=INT_MAX||n>UINT_MAX-3u||used>UINT_MAX-n-prefix-1u) Tcl_Panic("SplitPath overflow");
            if(pass) {
                (*argv)[k]=out+used;
                if(prefix) memcpy(out+used,"./",2);
                memcpy(out+used+prefix,start,n); out[used+prefix+n]=0;
            }
            ++k; used+=prefix+n+1;
        }
        if(!pass) {
            count=k; bytes=used;
            if(count>=UINT_MAX/sizeof(char *)||bytes>UINT_MAX-(count+1)*sizeof(char *)) Tcl_Panic("SplitPath overflow");
            *argv=(const char **)Tcl_Alloc((unsigned)((count+1)*sizeof(char *)+bytes));
        } else { (*argv)[count]=NULL; *argc=(int)count; }
    }
}

/* Stage the joined suffix independently: caller arguments can alias result. */
char *Tcl_JoinPath(int argc,const char *const argv[],Tcl_DString *result) {
    Tcl_DString tmp; Tcl_DStringInit(&tmp);
    for(int k=0;k<argc;k++) {
        const char *p=argv[k];
        if(!*p) continue;
        int tilde=*p=='~';
        if(*p=='/'||tilde) Tcl_DStringSetLength(&tmp,0);
        else if(tmp.length && p[0]=='.' && p[1]=='/' && p[2]=='~') p+=2;
        if(*p=='/') {
            int escapedTilde=p[1]=='.'&&p[2]=='/'&&p[3]=='~'&&(k+1<argc||strstr(p,"//")||p[strlen(p)-1]=='/');
            Tcl_DStringAppend(&tmp,"/",1);
            while(*p=='/') ++p;
            if(escapedTilde) p+=2;
        }
        while(*p) {
            const char *a=p; while(*p&&*p!='/') ++p;
            if(tmp.length&&tmp.string[tmp.length-1]!='/') Tcl_DStringAppend(&tmp,"/",1);
            if(p-a>INT_MAX) Tcl_Panic("JoinPath overflow");
            Tcl_DStringAppend(&tmp,a,(int)(p-a));
            if(tilde&&*p=='/') Tcl_DStringAppend(&tmp,"/",1);
            tilde=0;
            while(*p=='/') ++p;
        }
    }
    Tcl_DStringAppend(result,tmp.string,tmp.length);
    Tcl_DStringFree(&tmp); return result->string;
}
