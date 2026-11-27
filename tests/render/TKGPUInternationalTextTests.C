#include "TKGPUText.H"
#include "TKGPUUi.H"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
/* Source stays ASCII for older MSVC codepage settings. */
static const struct { const char *lang,*text; } samples[]={
 {"en","Settings"},
 {"uk","\xd0\x9d\xd0\xb0\xd0\xbb\xd0\xb0\xd1\x88\xd1\x82\xd1\x83\xd0\xb2\xd0\xb0\xd0\xbd\xd0\xbd\xd1\x8f"},
 {"fr","Param\xc3\xa8tres"}, {"de","Gr\xc3\xb6\xc3\x9f""e"},
 {"it","Velocit\xc3\xa0"}, {"es","Configuraci\xc3\xb3n"},
 {"pl","\xc5\x81\xc4\x85""czno\xc5\x9b\xc4\x87"},
 {"ar","\xd8\xa7\xd9\x84\xd8\xa5\xd8\xb9\xd8\xaf\xd8\xa7\xd8\xaf\xd8\xa7\xd8\xaa"},
 {"he","\xd7\x94\xd7\x92\xd7\x93\xd7\xa8\xd7\x95\xd7\xaa"},
 {"zh","\xe8\xae\xbe\xe7\xbd\xae"},
 {"ja","\xe8\xa8\xad\xe5\xae\x9a"},
 {"ko","\xec\x84\xa4\xec\xa0\x95"}
};
int main(void) {
    CHECK(strcmp(tg_text_native_backend(),"bitmap"));
    tg_canvas *canvas=tg_canvas_create(512,128); CHECK(canvas);
    for(size_t k=0;k<sizeof samples/sizeof samples[0];k++) {
        int w,h; fprintf(stderr,"Testing %s\n",samples[k].lang);
        CHECK(tg_text_native_measure(samples[k].text,2,&w,&h)==TG_OK&&w>0&&h>0&&w<=512&&h<=128);
        tg_canvas_clear(canvas,0);
        CHECK(tg_text_native_draw(canvas,(tg_rect){0,0,512,128},0,0,samples[k].text,2,0xffffff)==TG_OK);
        const uint32_t *p=tg_canvas_pixels(canvas);
        size_t lit=0;
        for(size_t j=0;j<512u*128u;j++) if(p[j]) lit++;
        CHECK(lit>0);
    }
    /* Hebrew logical start is on the right. */
    const char *rtl="\xd7\x90\xd7\x91\xd7\x92";
    int start,end; size_t byte;
    CHECK(tg_text_native_caret(rtl,2,0,&start)==TG_OK);
    CHECK(tg_text_native_caret(rtl,2,strlen(rtl),&end)==TG_OK);
    CHECK(start>end);
    CHECK(tg_text_native_hit(rtl,2,start-1,&byte)==TG_OK&&byte==0);
    CHECK(tg_text_native_hit(rtl,2,end+1,&byte)==TG_OK&&byte==strlen(rtl));
    CHECK(tg_text_native_caret(rtl,2,1,&start)==TG_ERROR);
    CHECK(tg_text_native_caret("",1,0,&start)==TG_OK&&start==0);
    CHECK(tg_text_native_hit("",1,100,&byte)==TG_OK&&byte==0);
    tg_canvas_clear(canvas,0);
    CHECK(tg_text_native_selection(canvas,(tg_rect){0,0,512,128},0,0,rtl,2,0,2,0x718eb9)==TG_OK);
    const uint32_t *selected=tg_canvas_pixels(canvas);
    size_t colored=0;
    for(size_t j=0;j<512u*128u;j++) if(selected[j]) colored++;
    CHECK(colored>0);
    CHECK(tg_text_native_selection(canvas,(tg_rect){0,0,512,128},0,0,rtl,2,1,2,0xffffff)==TG_ERROR);
    CHECK(tg_text_font_selection(canvas,(tg_rect){0,0,512,128},0,0,rtl,"",1,2,0xffffff)==TG_ERROR);
    if(!strcmp(tg_text_native_backend(),"pango-cairo")||!strcmp(tg_text_native_backend(),"uniscribe")) {
        size_t visual=0;
        CHECK(tg_text_font_move(rtl,"",&visual,-1)==TG_OK&&visual==2);
        CHECK(tg_text_font_move(rtl,"",&visual,1)==TG_OK&&visual==0);
        CHECK(tg_text_font_move(rtl,"",&visual,1)==TG_OK&&visual==0);
        visual=strlen(rtl);
        CHECK(tg_text_font_move(rtl,"",&visual,1)==TG_OK&&visual==4);
        tg_ui *ui=tg_ui_create(); CHECK(ui); char out[128],script[128];
        char *make[]={"text",".bidi"}; CHECK(tg_ui_command(ui,2,make,out,sizeof out)==TG_OK);
        char *insert[]={".bidi","insert","end","\xd7\x90\xd7\x91\xd7\x92\nABCDEF"};
        CHECK(tg_ui_command(ui,4,insert,out,sizeof out)==TG_OK);
        char *mark[]={".bidi","mark","set","insert","1.0"};
        CHECK(tg_ui_command(ui,5,mark,out,sizeof out)==TG_OK);
        tg_id id=tg_ui_find(ui,".bidi"); CHECK(id); (void)tg_ui_focus(ui,id);
        CHECK(tg_ui_apply_event(ui,(tg_event){TG_KEY_DOWN,id,0,0,TG_KEY_LEFT},script,sizeof script));
        char *index[]={".bidi","index","insert"};
        CHECK(tg_ui_command(ui,3,index,out,sizeof out)==TG_OK&&!strcmp(out,"1.1"));
        CHECK(tg_ui_apply_event(ui,(tg_event){TG_KEY_DOWN,id,0,0,TG_KEY_RIGHT},script,sizeof script));
        CHECK(tg_ui_command(ui,3,index,out,sizeof out)==TG_OK&&!strcmp(out,"1.0"));
        CHECK(tg_ui_apply_event(ui,(tg_event){TG_KEY_DOWN,id,0,0,TG_KEY_ARROW_DOWN},script,sizeof script));
        CHECK(tg_ui_command(ui,3,index,out,sizeof out)==TG_OK&&!strncmp(out,"2.",2));
        char *clear[]={".bidi","delete","1.0","end"}; CHECK(tg_ui_command(ui,4,clear,out,sizeof out)==TG_OK);
        char *rows[]={".bidi","insert","end","ABCDEFG\nA\nABCDEFG"};
        CHECK(tg_ui_command(ui,4,rows,out,sizeof out)==TG_OK);
        mark[4]="1.5"; CHECK(tg_ui_command(ui,5,mark,out,sizeof out)==TG_OK);
        CHECK(tg_ui_apply_event(ui,(tg_event){TG_KEY_DOWN,id,0,0,TG_KEY_ARROW_DOWN},script,sizeof script));
        CHECK(tg_ui_command(ui,3,index,out,sizeof out)==TG_OK&&!strcmp(out,"2.1"));
        CHECK(tg_ui_apply_event(ui,(tg_event){TG_KEY_DOWN,id,0,0,TG_KEY_ARROW_DOWN},script,sizeof script));
        CHECK(tg_ui_command(ui,3,index,out,sizeof out)==TG_OK&&!strcmp(out,"3.5"));
        tg_ui_destroy(ui);
        if(!strcmp(tg_text_native_backend(),"pango-cairo")||!strcmp(tg_text_native_backend(),"uniscribe")) {
            const char *mixed="abc \xd7\x90\xd7\x91\xd7\x92 123 xyz";
            tg_visual_cursor state={0}; size_t logical=0;
            CHECK(tg_text_font_visual_hit(mixed,"",0,&logical,&state)==TG_OK&&state.valid);
            size_t steps=0,limit=64;
            for(;steps<limit;steps++) {
                size_t old=logical; tg_visual_cursor previous=state;
                CHECK(tg_text_font_visual_move(mixed,"",&logical,1,&state)==TG_OK);
                CHECK(logical<=strlen(mixed)&&((unsigned char)mixed[logical]&0xc0)!=0x80);
                CHECK(state.valid&&state.offset==logical);
                if(old==logical&&previous.index==state.index&&previous.trailing==state.trailing&&previous.weak==state.weak) break;
            }
            CHECK(steps>5&&steps<limit);
            for(steps=0;steps<limit;steps++) {
                size_t old=logical; tg_visual_cursor previous=state;
                CHECK(tg_text_font_visual_move(mixed,"",&logical,-1,&state)==TG_OK);
                CHECK(logical<=strlen(mixed)&&((unsigned char)mixed[logical]&0xc0)!=0x80);
                CHECK(state.valid&&state.offset==logical);
                if(old==logical&&previous.index==state.index&&previous.trailing==state.trailing&&previous.weak==state.weak) break;
            }
            CHECK(steps>5&&steps<limit);
        }
        int w1,h1,w2,h2;
        CHECK(tg_text_font_measure("MMMM","Sans -10",&w1,&h1)==TG_OK);
        CHECK(tg_text_font_measure("MMMM","Sans -20",&w2,&h2)==TG_OK);
        CHECK(w2>w1&&h2>h1);
    }
    tg_canvas_destroy(canvas); puts("12 language samples rasterized"); return 0;
}
