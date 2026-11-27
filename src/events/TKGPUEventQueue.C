#include "../widgets/TKGPUWidgetInternal.H"
int tg_ui_post(tg_ui *u,tg_event e) {
    if(!u||((e.kind<TG_POINTER_DOWN||e.kind>TG_IME_REPLACE)&&!TG_IS_VIRTUAL(e.kind))||
       (e.target&&!tg_ui_get(u,e.target))) return TG_ERROR;
    if(e.kind==TG_CONFIGURE) for(size_t k=0;k<u->len;k++) {
        tg_event *pending=&u->queue[(u->head+k)%TG_EVENT_LIMIT];
        if(pending->kind==TG_CONFIGURE&&pending->target==e.target) { *pending=e; return TG_OK; }
    }
    if(u->len&&e.kind==TG_POINTER_MOVE) {
        tg_event *last=&u->queue[(u->head+u->len-1)%TG_EVENT_LIMIT];
        if(last->kind==TG_POINTER_MOVE&&last->key==e.key) { *last=e; return TG_OK; }
    }
    if(u->len&&e.kind==TG_WHEEL) {
        tg_event *last=&u->queue[(u->head+u->len-1)%TG_EVENT_LIMIT];
        uint32_t mod_mask=TG_MOD_SHIFT|TG_MOD_CONTROL|TG_MOD_ALT;
        if(last->kind==TG_WHEEL&&last->target==e.target&&((last->key&mod_mask)==(e.key&mod_mask))) {
            int d1=(int)(int16_t)(last->key&0xffffu);
            int d2=(int)(int16_t)(e.key&0xffffu);
            if((d1>0&&d2>0)||(d1<0&&d2<0)) {
                int sum=d1+d2;
                if(sum>1200) sum=1200;
                if(sum<-1200) sum=-1200;
                last->x=e.x; last->y=e.y;
                last->key=((uint32_t)(uint16_t)(int16_t)sum)|(e.key&mod_mask);
                return TG_OK;
            }
        }
    }
    if(u->len==TG_EVENT_LIMIT) return TG_ERROR;
    u->queue[(u->head+u->len)%TG_EVENT_LIMIT]=e; u->len++; return TG_OK;
}
int tg_ui_poll(tg_ui *u,tg_event *e) {
    if(!u||!e) return 0;
    while(u->len) {
        *e=u->queue[u->head]; u->head=(u->head+1)%TG_EVENT_LIMIT; u->len--;
        if(!e->target||tg_ui_get(u,e->target)) return 1;
    }
    return 0;
}
