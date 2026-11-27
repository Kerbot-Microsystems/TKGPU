/* Native Linux file attributes. Names and permissions follow Tcl's public API. */
#include <grp.h>
#include <pwd.h>
#include <inttypes.h>
#ifndef TKGPU_ATTR_API
#define TKGPU_ATTR_API(n) Tcl_##n
#endif
static int attr_error(Tcl_Interp *i,const char *action,const char *path,const char *detail) {
    int saved=errno;
    if(i) {
        Tcl_DString d; Tcl_DStringInit(&d);
        Tcl_DStringAppend(&d,action,-1); Tcl_DStringAppend(&d," \"",2);
        Tcl_DStringAppend(&d,path?path:"",-1); Tcl_DStringAppend(&d,"\": ",3);
        Tcl_DStringAppend(&d,detail,-1); Tcl_SetObjResult(i,Tcl_NewStringObj(d.string,d.length));
        Tcl_DStringFree(&d);
    }
    errno=saved; return TCL_ERROR;
}
const char *const *TKGPU_ATTR_API(FSFileAttrStrings)(Tcl_Obj *path,Tcl_Obj **names) {
    (void)names;
    static const char *const attrs[]={"-group","-owner","-permissions",NULL};
    const char *p=Tcl_GetString(path); return p&&*p?attrs:NULL;
}
static char *attr_identity(int owner,uintmax_t id,const char *name,uintmax_t *resolved) {
    size_t size=1024; char *buffer=NULL,*answer=NULL;
    while(size<=1048576) {
        char *next=realloc(buffer,size); if(!next) break; buffer=next;
        int rc;
        if(owner) {
            struct passwd entry,*out=NULL;
            rc=name?getpwnam_r(name,&entry,buffer,size,&out):getpwuid_r((uid_t)id,&entry,buffer,size,&out);
            if(!rc&&out) { if(resolved)*resolved=out->pw_uid; answer=strdup(out->pw_name); }
        } else {
            struct group entry,*out=NULL;
            rc=name?getgrnam_r(name,&entry,buffer,size,&out):getgrgid_r((gid_t)id,&entry,buffer,size,&out);
            if(!rc&&out) { if(resolved)*resolved=out->gr_gid; answer=strdup(out->gr_name); }
        }
        if(rc!=ERANGE) break;
        size*=2;
    }
    free(buffer); return answer;
}
int TKGPU_ATTR_API(FSFileAttrsGet)(Tcl_Interp *i,int index,Tcl_Obj *path,Tcl_Obj **out) {
    if(!out||index<0||index>2) return TCL_ERROR;
    Tcl_StatBuf st;
    if(Tcl_FSStat(path,&st)!=0) return attr_error(i,"could not read",Tcl_GetString(path),Tcl_ErrnoMsg(errno));
    char text[64]; char *name=NULL;
    if(index==2) snprintf(text,sizeof text,"0%04o",(unsigned)(S_ISREG(st.st_mode)?st.st_mode&~S_IFMT:st.st_mode));
    else {
        uintmax_t id=index==1?(uintmax_t)st.st_uid:(uintmax_t)st.st_gid;
        name=attr_identity(index==1,id,NULL,NULL);
        snprintf(text,sizeof text,"%" PRIuMAX,id);
    }
    *out=Tcl_NewStringObj(name?name:text,-1); free(name);
    return *out?TCL_OK:TCL_ERROR;
}
static int attr_mode(const char *s,mode_t old,mode_t *out) {
    mode_t bits=old&07777;
    size_t n=strlen(s);
    if(n==9) {
        mode_t v=0; int valid=1;
        for(int k=0;k<3;k++) {
            int shift=6-3*k; const char *p=s+3*k;
            if(p[0]=='r') v|=4<<shift; else if(p[0]!='-') valid=0;
            if(p[1]=='w') v|=2<<shift; else if(p[1]!='-') valid=0;
            if(p[2]=='x') v|=1<<shift;
            else if(k<2&&(p[2]=='s'||p[2]=='S')) {v|=k?02000:04000;if(p[2]=='s')v|=1<<shift;}
            else if(k==2&&(p[2]=='t'||p[2]=='T')) {v|=01000;if(p[2]=='t')v|=1;}
            else if(p[2]!='-') valid=0;
        }
        if(valid) {*out=v;return 1;}
    }
    while(*s) {
        unsigned who=0;
        while(*s&&strchr("ugoa",*s)) {who|=*s=='u'?4:*s=='g'?2:*s=='o'?1:7;++s;}
        if(!*s) break;
        if(!who)who=7;
        char op=*s++;if(op!='+'&&op!='-'&&op!='=')return 0;
        mode_t value=0,mask=0;
        if(who&4)mask|=04700;
        if(who&2)mask|=02070;
        if(who&1)mask|=01007;
        while(*s&&*s!=',') {
            switch(*s++) {
                case 'r':value|=0444;break;
                case 'w':value|=0222;break;
                case 'x':value|=0111;break;
                case 's':value|=06000;break;
                case 't':value|=01000;break;
                default:return 0;
            }
        }
        value&=mask;
        if(op=='=')bits=(bits&~mask)|value;
        else if(op=='+')bits|=value;
        else bits&=~value;
        if(*s==',')++s;
    }
    *out=bits;return 1;
}
int TKGPU_ATTR_API(FSFileAttrsSet)(Tcl_Interp *i,int index,Tcl_Obj *path,Tcl_Obj *value) {
    if(index<0||index>2)return TCL_ERROR;
    const char *actions[]={"could not set group for file","could not set owner for file","could not set permissions for file"};
    const char *original=Tcl_GetString(path),*s=Tcl_GetString(value);
    const char *raw=native_object_path(path);
    Tcl_DString d;char *p=NULL;
    if(raw)p=native_path(raw,&d);else Tcl_DStringInit(&d);
    int rc=TCL_ERROR;
    if(!p||!*p) {if(p)errno=ENOENT;goto system_error;}
    if(index==2) {
        int number; mode_t mode;
        if(Tcl_GetIntFromObj(NULL,value,&number)==TCL_OK)mode=(mode_t)number;
        else {
            struct stat st;
            if(stat(p,&st)!=0)goto system_error;
            if(!attr_mode(s,st.st_mode,&mode)) {
                if(i) {Tcl_DString msg;Tcl_DStringInit(&msg);Tcl_DStringAppend(&msg,"unknown permission string format \"",-1);Tcl_DStringAppend(&msg,s,-1);Tcl_DStringAppend(&msg,"\"",1);Tcl_SetObjResult(i,Tcl_NewStringObj(msg.string,msg.length));Tcl_DStringFree(&msg);}
                goto done;
            }
        }
        if(chmod(p,mode)!=0)goto system_error;
    } else {
        int number;uintmax_t id;
        if(Tcl_GetIntFromObj(NULL,value,&number)==TCL_OK)id=(unsigned)number;
        else {
            char *name=attr_identity(index==1,0,s,&id);
            if(!name) {
                Tcl_DString msg;Tcl_DStringInit(&msg);Tcl_DStringAppend(&msg,index==1?"user \"":"group \"",-1);Tcl_DStringAppend(&msg,s,-1);Tcl_DStringAppend(&msg,"\" does not exist",-1);
                attr_error(i,actions[index],original,msg.string);Tcl_DStringFree(&msg);goto done;
            }
            free(name);
        }
        if(chown(p,index==1?(uid_t)id:(uid_t)-1,index==0?(gid_t)id:(gid_t)-1)!=0)goto system_error;
    }
    rc=TCL_OK;goto done;
system_error:
    attr_error(i,actions[index],original,Tcl_ErrnoMsg(errno));
done:
    {int saved=errno;Tcl_DStringFree(&d);errno=saved;}
    return rc;
}
#undef TKGPU_ATTR_API
