#include "TKGPUCompat.H"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void normalize_exponent(char *buf) {
    char *e=strpbrk(buf,"eE");
    if(!e) return;
    *e='e';
    char *digits=e+1;
    if(*digits=='+'||*digits=='-') ++digits;
    if(digits[0]=='0'&&digits[1]&&digits[2]&&!digits[3]) {
        memmove(digits,digits+1,3);
    }
}

void Tcl_PrintDouble(Tcl_Interp *interp,double value,char *dst) {
    (void)interp;
    if(!dst) return;
    if(isnan(value)) {
        strcpy(dst,signbit(value)?"-NaN":"NaN");
        return;
    }
    if(isinf(value)) {
        strcpy(dst,signbit(value)?"-Inf":"Inf");
        return;
    }
    if(value==0.0) {
        strcpy(dst,signbit(value)?"-0.0":"0.0");
        return;
    }
    char buf[64];
    buf[0]=0;
    for(int prec=1;prec<=17;prec++) {
        int n=snprintf(buf,sizeof buf,"%.*g",prec,value);
        if(n<0||n>=(int)sizeof buf-2) continue;
        if(strtod(buf,NULL)==value) break;
    }
    normalize_exponent(buf);
    if(!strpbrk(buf,".eE")) strcat(buf,".0");
    strcpy(dst,buf);
}

static void append_vprintf(Tcl_Obj *obj,const char *format,va_list source) {
    if(!obj||!format) return;
    va_list probe;
    va_copy(probe,source);
    int needed=vsnprintf(NULL,0,format,probe);
    va_end(probe);
    if(needed<=0) return;
    char small[256];
    if((size_t)needed<sizeof small) {
        va_list copy;
        va_copy(copy,source);
        int written=vsnprintf(small,sizeof small,format,copy);
        va_end(copy);
        if(written>0) Tcl_AppendToObj(obj,small,written);
        return;
    }
    char *heap=malloc((size_t)needed+1);
    if(!heap) Tcl_Panic("cannot allocate formatted string");
    va_list copy;
    va_copy(copy,source);
    int written=vsnprintf(heap,(size_t)needed+1,format,copy);
    va_end(copy);
    if(written>0) Tcl_AppendToObj(obj,heap,written);
    free(heap);
}

void Tcl_AppendPrintfToObj(Tcl_Obj *obj,const char *format,...) {
    va_list args;
    va_start(args,format);
    append_vprintf(obj,format,args);
    va_end(args);
}

Tcl_Obj *Tcl_ObjPrintf(const char *format,...) {
    Tcl_Obj *obj=Tcl_NewObj();
    if(!obj) return NULL;
    if(format) {
        va_list args;
        va_start(args,format);
        append_vprintf(obj,format,args);
        va_end(args);
    }
    return obj;
}

static int fmt_err(Tcl_Interp *i,const char *msg) {
    if(i) Tcl_SetObjResult(i,Tcl_NewStringObj(msg,-1));
    return 1;
}

int Tcl_AppendFormatToObj(Tcl_Interp *i,Tcl_Obj *obj,const char *format,int objc,Tcl_Obj *const objv[]) {
    if(!obj||!format) return fmt_err(i,"invalid format arguments");
    int argIndex=0,usePos=-1;
    const char *p=format;
    while(*p) {
        const char *start=p;
        while(*p&&*p!='%') ++p;
        if(p>start) Tcl_AppendToObj(obj,start,(int)(p-start));
        if(!*p) break;
        ++p;
        if(*p=='%') { Tcl_AppendToObj(obj,"%",1); ++p; continue; }
        /* Check positional n$ */
        int curArg=-1;
        const char *q=p;
        if(*q>='0'&&*q<='9') {
            long pos=0;
            while(*q>='0'&&*q<='9') { pos=pos*10+(*q-'0'); ++q; }
            if(*q=='$') {
                if(usePos==0) return fmt_err(i,"cannot mix \"%\" and \"%n$\" conversion specifiers");
                usePos=1;
                if(pos<1||pos>objc) return fmt_err(i,"\"%n$\" argument index out of range");
                curArg=(int)(pos-1);
                p=q+1;
            }
        }
        if(curArg<0) {
            if(usePos==1) return fmt_err(i,"cannot mix \"%\" and \"%n$\" conversion specifiers");
            usePos=0;
        }
        /* Flags */
        char spec[64];
        int sn=0;
        spec[sn++]='%';
        while(*p=='-'||*p=='+'||*p==' '||*p=='0'||*p=='#') {
            if(sn<32) spec[sn++]=*p;
            ++p;
        }
        /* Width */
        int hasWidth=0,width=0;
        if(*p=='*') {
            ++p;
            if(argIndex>=objc) return fmt_err(i,"not enough arguments for all format specifiers");
            if(Tcl_GetIntFromObj(i,objv[argIndex++],&width)!=0) return 1;
            hasWidth=1;
        } else {
            while(*p>='0'&&*p<='9') { if(sn<40) spec[sn++]=*p; ++p; }
        }
        /* Precision */
        int hasPrec=0,prec=0;
        if(*p=='.') {
            ++p;
            if(*p=='*') {
                ++p;
                if(argIndex>=objc) return fmt_err(i,"not enough arguments for all format specifiers");
                if(Tcl_GetIntFromObj(i,objv[argIndex++],&prec)!=0) return 1;
                hasPrec=1;
            } else {
                if(sn<42) spec[sn++]='.';
                while(*p>='0'&&*p<='9') { if(sn<50) spec[sn++]=*p; ++p; }
            }
        }
        /* Length modifiers */
        while(*p=='l'||*p=='h'||*p=='j'||*p=='z'||*p=='t') ++p;
        char conv=*p;
        if(!conv) return fmt_err(i,"format string ended in middle of field specifier");
        ++p;
        if(curArg<0) {
            if(argIndex>=objc) return fmt_err(i,"not enough arguments for all format specifiers");
            curArg=argIndex++;
        }
        Tcl_Obj *val=objv[curArg];
        if(hasWidth) sn+=snprintf(spec+sn,sizeof spec-(size_t)sn,"%d",width);
        if(hasPrec) sn+=snprintf(spec+sn,sizeof spec-(size_t)sn,".%d",prec);
        if(conv=='s') {
            spec[sn++]='s'; spec[sn]=0;
            const char *s=Tcl_GetString(val);
            Tcl_AppendPrintfToObj(obj,spec,s?s:"");
        } else if(conv=='c') {
            int ch=0;
            if(Tcl_GetIntFromObj(i,val,&ch)!=0) return 1;
            char utf[8];
            int ulen=Tcl_UniCharToUtf(ch,utf);
            utf[ulen]=0;
            spec[sn++]='s'; spec[sn]=0;
            Tcl_AppendPrintfToObj(obj,spec,utf);
        } else if(conv=='d'||conv=='i') {
            long long n=0;
            if(Tcl_GetWideIntFromObj(i,val,&n)!=0) return 1;
            spec[sn++]='l'; spec[sn++]='l'; spec[sn++]='d'; spec[sn]=0;
            Tcl_AppendPrintfToObj(obj,spec,n);
        } else if(conv=='u'||conv=='x'||conv=='X'||conv=='o') {
            long long n=0;
            if(Tcl_GetWideIntFromObj(i,val,&n)!=0) return 1;
            spec[sn++]='l'; spec[sn++]='l'; spec[sn++]=conv; spec[sn]=0;
            Tcl_AppendPrintfToObj(obj,spec,(unsigned long long)n);
        } else if(conv=='e'||conv=='E'||conv=='f'||conv=='F'||conv=='g'||conv=='G') {
            double d=0.0;
            if(Tcl_GetDoubleFromObj(i,val,&d)!=0) return 1;
            spec[sn++]=conv; spec[sn]=0;
            Tcl_AppendPrintfToObj(obj,spec,d);
        } else {
            if(i) Tcl_SetObjResult(i,Tcl_ObjPrintf("bad field specifier \"%c\"",conv));
            return 1;
        }
    }
    return 0;
}

Tcl_Obj *Tcl_Format(Tcl_Interp *i,const char *format,int objc,Tcl_Obj *const objv[]) {
    Tcl_Obj *obj=Tcl_NewObj();
    if(!obj) return NULL;
    if(Tcl_AppendFormatToObj(i,obj,format,objc,objv)!=0) {
        TclFreeObj(obj);
        return NULL;
    }
    return obj;
}
