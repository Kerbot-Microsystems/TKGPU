#include "TKGPUCompat.H"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
char *Tcl_AttemptAlloc(unsigned int size) { return malloc(size?size:1); }
char *Tcl_AttemptRealloc(char *ptr,unsigned int size) { return realloc(ptr,size?size:1); }
char *Tcl_Alloc(unsigned int size) { char *p=Tcl_AttemptAlloc(size); if(!p) abort(); return p; }
char *Tcl_Realloc(char *ptr,unsigned int size) { char *p=Tcl_AttemptRealloc(ptr,size); if(!p) abort(); return p; }
void Tcl_Free(char *ptr) { free(ptr); }

static Tcl_PanicProc *panic_handler=NULL;
void Tcl_SetPanicProc(Tcl_PanicProc *proc) { panic_handler=proc; }

/* Tcl's unrecoverable error contract: diagnostic followed by termination. */
void Tcl_PanicVA(const char *format,va_list source) {
    va_list args; va_copy(args,source);
    if(panic_handler) {
        char buf[1024];
        vsnprintf(buf,sizeof buf,format?format:"Tcl_Panic",args);
        va_end(args);
        panic_handler("%s",buf);
        abort();
    }
    vfprintf(stderr,format?format:"Tcl_Panic",args);
    va_end(args); fputc('\n',stderr); fflush(stderr); abort();
}

void Tcl_Panic(const char *format,...) {
    va_list args; va_start(args,format);
    Tcl_PanicVA(format,args);
}

static Tcl_Mutex alloc_mutex;
Tcl_Mutex *Tcl_GetAllocMutex(void) { return &alloc_mutex; }
void Tcl_InitMemory(Tcl_Interp *interp) { (void)interp; }
int Tcl_DumpActiveMemory(const char *fileName) { (void)fileName; return 0; }
void Tcl_ValidateAllMemory(const char *file,int line) { (void)file; (void)line; }

int Tcl_GetErrno(void) { return errno; }
void Tcl_SetErrno(int value) { errno=value; }

#include <stdint.h>
/* Reflected CRC-32/ISO-HDLC, two nibble steps per byte. */
unsigned int Tcl_ZlibCRC32(unsigned int crc,const unsigned char *buf,int len) {
    static const uint32_t tab[16]={0x00000000u,0x1db71064u,0x3b6e20c8u,0x26d930acu,0x76dc4190u,0x6b6b51f4u,0x4db26158u,0x5005713cu,0xedb88320u,0xf00f9344u,0xd6d6a3e8u,0xcb61b38cu,0x9b64c2b0u,0x86d3d2d4u,0xa00ae278u,0xbdbdf21cu};
    if(!buf) return 0;
    uint32_t c=(uint32_t)crc^0xffffffffu;
    while(len>0) { c^=*buf++; c=(c>>4)^tab[c&15u]; c=(c>>4)^tab[c&15u]; --len; }
    return (unsigned int)(c^0xffffffffu);
}
unsigned int Tcl_ZlibAdler32(unsigned int adler,const unsigned char *buf,int len) {
    if(!buf) return 1;
    if(len<0) return adler;
    uint32_t a=(adler&65535u)%65521u,b=(adler>>16)%65521u;
    while(len>0) {
        int n=len>5552?5552:len; len-=n;
        while(n--) { a+=*buf++; b+=a; }
        a%=65521u; b%=65521u;
    }
    return (unsigned int)((b<<16)|a);
}
