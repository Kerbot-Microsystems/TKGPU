#include "TKGPUCompat.H"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <zlib.h>

enum {
    ZLIB_FMT_RAW=1, ZLIB_FMT_ZLIB=2, ZLIB_FMT_GZIP=4, ZLIB_FMT_AUTO=8,
    ZLIB_MODE_DEFLATE=16, ZLIB_MODE_INFLATE=32
};

typedef struct {
    unsigned char *data;
    size_t len,cap;
} zbuf;

static void zbuf_push(zbuf *b,const void *src,size_t n) {
    if(!n) return;
    if(b->len+n>b->cap) {
        size_t next=b->cap?b->cap*2:256;
        while(next<b->len+n) next*=2;
        unsigned char *p=realloc(b->data,next);
        if(!p) Tcl_Panic("zlib buffer allocation failed");
        b->data=p; b->cap=next;
    }
    memcpy(b->data+b->len,src,n);
    b->len+=n;
}

static voidpf codec_alloc(voidpf opaque,uInt count,uInt size) {
    (void)opaque;
    if(size&&count>SIZE_MAX/size) return NULL;
    return calloc(count,size);
}
static void codec_free(voidpf opaque,voidpf address) { (void)opaque; free(address); }
static int codec(zbuf *out,const unsigned char *in,size_t len,int format,int level,int encode,unsigned int *checksum) {
    int bits;
    switch(format) {
    case ZLIB_FMT_RAW: bits=-MAX_WBITS; break;
    case ZLIB_FMT_ZLIB: bits=MAX_WBITS; break;
    case ZLIB_FMT_GZIP: bits=MAX_WBITS+16; break;
    case ZLIB_FMT_AUTO: if(encode) return 1; bits=MAX_WBITS+32; break;
    default: return 1;
    }
    if(len>UINT_MAX||level< -1||level>9) return 1;
    z_stream stream; memset(&stream,0,sizeof stream);
    stream.zalloc=codec_alloc; stream.zfree=codec_free;
    int rc=encode?deflateInit2(&stream,level,Z_DEFLATED,bits,8,Z_DEFAULT_STRATEGY):inflateInit2(&stream,bits);
    if(rc!=Z_OK) return 1;
    stream.next_in=(Bytef *)in; stream.avail_in=(uInt)len;
    unsigned char block[32768];
    do {
        stream.next_out=block; stream.avail_out=sizeof block;
        uInt before=stream.avail_in;
        rc=encode?deflate(&stream,Z_FINISH):inflate(&stream,Z_NO_FLUSH);
        size_t produced=sizeof block-stream.avail_out;
        if(produced>(size_t)INT_MAX-out->len) { rc=Z_MEM_ERROR; break; }
        zbuf_push(out,block,produced);
        if(rc==Z_STREAM_END) break;
        if(rc!=Z_OK||(!produced&&before==stream.avail_in)) { rc=Z_DATA_ERROR; break; }
    } while(1);
    if(checksum) *checksum=(unsigned int)stream.adler;
    if(encode) deflateEnd(&stream); else inflateEnd(&stream);
    return rc==Z_STREAM_END?0:1;
}
static int inflate_stored(zbuf *out,const unsigned char *in,size_t len,int format,unsigned int *checksum) {
    return codec(out,in,len,format,Z_DEFAULT_COMPRESSION,0,checksum);
}
static int deflate_stored(zbuf *out,const unsigned char *in,size_t len,int format,int level,unsigned int *checksum) {
    return codec(out,in,len,format,level,1,checksum);
}

int Tcl_ZlibDeflate(Tcl_Interp *i,int format,Tcl_Obj *data,int level,Tcl_Obj *gzipHeader) {
    (void)gzipHeader;
    if(!i||!data) return 1;
    int len=0;
    unsigned char *bytes=Tcl_GetByteArrayFromObj(data,&len);
    zbuf out={0};
    if(deflate_stored(&out,bytes,(size_t)(len>0?len:0),format,level,NULL)!=0) {
        free(out.data); Tcl_SetObjResult(i,Tcl_NewStringObj("invalid compression parameters",-1)); return 1;
    }
    Tcl_SetObjResult(i,Tcl_NewByteArrayObj(out.data,(int)out.len));
    free(out.data);
    return 0;
}

int Tcl_ZlibInflate(Tcl_Interp *i,int format,Tcl_Obj *data,int buffersize,Tcl_Obj *gzipHeader) {
    (void)buffersize; (void)gzipHeader;
    if(!i||!data) return 1;
    int len=0;
    unsigned char *bytes=Tcl_GetByteArrayFromObj(data,&len);
    zbuf out={0};
    if(inflate_stored(&out,bytes,(size_t)(len>0?len:0),format,NULL)!=0) {
        free(out.data);
        Tcl_SetObjResult(i,Tcl_NewStringObj("invalid compressed data",-1));
        return 1;
    }
    Tcl_SetObjResult(i,Tcl_NewByteArrayObj(out.data,(int)out.len));
    free(out.data);
    return 0;
}

struct Tcl_ZlibStream_ {
    Tcl_Interp *interp;
    int mode;
    int format;
    int level;
    int eof;
    unsigned int checksum;
    zbuf inbuf;
    zbuf outbuf;
    size_t out_pos;
    Tcl_Obj *cmd_name;
};

int Tcl_ZlibStreamInit(Tcl_Interp *i,int mode,int format,int level,Tcl_Obj *dictObj,Tcl_ZlibStream *handlePtr) {
    (void)dictObj;
    if(!handlePtr) return 1;
    *handlePtr=NULL;
    if((mode!=ZLIB_MODE_DEFLATE&&mode!=ZLIB_MODE_INFLATE)||level< -1||level>9||
       (format!=1&&format!=2&&format!=4&&!(format==8&&mode==ZLIB_MODE_INFLATE))) return 1;
    struct Tcl_ZlibStream_ *s=calloc(1,sizeof *s);
    if(!s) Tcl_Panic("cannot allocate zlib stream");
    s->interp=i;
    s->mode=mode;
    s->format=format; s->level=level;
    s->checksum=(s->format==ZLIB_FMT_GZIP)?0u:1u;
    s->cmd_name=Tcl_ObjPrintf("::tcl::zlib::stream%p",(void *)s);
    if(s->cmd_name) TKGPU_RetainObject(s->cmd_name);
    *handlePtr=s;
    return 0;
}

int Tcl_ZlibStreamClose(Tcl_ZlibStream s) {
    if(!s) return 0;
    free(s->inbuf.data);
    free(s->outbuf.data);
    if(s->cmd_name) TKGPU_ReleaseObject(s->cmd_name);
    free(s);
    return 0;
}

int Tcl_ZlibStreamReset(Tcl_ZlibStream s) {
    if(!s) return 1;
    s->inbuf.len=0;
    s->outbuf.len=0;
    s->out_pos=0;
    s->eof=0;
    s->checksum=(s->format==ZLIB_FMT_GZIP)?0u:1u;
    return 0;
}

Tcl_Obj *Tcl_ZlibStreamGetCommandName(Tcl_ZlibStream s) {
    return s?s->cmd_name:NULL;
}

int Tcl_ZlibStreamEof(Tcl_ZlibStream s) {
    return s?s->eof:1;
}

int Tcl_ZlibStreamChecksum(Tcl_ZlibStream s) {
    return s?(int)s->checksum:0;
}

int Tcl_ZlibStreamPut(Tcl_ZlibStream s,Tcl_Obj *data,int flush) {
    if(!s) return 1;
    if(data) {
        int len=0;
        unsigned char *bytes=Tcl_GetByteArrayFromObj(data,&len);
        if(len>0) zbuf_push(&s->inbuf,bytes,(size_t)len);
    }
    if(flush>=2) {
        if(s->mode==ZLIB_MODE_DEFLATE) {
            if(deflate_stored(&s->outbuf,s->inbuf.data,s->inbuf.len,s->format,s->level,&s->checksum)!=0) return 1;
            s->inbuf.len=0;
        } else {
            if(inflate_stored(&s->outbuf,s->inbuf.data,s->inbuf.len,s->format,&s->checksum)!=0) {
                return 1;
            }
            s->inbuf.len=0;
        }
        if(flush==4) s->eof=1;
    }
    return 0;
}

int Tcl_ZlibStreamGet(Tcl_ZlibStream s,Tcl_Obj *data,int count) {
    if(!s||!data) return 1;
    size_t avail=s->outbuf.len-s->out_pos;
    size_t take=(count<0||(size_t)count>avail)?avail:(size_t)count;
    Tcl_SetByteArrayObj(data,s->outbuf.data+s->out_pos,(int)take);
    s->out_pos+=take;
    if(s->out_pos==s->outbuf.len) { s->outbuf.len=0; s->out_pos=0; }
    return 0;
}

void Tcl_ZlibStreamSetCompressionDictionary(Tcl_ZlibStream s,Tcl_Obj *dictObj) {
    (void)s; (void)dictObj;
}
