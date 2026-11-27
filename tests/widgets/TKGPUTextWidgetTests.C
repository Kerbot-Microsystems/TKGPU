#include "TKGPUUi.H"
#include "../../src/render/async/TKGPUSnapshot.H"
#include <string.h>
#include <stdio.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
#define E(s,v) CHECK(tg_eval(i,s)==TG_OK&&!strcmp(tg_result(i),v))
static int scroll_regression(void) {
    tg_interp *i=tg_create();
    CHECK(i);
    CHECK(tg_eval(i,"text .t; place .t -width 100 -height 30; .t insert end {a\nb\nc\nd\ne\nf\ng\nh\ni\nj\nk\nl}")==TG_OK);
    CHECK(tg_ui_arrange(tg_interp_ui(i),100,100)==TG_OK);
    CHECK(tg_eval(i,".t yview moveto 1.0; .t yview")==TG_OK);
    CHECK(tg_result(i)[0]=='0'&&tg_result(i)[1]=='.');
    CHECK(tg_eval(i,".t yview moveto 0.0; .t yview")==TG_OK);
    CHECK(tg_result(i)[0]=='0'&&tg_result(i)[1]==' ');
    double first,last,oldlast;
    CHECK(sscanf(tg_result(i),"%lf %lf",&first,&oldlast)==2&&first==0);
    E(".t insert end {\nm\nn\no}","");
    CHECK(tg_eval(i,".t yview")==TG_OK);
    CHECK(sscanf(tg_result(i),"%lf %lf",&first,&last)==2&&first==0&&last<oldlast);
    E(".t delete 12.end end","");
    E(".t index end","13.0");
    CHECK(tg_eval(i,".t yview")==TG_OK);
    CHECK(sscanf(tg_result(i),"%lf %lf",&first,&last)==2&&first==0&&last==oldlast);
    E(".t delete 1.0 end","");
    E(".t yview","0 1");
    tg_destroy(i); return 0;
}
static int long_document_regression(void) {
    tg_interp *i=tg_create(); CHECK(i);
    E("text .doc; .doc tag configure heading -foreground blue","");
    for(int j=0;j<1500;j++) {
        CHECK(tg_eval(i,".doc insert end {some documentation text\n}")==TG_OK);
    }
    E(".doc insert end {Last heading} heading; .doc search {Last heading} 1.0 end","1501.0");
    E(".doc tag ranges heading","1501.0 1501.12");
    tg_ui *clone=tg_snapshot_create(tg_interp_ui(i)); CHECK(clone);
    E(".doc delete 1.0 end","");
    char *get[]={".doc","get","1501.0","1501.12"},out[64];
    CHECK(tg_ui_command(clone,4,get,out,sizeof out)==TG_OK&&!strcmp(out,"Last heading"));
    tg_ui_destroy(clone); tg_destroy(i); return 0;
}
static int mark_regression(void) {
    tg_interp *i=tg_create(); CHECK(i);
    E("text .m; .m insert end abcdef","");
    E(".m mark set leftmark 1.2","");
    E(".m mark set rightmark 1.2","");
    E(".m mark gravity leftmark left","");
    E(".m mark gravity leftmark","left");
    E(".m mark gravity rightmark","right");
    E(".m mark names","insert leftmark rightmark");
    E(".m insert leftmark XY","");
    E(".m index leftmark","1.2");
    E(".m index rightmark","1.4");
    E(".m index rightmark-1c","1.3");
    E(".m delete 1.0 1.3","");
    E(".m index leftmark","1.0");
    E(".m index rightmark","1.1");
    E(".m get 1.0 end-1c","Ycdef");
    tg_ui *copy=tg_snapshot_create(tg_interp_ui(i)); CHECK(copy);
    E(".m mark unset leftmark rightmark","");
    E(".m mark names","insert");
    CHECK(tg_eval(i,".m index rightmark")==TG_ERROR);
    char *query[]={".m","index","rightmark"}, out[64];
    CHECK(tg_ui_command(copy,3,query,out,sizeof out)==TG_OK&&!strcmp(out,"1.1"));
    tg_ui_destroy(copy); tg_destroy(i); return 0;
}
static int tag_neighbor_regression(void) {
    tg_interp *i=tg_create(); CHECK(i);
    E("text .n; .n insert end {abcdef\nghijkl}; .n tag add x 1.1 1.4 2.1 2.4","");
    E(".n tag nextrange x 1.2 end","2.1 2.4");
    E(".n tag nextrange x 1.0 1.2","1.1 1.4");
    E(".n tag nextrange x 1.1","1.1 1.4");
    E(".n tag nextrange x 1.4 end","2.1 2.4");
    E(".n tag prevrange x 2.2 1.0","2.1 2.4");
    E(".n tag prevrange x 2.1 1.0","1.1 1.4");
    E(".n tag prevrange x end 2.2","");
    E(".n tag prevrange x 2.2 2.2","");
    E(".n tag prevrange x 1.3 1.2","");
    E(".n tag nextrange missing 1.0","");
    E(".n tag ranges x","1.1 1.4 2.1 2.4");
    CHECK(tg_eval(i,".n tag nextrange x invalid")==TG_ERROR);
    tg_destroy(i); return 0;
}
static int selection_modified_regression(void) {
    tg_interp *i=tg_create(); CHECK(i);
    E("text .s; .s edit modified","0");
    E(".s insert end {abcdef\nghijkl}; .s edit modified","1");
    E(".s edit modified false","");
    E(".s tag add sel 1.1 1.3 2.1 2.4; .s index sel.first","1.1");
    E(".s index sel.last","2.4");
    E(".s index sel.first+1c","1.2");
    E(".s edit modified","0");
    E(".s delete 1.0 1.0; .s insert 1.0 {}; .s edit modified","0");
    E(".s configure -state disabled; .s delete 1.0 2.0; .s edit modified","0");
    E(".s configure -state normal; .s insert 1.0 X; .s edit modified","1");
    E(".s index sel.first","1.2");
    E(".s index sel.last","2.4");
    E(".s tag remove sel 1.0 end","");
    CHECK(tg_eval(i,".s index sel.first")==TG_ERROR);
    E(".s edit modified 0; .s edit modified","0");
    CHECK(tg_eval(i,".s edit modified invalid")==TG_ERROR);
    E(".s edit modified","0");
    tg_destroy(i); return 0;
}
static int compound_index_regression(void) {
    tg_interp *i=tg_create(); CHECK(i);
    E("text .idx; .idx insert end {abc\ndefgh\nij}","");
    E(".idx index {1.2 + 1 lines}","2.2");
    E(".idx index {2.4 - 1 lines}","1.3");
    E(".idx index {2.3 + 10 lines}","4.0");
    E(".idx index {2.3 - 10 lines}","1.3");
    E(".idx index {2.4 + 1 lines}","3.2");
    E(".idx index {end - 1 lines}","3.0");
    E(".idx index {1.2 +1l -1c}","2.1");
    E(".idx index {1.2 + 1 line lineend}","2.5");
    E(".idx index 1.2+1c+1c","2.0");
    E(".idx index {1.2 +2 chars -1 char}","1.3");
    E(".idx index {1.2 linestart +1c}","1.1");
    E(".idx index {1.2 + 1 lines linestart}","2.0");
    E(".idx index {1.2 +1c   }","1.3");
    E(".idx index {1.2 + 1 indices}","1.3");
    E(".idx index {2.3 + 1000000 lines}","4.0");
    E(".idx index {2.3 + 999999999999999999 chars}","4.0");
    CHECK(tg_eval(i,".idx index {1.2 + 1 invalid}")==TG_ERROR);
    E(".idx get 1.0 end-1c","abc\ndefgh\nij");
    tg_destroy(i); return 0;
}
static int undo_regression(void) {
    tg_interp *i=tg_create(); CHECK(i);
    E("text .u -undo true; .u cget -undo","1");
    E(".u cget -autoseparators","1");
    E(".u cget -maxundo","0");
    E(".u edit canundo","0");
    E(".u insert end a; .u insert end b; .u edit canundo","1");
    E(".u edit undo; .u get 1.0 end-1c","");
    E(".u edit modified","0");
    E(".u edit canredo","1");
    E(".u edit redo; .u get 1.0 end-1c","ab");
    E(".u edit separator; .u insert end {\xc3\xa9} x; .u edit separator","");
    E(".u delete 1.2 1.3; .u edit undo; .u get 1.0 end-1c","ab\xc3\xa9");
    E(".u tag ranges x","");
    E(".u edit redo; .u get 1.0 end-1c","ab");
    E(".u edit undo; .u insert end z; .u edit canredo","0");
    E(".u edit reset; .u edit canundo","0");
    CHECK(tg_eval(i,".u edit undo")==TG_ERROR);
    E(".u configure -maxundo 1; .u edit separator; .u insert end X; .u edit separator; .u insert end Y; .u edit undo; .u get 1.0 end-1c","ab\xc3\xa9zX");
    E(".u edit canundo","0");
    E(".u configure -autoseparators false; .u cget -autoseparators","0");
    CHECK(tg_eval(i,".u configure -undo invalid")==TG_ERROR);
    E(".u cget -undo","1");
    E(".u configure -undo false; .u insert end Q; .u edit canundo","0");
    E(".u configure -undo true; .u edit reset; .u edit modified false","");
    E(".u insert end Z; .u edit undo; .u edit modified","0");
    tg_ui *copy=tg_snapshot_create(tg_interp_ui(i)); CHECK(copy);
    E(".u edit redo; .u edit modified","1");
    tg_ui_destroy(copy);
    E("text .group -undo true; .group insert end abc; .group insert 1.0 x; .group edit undo; .group get 1.0 end-1c","");
    E(".group edit redo; .group get 1.0 end-1c","xabc");
    E(".group tag add x 1.0 1.4; .group edit reset; .group delete 1.1 1.2; .group edit undo; .group tag ranges x","1.0 1.4");
    E(".group edit reset; .group configure -autoseparators false; .group insert end A; .group insert 1.0 B; .group delete 1.0 1.1; .group edit undo; .group get 1.0 end-1c","xabc");
    tg_destroy(i); return 0;
}
int main(void) {
    CHECK(undo_regression()==0);
    CHECK(compound_index_regression()==0);
    CHECK(selection_modified_regression()==0);
    CHECK(tag_neighbor_regression()==0);
    CHECK(mark_regression()==0);
    CHECK(long_document_regression()==0);
    CHECK(scroll_regression()==0);
    tg_interp *i=tg_create(); CHECK(i);
    E("text .t; .t get 1.0 end","\n");
    char *insert[]={".t","insert","end","alpha\nbeta"};
    char out[64]; tg_ui *u=tg_interp_ui(i);
    CHECK(tg_ui_command(u,4,insert,out,sizeof out)==TG_OK);
    E(".t get 1.0 end-1c","alpha\nbeta");
    E(".t index end","3.0");
    E(".t count 1.0 end","11");
    E(".t count -chars 1.0 end-1c","10");
    E(".t count -chars -lines 1.0 end","11 2");
    E(".t count -lines -indices 2.0 1.0","-1 -6");
    E(".t count -chars 1.0 1.0","0");
    CHECK(tg_eval(i,".t count -displaychars 1.0 end")==TG_ERROR);
    E("text .unicode; .unicode insert end {A\xc3\xa9\xd0\x96}; .unicode count -chars 1.0 end-1c","3");
    E("destroy .unicode","");
    E(".t index 1.end","1.5");
    E(".t index {2.2 linestart}","2.0");
    E(".t index {2.2 lineend}","2.4");
    E(".t index {1.end-2c}","1.3");
    E(".t compare 1.0 < 2.0","1");
    E(".t compare 2.end == end-1c","1");
    E(".t compare 1.0 > 2.0","0");
    E(".t compare 2.0 != 1.0","1");
    E(".t compare 2.0 >= 2.0","1");
    E(".t compare 2.0 <= 1.0","0");
    CHECK(tg_eval(i,".t compare 1.0 nonsense 2.0")==TG_ERROR);
    E(".t get 1.0 end-1c","alpha\nbeta");
    E(".t search -nocase BETA 1.0 end","2.0");
    E(".t search BETA 1.0 end","");
    E(".t search -backwards alpha end 1.0","1.0");
    E(".t tag configure match -background yellow -foreground black","");
    E(".t tag add match 2.0 2.4; .t tag ranges match","2.0 2.4");
    CHECK(tg_eval(i,".t tag add match 1.0 1.2 invalid 2.0")==TG_ERROR);
    E(".t tag ranges match","2.0 2.4");
    CHECK(tg_eval(i,".t tag remove match 2.0 2.2 invalid 2.0")==TG_ERROR);
    E(".t tag ranges match","2.0 2.4");
    E(".t tag remove match 2.1 2.3; .t tag ranges match","2.0 2.1 2.3 2.4");
    E(".t tag remove match 1.0 end; .t tag ranges match","");
    E(".t tag configure first -foreground red","");
    E(".t tag configure second -background blue","");
    E(".t tag add first 1.0 1.2","");
    E(".t tag add second 1.1 2.2","");
    E(".t tag names 1.1","first second");
    E(".t tag cget second -background","#0000ff");
    CHECK(tg_eval(i,".t tag configure second -background red -foreground invalid")==TG_ERROR);
    E(".t tag cget second -background","#0000ff");
    E(".t tag delete first","");
    E(".t tag names 1.1","second");
    E(".t tag ranges second","1.1 2.2");
    E(".t tag delete second","");
    E(".t tag names 1.1","");
    E(".t replace 2.0 2.4 gamma; .t get 2.0 end-1c","gamma");
    E(".t mark set insert 1.0; .t index insert","1.0");
    E(".t configure -state disabled; .t delete 1.0 end; .t get 1.0 1.5","alpha");
    E(".t configure -state normal; place .t -width 100 -height 60; message .m -text {wrapped message}; place .m -y 60 -width 50 -height 40","");
    CHECK(tg_ui_arrange(u,120,100)==TG_OK);
    tg_id id=tg_ui_find(u,".t"); char script[32];
    CHECK(tg_ui_apply_event(u,(tg_event){TG_POINTER_DOWN,id,4,3,0},script,sizeof script));
    CHECK(tg_ui_apply_event(u,(tg_event){TG_TEXT_INPUT,id,0,0,'X'},script,sizeof script));
    E(".t get 1.0 1.1","X");
    tg_ui *copy=tg_snapshot_create(u); CHECK(copy);
    E(".t delete 1.0 end; .t get 1.0 end","\n");
    char *get[]={".t","get","1.0","1.1"};
    CHECK(tg_ui_command(copy,4,get,out,sizeof out)==TG_OK&&!strcmp(out,"X"));
    tg_canvas *c=tg_canvas_create(120,100); CHECK(c&&tg_ui_paint(copy,c)==TG_OK);
    tg_canvas_destroy(c); tg_ui_destroy(copy); tg_destroy(i); return 0;
}
