/* Independent registry for explicitly provided native packages.
   The Tcl bootstrap entry stays separate; no script loader is implied. */
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#ifndef PKG_API
#define PKG_API(n) Tcl_##n
#endif
typedef struct tg_pkg_entry {
    struct tg_pkg_entry *next;
    char *name,*version;
    const void *data;
} tg_pkg_entry;
typedef struct {tg_pkg_entry *first;} tg_pkg_registry;
#define TG_PKG_ASSOC "TKGPU.reference.packages.v1"
static void pkg_clear(void *data,Tcl_Interp *i) {
    (void)i;tg_pkg_registry *r=data;
    while(r->first){tg_pkg_entry *p=r->first;r->first=p->next;free(p->name);free(p->version);free(p);}
    free(r);
}
static tg_pkg_registry *pkg_registry(Tcl_Interp *i,int create) {
    tg_pkg_registry *r=Tcl_GetAssocData(i,TG_PKG_ASSOC,NULL);
    if(!r&&create){r=calloc(1,sizeof *r);if(!r)Tcl_Panic("package registry allocation failed");Tcl_SetAssocData(i,TG_PKG_ASSOC,pkg_clear,r);}
    return r;
}
static tg_pkg_entry *pkg_find(tg_pkg_registry *r,const char *name) {
    if(r)for(tg_pkg_entry *p=r->first;p;p=p->next)if(!strcmp(p->name,name))return p;
    return NULL;
}
/* Numeric segments are compared by normalized digit count, then bytes, so
   large version components never overflow a machine integer. Alpha and beta
   are ordered below zero-valued release components. */
typedef struct {const char *p;size_t n;int phase;} pkg_part;
typedef struct {pkg_part v[128];int n,pre;} pkg_version;
static int pkg_parse(const char *s,size_t len,pkg_version *v) {
    size_t at=0;v->n=0;v->pre=0;
    while(at<len) {
        if(v->n>=125||s[at]<'0'||s[at]>'9')return 0;
        size_t start=at;while(at<len&&s[at]>='0'&&s[at]<='9')++at;
        while(start<at&&s[start]=='0')++start;
        v->v[v->n++]=(pkg_part){s+start,at-start,0};
        if(at==len)return 1;
        char ch=s[at++];
        if(ch=='a'||ch=='b'){if(v->pre)return 0;v->pre=1;v->v[v->n++]=(pkg_part){NULL,0,ch=='a'?-2:-1};}
        else if(ch!='.')return 0;
        if(at==len)return 0;
    }
    return 0;
}
static int pkg_part_cmp(pkg_part a,pkg_part b) {
    if(a.phase!=b.phase)return a.phase<b.phase?-1:1;
    if(a.n!=b.n)return a.n<b.n?-1:1;
    int c=a.n?memcmp(a.p,b.p,a.n):0;return (c>0)-(c<0);
}
static int pkg_cmp(const pkg_version *a,const pkg_version *b) {
    pkg_part zero={NULL,0,0};int n=a->n>b->n?a->n:b->n;
    for(int j=0;j<n;j++){int c=pkg_part_cmp(j<a->n?a->v[j]:zero,j<b->n?b->v[j]:zero);if(c)return c;}
    return 0;
}
static void pkg_lower(pkg_version *v) {if(!v->pre)v->v[v->n++]=(pkg_part){NULL,0,-2};}
static void pkg_bad(Tcl_Interp *i,const char *s,size_t n,const char *prefix) {
    Tcl_DString d;Tcl_DStringInit(&d);Tcl_DStringAppend(&d,prefix,-1);Tcl_DStringAppend(&d,s,(int)n);Tcl_DStringAppend(&d,"\"",1);
    Tcl_SetObjResult(i,Tcl_NewStringObj(d.string,d.length));Tcl_DStringFree(&d);
}
static int pkg_satisfies(Tcl_Interp *i,const char *have,const char *need,int exact,int *as_exact,size_t *label_len) {
    *as_exact=exact;*label_len=need?strlen(need):0;
    if(!need)return 1;
    pkg_version h,l,u;const char *sep=exact?NULL:strchr(need,'-');size_t n=sep?(size_t)(sep-need):*label_len;
    if(sep&&strchr(sep+1,'-')){pkg_bad(i,need,*label_len,"expected versionMin-versionMax but got \"");return -1;}
    if(!pkg_parse(need,n,&l)){pkg_bad(i,need,n,"expected version number but got \"");return -1;}
    if(sep&&sep[1]) {
        if(!pkg_parse(sep+1,strlen(sep+1),&u)){pkg_bad(i,sep+1,strlen(sep+1),"expected version number but got \"");return -1;}
        if(!pkg_cmp(&l,&u)){
            if(n==strlen(sep+1)&&!memcmp(need,sep+1,n)){*as_exact=1;*label_len=n;}
            else {if(!pkg_parse(have,strlen(have),&h))return 0;return pkg_cmp(&h,&l)==0;}
        }
    }
    if(!pkg_parse(have,strlen(have),&h))return 0;
    if(*as_exact)return pkg_cmp(&h,&l)==0;
    pkg_lower(&l);
    if(pkg_cmp(&h,&l)<0)return 0;
    if(!sep)return pkg_part_cmp(h.v[0],l.v[0])==0;
    if(!sep[1])return 1;
    pkg_lower(&u);return pkg_cmp(&h,&u)<0;
}
int PKG_API(PkgProvideEx)(Tcl_Interp *i,const char *name,const char *version,const void *data) {
    if(!i||!name||!version)return 1;
    pkg_version v;
    if(!pkg_parse(version,strlen(version),&v)){pkg_bad(i,version,strlen(version),"expected version number but got \"");return 1;}
    tg_pkg_registry *r=pkg_registry(i,1);tg_pkg_entry *p=pkg_find(r,name);
    if(p) {
        pkg_version old;
        if(!pkg_parse(p->version,strlen(p->version),&old)||pkg_cmp(&old,&v)) {
            Tcl_ResetResult(i);Tcl_AppendResult(i,"conflicting versions provided for package \"",name,"\": ",p->version,", then ",version,(char *)NULL);return 1;
        }
        if(data)p->data=data;
        return 0;
    }
    p=calloc(1,sizeof *p);if(!p)Tcl_Panic("package allocation failed");
    p->name=strdup(name);p->version=strdup(version);if(!p->name||!p->version)Tcl_Panic("package allocation failed");
    p->data=data;p->next=r->first;r->first=p;return 0;
}
int PKG_API(PkgProvide)(Tcl_Interp *i,const char *name,const char *version){return PKG_API(PkgProvideEx)(i,name,version,NULL);}
static const char *pkg_present(Tcl_Interp *i,const char *name,const char *version,int exact,void *data,int requiring) {
    if(!i||!name)return NULL;
    tg_pkg_entry *p=pkg_find(pkg_registry(i,0),name);
    if(!p){
        Tcl_ResetResult(i);
        if(requiring)Tcl_AppendResult(i,"can't find package ",name,version?" ":"",version?version:"",(char *)NULL);
        else Tcl_AppendResult(i,"package ",name,version?" ":"",version?version:""," is not present",(char *)NULL);
        return NULL;
    }
    int as_exact;size_t n;int match=pkg_satisfies(i,p->version,version,exact,&as_exact,&n);
    if(match<0)return NULL;
    if(!match){Tcl_ResetResult(i);Tcl_AppendResult(i,"version conflict for package \"",name,"\": have ",p->version,", need ",as_exact?"exactly ":"",(char *)NULL);
        if(version){Tcl_DString d;Tcl_DStringInit(&d);Tcl_DStringAppend(&d,version,(int)n);Tcl_AppendResult(i,d.string,(char *)NULL);Tcl_DStringFree(&d);}return NULL;}
    if(data)*(const void **)data=p->data;
    return p->version;
}
const char *PKG_API(PkgPresentEx)(Tcl_Interp *i,const char *name,const char *version,int exact,void *data){return pkg_present(i,name,version,exact,data,0);}
const char *PKG_API(PkgPresent)(Tcl_Interp *i,const char *name,const char *version,int exact){return pkg_present(i,name,version,exact,NULL,0);}
#undef PKG_API
