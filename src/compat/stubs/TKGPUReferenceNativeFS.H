/* Native Linux paths only. No VFS registry, locale transcoding or cwd cache. */
#include <pwd.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#ifndef TKGPU_NATIVE_API
#define TKGPU_NATIVE_API(n) Tcl_##n
#endif
static char *native_home(const char *user,size_t len) {
    if(!len) {
        const char *home=getenv("HOME");
        if(home) return strdup(home);
    }
    if(len>=INT_MAX) return NULL;
    char *name=malloc(len+1); if(!name) return NULL;
    memcpy(name,user,len); name[len]=0;
    size_t cap=1024; char *buffer=NULL,*home=NULL;
    for(;;) {
        char *next=realloc(buffer,cap); if(!next) break; buffer=next;
        struct passwd record,*found=NULL;
        int rc=len?getpwnam_r(name,&record,buffer,cap,&found):getpwuid_r(getuid(),&record,buffer,cap,&found);
        if(rc==ERANGE&&cap<1048576) {cap*=2;continue;}
        if(!rc&&found&&found->pw_dir) home=strdup(found->pw_dir);
        break;
    }
    free(buffer); free(name); return home;
}
char *TKGPU_NATIVE_API(TranslateFileName)(Tcl_Interp *i,const char *name,Tcl_DString *d) {
    Tcl_DStringInit(d);
    if(!name) return NULL;
    if(*name!='~') {const char *parts[]={name};return Tcl_JoinPath(1,parts,d);}
    int saved=errno;
    const char *tail=strchr(name,'/'); if(!tail) tail=name+strlen(name);
    size_t n=(size_t)(tail-name-1);
    char *home=native_home(name+1,n);
    if(!home) {
        if(i) {
            Tcl_DString message;Tcl_DStringInit(&message);
            Tcl_DStringAppend(&message,"user \"",-1);
            if(n<INT_MAX) Tcl_DStringAppend(&message,name+1,(int)n);
            Tcl_DStringAppend(&message,"\" doesn't exist",-1);
            Tcl_SetObjResult(i,Tcl_NewStringObj(message.string,message.length));
            Tcl_DStringFree(&message);
        }
        errno=saved;return NULL;
    }
    while(*tail=='/') ++tail;
    Tcl_DString suffix; Tcl_DStringInit(&suffix);
    if(*tail=='~') Tcl_DStringAppend(&suffix,"./",2);
    Tcl_DStringAppend(&suffix,tail,-1);
    const char *parts[]={home,suffix.string};
    Tcl_JoinPath(*tail?2:1,parts,d);
    Tcl_DStringFree(&suffix);free(home);errno=saved;return d->string;
}
static int native_valid_path(const char *s) {
    /* Accept standard UTF-8 scalar values; reject invalid and modified NUL. */
    const unsigned char *p=(const unsigned char *)s;
    while(*p) {
        unsigned a=*p++;
        if(a<128) continue;
        unsigned n=a>=0xc2&&a<=0xdf?1:a>=0xe0&&a<=0xef?2:a>=0xf0&&a<=0xf4?3:0;
        if(!n||!*p||(a==0xe0&&*p<0xa0)||(a==0xed&&*p>=0xa0)||(a==0xf0&&*p<0x90)||(a==0xf4&&*p>=0x90)) return 0;
        while(n--) {if((*p&0xc0)!=0x80) return 0;++p;}
    }
    return 1;
}
static char *native_path(const char *s,Tcl_DString *d) {
    if(!s||!native_valid_path(s)) {Tcl_DStringInit(d);errno=EILSEQ;return NULL;}
    char *p=TKGPU_NATIVE_API(TranslateFileName)(NULL,s,d);
    if(p&&!native_valid_path(p)) {errno=EILSEQ;return NULL;}
    return p;
}
static const char *native_object_path(Tcl_Obj *o) {
    int n=0;const char *s=Tcl_GetStringFromObj(o,&n);
    if(!s||n<0||memchr(s,0,(size_t)n)) {errno=EILSEQ;return NULL;}
    return s;
}
int TKGPU_NATIVE_API(Access)(const char *s,int mode) {
    Tcl_DString d; char *p=native_path(s,&d);int rc=p?access(p,mode):-1;
    int err=errno;Tcl_DStringFree(&d);errno=err;return rc;
}
int TKGPU_NATIVE_API(Stat)(const char *s,struct stat *buf) {
    Tcl_DString d;char *p=native_path(s,&d);int rc=p?stat(p,buf):-1;
    if(rc==0) buf->st_atim.tv_nsec=buf->st_mtim.tv_nsec=buf->st_ctim.tv_nsec=0;
    int err=errno;Tcl_DStringFree(&d);errno=err;return rc;
}
int TKGPU_NATIVE_API(FSAccess)(Tcl_Obj *o,int mode) {
    const char *s=native_object_path(o);return s?TKGPU_NATIVE_API(Access)(s,mode):-1;
}
static int native_object_stat(Tcl_Obj *o,Tcl_StatBuf *buf,int follow) {
    const char *s=native_object_path(o);if(!s)return -1;
    Tcl_DString d;char *p=native_path(s,&d);int rc=-1;
    if(!p&&!follow&&errno!=EILSEQ) errno=EFAULT;
    if(p) {
#if defined(HAVE_STRUCT_STAT64)
        rc=follow?stat64(p,buf):lstat64(p,buf);
#else
        rc=follow?stat(p,buf):lstat(p,buf);
#endif
    }
    int err=errno;Tcl_DStringFree(&d);errno=err;return rc;
}
int TKGPU_NATIVE_API(FSStat)(Tcl_Obj *o,Tcl_StatBuf *buf) {return native_object_stat(o,buf,1);}
int TKGPU_NATIVE_API(FSLstat)(Tcl_Obj *o,Tcl_StatBuf *buf) {return native_object_stat(o,buf,0);}
#undef TKGPU_NATIVE_API
