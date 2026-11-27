#include "TKGPUFileHandlers.H"
#ifndef _WIN32
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#define MASK (TKGPU_READABLE|TKGPU_WRITABLE|TKGPU_EXCEPTION)
void Tcl_CreateFileHandler(int fd,int mask,Tcl_FileProc *fn,void *context) {
    if(fd<0||!fn) return;
    tg_notifier *s=tg_notifier_get(1);
    if(!s) return;
    for(size_t k=0;k<s->file_count;k++) if(s->files[k].fd==fd) {
        s->files[k]=(tg_notifier_file){fd,mask&MASK,fn,context}; return;
    }
    if(s->file_count==TG_NOTIFIER_LIMIT) return;
    s->files[s->file_count++]=(tg_notifier_file){fd,mask&MASK,fn,context};
}
void Tcl_DeleteFileHandler(int fd) {
    tg_notifier *s=tg_notifier_get(0);
    if(!s) return;
    for(size_t k=0;k<s->file_count;k++) if(s->files[k].fd==fd) {
        memmove(s->files+k,s->files+k+1,(s->file_count-k-1)*sizeof s->files[0]);
        s->file_count--; if(s->file_cursor>=s->file_count) s->file_cursor=0; return;
    }
}
int tg_file_pending(const tg_notifier *s) {
    for(size_t k=0;k<s->file_count;k++) if(s->files[k].mask) return 1;
    return 0;
}
int tg_file_dispatch(tg_notifier *s) {
    struct pollfd p[TG_NOTIFIER_LIMIT];
    size_t n=s->file_count;
    if(!n) return 0;
    for(size_t k=0;k<n;k++) {
        const tg_notifier_file *f=&s->files[k]; short events=0;
        if(f->mask&TKGPU_READABLE) events|=POLLIN;
        if(f->mask&TKGPU_WRITABLE) events|=POLLOUT;
        if(f->mask&TKGPU_EXCEPTION) events|=POLLPRI;
        p[k]=(struct pollfd){events?f->fd:-1,events,0};
    }
    if(poll(p,(nfds_t)n,0)<=0) return 0;
    for(size_t j=0;j<n;j++) {
        size_t k=(s->file_cursor+j)%n;
        short ready=p[k].revents; tg_notifier_file f=s->files[k]; int mask=0;
        if(ready&POLLNVAL) { s->files[k].mask=0; continue; }
        if(ready&(POLLIN|POLLHUP|POLLERR)) mask|=TKGPU_READABLE;
        if(ready&(POLLOUT|POLLERR)) mask|=TKGPU_WRITABLE;
        if(ready&POLLPRI) mask|=TKGPU_EXCEPTION;
        mask&=f.mask;
        if(mask) {
            s->file_cursor=(k+1)%n;
            f.fn(f.context,mask); return 1;
        }
    }
    return 0;
}
#endif
