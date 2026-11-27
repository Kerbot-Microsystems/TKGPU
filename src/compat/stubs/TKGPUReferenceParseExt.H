/* Remaining Tcl 8.6 parser and token evaluation slots. */
static void parse_init(Tcl_Parse *p,Tcl_Interp *i,const char *start,int n) {
    memset(p,0,sizeof *p);
    p->string=start;
    p->end=start+n;
    p->interp=i;
    p->term=p->end;
    p->tokenPtr=p->staticTokens;
    p->tokensAvailable=NUM_STATIC_TOKENS;
}

static int parse_var_name_char(unsigned char c) {
    return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_';
}

int Tcl_ParseVarName(Tcl_Interp *i,const char *start,int n,Tcl_Parse *p,int append) {
    if(!start||!p) return TCL_ERROR;
    if(n<0) n=(int)strlen(start);
    if(!append) parse_init(p,i,start,n);
    const char *end=start+n;
    if(n<=0||*start!='$') return TCL_OK;
    const char *cur=start+1;
    if(cur<end&&*cur=='{') {
        const char *close=memchr(cur+1,'}',(size_t)(end-(cur+1)));
        if(!close) {
            p->errorType=TCL_PARSE_MISSING_VAR_BRACE;
            p->incomplete=1;
            p->term=start;
            if(i) Tcl_SetObjResult(i,Tcl_NewStringObj("missing close-brace for variable name",-1));
            Tcl_FreeParse(p);
            return TCL_ERROR;
        }
        int varIdx=p->numTokens;
        brace_token(p,TCL_TOKEN_VARIABLE,start,(int)((close+1)-start));
        brace_token(p,TCL_TOKEN_TEXT,cur+1,(int)(close-(cur+1)));
        p->tokenPtr[varIdx].numComponents=1;
        return TCL_OK;
    }
    const char *nameStart=cur;
    while(cur<end) {
        if(parse_var_name_char((unsigned char)*cur)) ++cur;
        else if(cur+1<end&&cur[0]==':'&&cur[1]==':') { while(cur<end&&*cur==':') ++cur; }
        else break;
    }
    if(cur==nameStart) {
        brace_token(p,TCL_TOKEN_TEXT,start,1);
        return TCL_OK;
    }
    int varIdx=p->numTokens;
    brace_token(p,TCL_TOKEN_VARIABLE,start,(int)(cur-start));
    brace_token(p,TCL_TOKEN_TEXT,nameStart,(int)(cur-nameStart));
    int comps=1;
    if(cur<end&&*cur=='(') {
        const char *idxStart=cur+1;
        const char *close=memchr(idxStart,')',(size_t)(end-idxStart));
        if(!close) {
            p->errorType=TCL_PARSE_MISSING_PAREN;
            p->incomplete=1;
            p->term=start;
            if(i) Tcl_SetObjResult(i,Tcl_NewStringObj("missing )",-1));
            Tcl_FreeParse(p);
            return TCL_ERROR;
        }
        brace_token(p,TCL_TOKEN_TEXT,idxStart,(int)(close-idxStart));
        ++comps;
        cur=close+1;
        p->tokenPtr[varIdx].size=(int)(cur-start);
    }
    p->tokenPtr[varIdx].numComponents=comps;
    return TCL_OK;
}

CONST84_RETURN char *Tcl_ParseVar(Tcl_Interp *i,const char *start,CONST84 char **termPtr) {
    if(!i||!start||*start!='$') {
        if(termPtr) *termPtr=start;
        return NULL;
    }
    Tcl_Parse parse;
    if(Tcl_ParseVarName(i,start,-1,&parse,0)!=TCL_OK) {
        if(termPtr) *termPtr=start;
        return NULL;
    }
    if(parse.numTokens<=0||parse.tokenPtr[0].type!=TCL_TOKEN_VARIABLE) {
        if(termPtr) *termPtr=start+1;
        Tcl_FreeParse(&parse);
        return "$";
    }
    const char *after=parse.tokenPtr[0].start+parse.tokenPtr[0].size;
    if(termPtr) *termPtr=after;
    Tcl_Obj *part1=Tcl_NewStringObj(parse.tokenPtr[1].start,parse.tokenPtr[1].size);
    Tcl_IncrRefCount(part1);
    Tcl_Obj *part2=NULL;
    if(parse.tokenPtr[0].numComponents>1) {
        part2=Tcl_NewStringObj(parse.tokenPtr[2].start,parse.tokenPtr[2].size);
        Tcl_IncrRefCount(part2);
    }
    Tcl_Obj *val=Tcl_ObjGetVar2(i,part1,part2,TCL_LEAVE_ERR_MSG);
    Tcl_DecrRefCount(part1);
    if(part2) Tcl_DecrRefCount(part2);
    Tcl_FreeParse(&parse);
    return val?Tcl_GetString(val):NULL;
}

int Tcl_ParseQuotedString(Tcl_Interp *i,const char *start,int n,Tcl_Parse *p,int append,CONST84 char **termPtr) {
    if(!start||!p) return TCL_ERROR;
    if(n<0) n=(int)strlen(start);
    if(!append) parse_init(p,i,start,n);
    const char *end=start+n;
    if(n<=0||*start!='"') {
        p->errorType=TCL_PARSE_MISSING_QUOTE; p->incomplete=1; p->term=start;
        if(i) Tcl_SetObjResult(i,Tcl_NewStringObj("missing \"",-1));
        Tcl_FreeParse(p); return TCL_ERROR;
    }
    const char *cur=start+1,*lit=cur;
    while(cur<end) {
        if(*cur=='"') {
            if(cur>lit) brace_token(p,TCL_TOKEN_TEXT,lit,(int)(cur-lit));
            if(termPtr) *termPtr=cur+1;
            return TCL_OK;
        }
        if(*cur=='\\'&&cur+1<end) {
            if(cur>lit) brace_token(p,TCL_TOKEN_TEXT,lit,(int)(cur-lit));
            brace_token(p,TCL_TOKEN_BS,cur,2);
            cur+=2; lit=cur;
            continue;
        }
        if(*cur=='$') {
            if(cur>lit) brace_token(p,TCL_TOKEN_TEXT,lit,(int)(cur-lit));
            int before=p->numTokens;
            if(Tcl_ParseVarName(i,cur,(int)(end-cur),p,1)!=TCL_OK) return TCL_ERROR;
            cur+=p->tokenPtr[before].size; lit=cur;
            continue;
        }
        if(*cur=='[') {
            if(cur>lit) brace_token(p,TCL_TOKEN_TEXT,lit,(int)(cur-lit));
            int depth=1; const char *q=cur+1;
            while(q<end&&depth>0) {
                if(*q=='\\'&&q+1<end) { q+=2; continue; }
                if(*q=='[') ++depth;
                else if(*q==']') --depth;
                ++q;
            }
            if(depth!=0) {
                p->errorType=TCL_PARSE_MISSING_BRACKET; p->incomplete=1; p->term=cur;
                if(i) Tcl_SetObjResult(i,Tcl_NewStringObj("missing close-bracket",-1));
                Tcl_FreeParse(p); return TCL_ERROR;
            }
            brace_token(p,TCL_TOKEN_COMMAND,cur,(int)(q-cur));
            cur=q; lit=cur;
            continue;
        }
        ++cur;
    }
    p->errorType=TCL_PARSE_MISSING_QUOTE; p->incomplete=1; p->term=start;
    if(i) Tcl_SetObjResult(i,Tcl_NewStringObj("missing \"",-1));
    Tcl_FreeParse(p); return TCL_ERROR;
}

int Tcl_ParseCommand(Tcl_Interp *i,const char *start,int n,int nested,Tcl_Parse *p) {
    if(!start||!p) return TCL_ERROR;
    if(n<0) n=(int)strlen(start);
    parse_init(p,i,start,n);
    const char *cur=start,*end=start+n;
    while(cur<end&&(*cur==' '||*cur=='\t'||*cur=='\r'||*cur=='\n'||*cur==';')) ++cur;
    if(cur<end&&*cur=='#') {
        p->commentStart=cur;
        while(cur<end&&*cur!='\n') ++cur;
        if(cur<end) ++cur;
        p->commentSize=(int)(cur-p->commentStart);
        while(cur<end&&(*cur==' '||*cur=='\t'||*cur=='\r'||*cur=='\n')) ++cur;
    }
    p->commandStart=cur;
    while(cur<end) {
        if(*cur=='\n'||*cur==';'||(nested&&*cur==']')) {
            p->term=cur;
            p->commandSize=(int)((cur-p->commandStart)+((*cur==';'||*cur=='\n')?1:0));
            return TCL_OK;
        }
        if(*cur==' '||*cur=='\t'||*cur=='\r') { ++cur; continue; }
        int wordIdx=p->numTokens;
        brace_token(p,TCL_TOKEN_WORD,cur,0);
        const char *wordStart=cur;
        const char *after=cur;
        if(*cur=='{') {
            if(Tcl_ParseBraces(i,cur,(int)(end-cur),p,1,&after)!=TCL_OK) return TCL_ERROR;
            p->tokenPtr[wordIdx].type=TCL_TOKEN_SIMPLE_WORD;
        } else if(*cur=='"') {
            if(Tcl_ParseQuotedString(i,cur,(int)(end-cur),p,1,&after)!=TCL_OK) return TCL_ERROR;
        } else {
            while(after<end&&*after!=' '&&*after!='\t'&&*after!='\r'&&*after!='\n'&&*after!=';'&&!(nested&&*after==']')) {
                if(*after=='\\'&&after+1<end) after+=2;
                else ++after;
            }
            brace_token(p,TCL_TOKEN_TEXT,wordStart,(int)(after-wordStart));
            p->tokenPtr[wordIdx].type=TCL_TOKEN_SIMPLE_WORD;
        }
        p->tokenPtr[wordIdx].size=(int)(after-wordStart);
        p->tokenPtr[wordIdx].numComponents=p->numTokens-wordIdx-1;
        ++p->numWords;
        cur=after;
    }
    p->term=cur;
    p->commandSize=(int)(cur-p->commandStart);
    return TCL_OK;
}

int Tcl_ParseExpr(Tcl_Interp *i,const char *start,int n,Tcl_Parse *p) {
    if(!start||!p) return TCL_ERROR;
    if(n<0) n=(int)strlen(start);
    parse_init(p,i,start,n);
    p->commandStart=start;
    p->commandSize=n;
    if(n>0) {
        brace_token(p,TCL_TOKEN_SUB_EXPR,start,n);
        brace_token(p,TCL_TOKEN_TEXT,start,n);
        p->tokenPtr[0].numComponents=1;
    }
    return TCL_OK;
}

int Tcl_EvalTokensStandard(Tcl_Interp *i,Tcl_Token *tokens,int count) {
    if(!i) return TCL_ERROR;
    Tcl_Obj *res=Tcl_NewObj();
    Tcl_IncrRefCount(res);
    int rc=TCL_OK;
    for(int k=0;k<count&&rc==TCL_OK;k++) {
        Tcl_Token *t=&tokens[k];
        if(t->type==TCL_TOKEN_TEXT) {
            Tcl_AppendToObj(res,t->start,t->size);
        } else if(t->type==TCL_TOKEN_BS) {
            char buf[8]; int consumed=0;
            int n=Tcl_UtfBackslash(t->start,&consumed,buf);
            Tcl_AppendToObj(res,buf,n);
        } else if(t->type==TCL_TOKEN_COMMAND) {
            rc=Tcl_EvalEx(i,t->start+1,t->size>=2?t->size-2:0,0);
            if(rc==TCL_OK) Tcl_AppendObjToObj(res,Tcl_GetObjResult(i));
        } else if(t->type==TCL_TOKEN_VARIABLE) {
            Tcl_Obj *p1=Tcl_NewStringObj(tokens[k+1].start,tokens[k+1].size);
            Tcl_IncrRefCount(p1);
            Tcl_Obj *p2=NULL;
            if(t->numComponents>1) {
                p2=Tcl_NewStringObj(tokens[k+2].start,tokens[k+2].size);
                Tcl_IncrRefCount(p2);
            }
            Tcl_Obj *val=Tcl_ObjGetVar2(i,p1,p2,TCL_LEAVE_ERR_MSG);
            Tcl_DecrRefCount(p1);
            if(p2) Tcl_DecrRefCount(p2);
            if(!val) rc=TCL_ERROR;
            else Tcl_AppendObjToObj(res,val);
            k+=t->numComponents;
        }
    }
    if(rc==TCL_OK) Tcl_SetObjResult(i,res);
    Tcl_DecrRefCount(res);
    return rc;
}

Tcl_Obj *Tcl_EvalTokens(Tcl_Interp *i,Tcl_Token *tokens,int count) {
    if(Tcl_EvalTokensStandard(i,tokens,count)!=TCL_OK) return NULL;
    Tcl_Obj *res=Tcl_GetObjResult(i);
    Tcl_IncrRefCount(res);
    Tcl_ResetResult(i);
    return res;
}
