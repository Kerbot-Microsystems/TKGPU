/* Independent bounded brace parser; no evaluation or substitution. */
#ifndef TKGPU_PARSE_API
#define TKGPU_PARSE_API(n) Tcl_##n
#endif
void TKGPU_PARSE_API(FreeParse)(Tcl_Parse *p) {
    if(p->tokenPtr && p->tokenPtr!=p->staticTokens) Tcl_Free((char *)p->tokenPtr);
    p->tokenPtr=p->staticTokens;
}
static void brace_token(Tcl_Parse *p,int type,const char *start,int len) {
    if(p->numTokens==p->tokensAvailable) {
        if(p->tokensAvailable>INT_MAX/2 || (size_t)p->tokensAvailable*2>UINT_MAX/sizeof(Tcl_Token)) Tcl_Panic("brace token overflow");
        int capacity=p->tokensAvailable*2;
        Tcl_Token *tokens=(Tcl_Token *)Tcl_Alloc((unsigned)((size_t)capacity*sizeof *tokens));
        memcpy(tokens,p->tokenPtr,(size_t)p->numTokens*sizeof *tokens);
        if(p->tokenPtr!=p->staticTokens) Tcl_Free((char *)p->tokenPtr);
        p->tokenPtr=tokens; p->tokensAvailable=capacity;
    }
    p->tokenPtr[p->numTokens++]=(Tcl_Token){type,start,len,0};
}
int TKGPU_PARSE_API(ParseBraces)(Tcl_Interp *i,const char *start,int n,Tcl_Parse *p,int append,const char **term) {
    if(!start||!p) return TCL_ERROR;
    if(n<0) { size_t len=strlen(start); if(len>INT_MAX) return TCL_ERROR; n=(int)len; }
    if(!append) {
        memset(p,0,sizeof *p); p->string=start; p->end=start+n; p->interp=i;
        p->term=p->end; p->tokenPtr=p->staticTokens; p->tokensAvailable=NUM_STATIC_TOKENS;
    }
    if(n==0||*start!='{') goto missing;
    const char *end=start+n,*cur=start+1,*literal=cur;
    int depth=1,initial=p->numTokens;
    while(cur<end) {
        if(*cur=='\\'&&cur+1<end) {
            if(cur[1]=='\n') {
                if(cur>literal) brace_token(p,TCL_TOKEN_TEXT,literal,(int)(cur-literal));
                const char *bs=cur; cur+=2;
                while(cur<end&&(*cur==' '||*cur=='\t')) ++cur;
                brace_token(p,TCL_TOKEN_BS,bs,(int)(cur-bs)); literal=cur;
            } else cur+=2;
            continue;
        }
        if(*cur=='{') ++depth;
        else if(*cur=='}'&&!--depth) {
            if(cur>literal||p->numTokens==initial) brace_token(p,TCL_TOKEN_TEXT,literal,(int)(cur-literal));
            if(term) *term=cur+1;
            return TCL_OK;
        }
        ++cur;
    }
missing:
    p->errorType=TCL_PARSE_MISSING_BRACE; p->incomplete=1; p->term=start;
    if(i) Tcl_SetObjResult(i,Tcl_NewStringObj("missing close-brace",-1));
    TKGPU_PARSE_API(FreeParse)(p); return TCL_ERROR;
}
#undef TKGPU_PARSE_API
