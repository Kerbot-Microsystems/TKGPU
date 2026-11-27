/* Original TKGPU result contracts for successful native UI queries.
   No lexical guessing: a numeric-looking label or Treeview ID stays text.
   Called only after built-in dispatch, never after a registered callback. */
#include "../TKGPUCoreInternal.H"
#include <string.h>
static int eq(const char *a,const char *b) { return !strcmp(a,b); }
void tg_host_ui_result_kind(tg_interp *i,size_t n,char **a) {
    if(n<2||i->result_kind!=TG_RESULT_STRING) return;
    const char *op=a[1];
    if(eq(a[0],"winfo")&&n==3) {
        if(eq(op,"exists")||eq(op,"width")||eq(op,"height")||eq(op,"x")||eq(op,"y")||
           eq(op,"rootx")||eq(op,"rooty")||eq(op,"reqwidth")||eq(op,"reqheight")||
           eq(op,"ismapped")||eq(op,"viewable")||eq(op,"screenwidth")||eq(op,"screenheight"))
            i->result_kind=TG_RESULT_INTEGER;
        else if(eq(op,"children")) i->result_kind=TG_RESULT_LIST;
        return;
    }
    if(eq(a[0],"wm")&&n==3) {
        if(eq(op,"minsize")||eq(op,"maxsize")||eq(op,"resizable")) i->result_kind=TG_RESULT_INT_LIST;
        else if(eq(op,"overrideredirect")) i->result_kind=TG_RESULT_INTEGER;
        return;
    }
    if((eq(a[0],"pack")||eq(a[0],"grid")||eq(a[0],"place"))&&n==3) {
        if(eq(op,"slaves")||eq(op,"info")) i->result_kind=TG_RESULT_LIST;
        else if(eq(op,"propagate")) i->result_kind=TG_RESULT_INTEGER;
        return;
    }
    if(eq(a[0],"image")&&n==3&&(eq(op,"width")||eq(op,"height"))) {
        i->result_kind=TG_RESULT_INTEGER; return;
    }
    if(eq(a[0],"font")) {
        size_t pos_args=0;
        const char *opt=NULL;
        for(size_t k=2;k<n;k++) {
            if(eq(a[k],"-displayof")&&k+1<n) { k++; continue; }
            pos_args++;
            opt=a[k];
        }
        if(eq(op,"names")||eq(op,"families")) i->result_kind=TG_RESULT_LIST;
        else if(eq(op,"measure")) i->result_kind=TG_RESULT_INTEGER;
        else if(eq(op,"metrics")) i->result_kind=pos_args>=2?TG_RESULT_INTEGER:TG_RESULT_LIST;
        else if(eq(op,"configure")||eq(op,"config")||eq(op,"actual")) {
            if(pos_args==1) i->result_kind=TG_RESULT_LIST;
            else if(pos_args==2&&opt&&(eq(opt,"-size")||eq(opt,"-underline")||eq(opt,"-overstrike")))
                i->result_kind=TG_RESULT_INTEGER;
        }
        return;
    }
    if(eq(a[0],"ttk::style")) {
        if((n==3&&(eq(op,"configure")||eq(op,"map")))||
           (n==3&&eq(op,"theme")&&eq(a[2],"names"))||
           (n==3&&eq(op,"layout"))||(n>=3&&n<=4&&eq(op,"element")&&(eq(a[2],"names")||eq(a[2],"options")))) i->result_kind=TG_RESULT_LIST;
        return;
    }
    if(a[0][0]!='.') return;
    const tg_widget *w=tg_ui_get(i->ui,tg_ui_find(i->ui,a[0]));
    if(!w) return;
    if(w->themed) {
        if(eq(op,"state")&&(n==2||n==3)) { i->result_kind=TG_RESULT_LIST; return; }
        if(eq(op,"instate")&&n==3) { i->result_kind=TG_RESULT_BOOLEAN; return; }
    }
    if(w->kind==TG_LISTBOX) {
        if((n==2&&eq(op,"size"))||(n==3&&(eq(op,"index")||eq(op,"nearest")))||
           (n==4&&eq(op,"selection")&&eq(a[2],"includes"))) i->result_kind=TG_RESULT_INTEGER;
        else if(n==2&&eq(op,"curselection")) i->result_kind=TG_RESULT_INT_LIST;
        else if(n==4&&eq(op,"get")) i->result_kind=TG_RESULT_LIST;
    } else if(w->kind==TG_CANVAS) {
        if(n>=3&&eq(op,"create")) i->result_kind=TG_RESULT_INTEGER;
        else if(n>=3&&(eq(op,"find")||eq(op,"bbox"))) i->result_kind=TG_RESULT_INT_LIST;
        else if(n==3&&eq(op,"gettags")) i->result_kind=TG_RESULT_LIST;
        else if(n==3&&eq(op,"coords")) i->result_kind=TG_RESULT_DOUBLE_LIST;
        else if((n==3||n==4)&&(eq(op,"canvasx")||eq(op,"canvasy"))) i->result_kind=TG_RESULT_DOUBLE;
    } else if(w->kind==TG_SCROLLBAR) {
        if(n==2&&eq(op,"get")) i->result_kind=TG_RESULT_DOUBLE_LIST;
        else if(n==4&&(eq(op,"fraction")||eq(op,"delta"))) i->result_kind=TG_RESULT_DOUBLE;
    } else if(w->kind==TG_MENU) {
        if(n==3&&((eq(op,"index")&&strcmp(i->result,"none"))||eq(op,"yposition"))) i->result_kind=TG_RESULT_INTEGER;
    } else if(w->kind==TG_TEXT) {
        if(n==5&&eq(op,"compare")) i->result_kind=TG_RESULT_INTEGER;
        else if(n>=4&&eq(op,"count")) i->result_kind=n<=5?TG_RESULT_INTEGER:TG_RESULT_INT_LIST;
        else if(n==3&&eq(op,"edit")&&(eq(a[2],"modified")||eq(a[2],"canundo")||eq(a[2],"canredo"))) i->result_kind=TG_RESULT_INTEGER;
        else if(n==3&&eq(op,"mark")&&eq(a[2],"names")) i->result_kind=TG_RESULT_LIST;
        else if(eq(op,"tag")&&((n==3&&eq(a[2],"names"))||
                (n==4&&(eq(a[2],"names")||eq(a[2],"ranges")))||
                ((n==5||n==6)&&(eq(a[2],"nextrange")||eq(a[2],"prevrange"))))) i->result_kind=TG_RESULT_LIST;
    } else if(w->kind==TG_PANEDWINDOW) {
        if(n==2&&eq(op,"panes")) i->result_kind=TG_RESULT_LIST;
        else if(n==3&&eq(op,"pane")) i->result_kind=TG_RESULT_LIST;
        else if((n==4&&eq(op,"pane")&&eq(a[3],"-weight"))||
                ((n==3||n==4)&&eq(op,"sashpos"))||
                (n==4&&eq(op,"identify")&&i->result[0])) i->result_kind=TG_RESULT_INTEGER;
    } else if(w->kind==TG_NOTEBOOK) {
        if(n==3&&eq(op,"index")) i->result_kind=TG_RESULT_INTEGER;
        else if(n==2&&eq(op,"tabs")) i->result_kind=TG_RESULT_LIST;
        else if(n==3&&eq(op,"tab")) i->result_kind=TG_RESULT_LIST;
        else if(n==3&&eq(op,"bbox")) i->result_kind=TG_RESULT_INT_LIST;
        else if(n==5&&eq(op,"identify")&&eq(a[2],"tab")&&i->result[0])
            i->result_kind=TG_RESULT_INTEGER;
    } else if(w->kind==TG_ENTRY||w->kind==TG_COMBOBOX||w->kind==TG_SPINBOX) {
        if(n==3&&eq(op,"index")) i->result_kind=TG_RESULT_INTEGER;
        else if(n==2&&eq(op,"xview")) i->result_kind=TG_RESULT_DOUBLE_LIST;
        else if(n==3&&eq(op,"selection")&&eq(a[2],"present")) i->result_kind=TG_RESULT_INTEGER;
        else if(w->kind==TG_COMBOBOX&&n==2&&eq(op,"current")) i->result_kind=TG_RESULT_INTEGER;
    } else if(w->kind==TG_TREEVIEW) {
        if(n==3&&(eq(op,"exists")||eq(op,"index"))) i->result_kind=TG_RESULT_INTEGER;
        else if((n==3||n==4)&&eq(op,"bbox")) i->result_kind=TG_RESULT_INT_LIST;
        else if(n==3&&(eq(op,"item")||eq(op,"set"))) i->result_kind=TG_RESULT_LIST;
        else if((n==2||n==3)&&eq(op,"children")) i->result_kind=TG_RESULT_LIST;
        else if(n==2&&eq(op,"selection")) i->result_kind=TG_RESULT_LIST;
        else if(n==3&&eq(op,"tag")&&eq(a[2],"names")) i->result_kind=TG_RESULT_LIST;
        else if(n==4&&eq(op,"item")&&(eq(a[3],"-values")||eq(a[3],"-tags"))) i->result_kind=TG_RESULT_LIST;
        else if(n==4&&eq(op,"item")&&eq(a[3],"-open")) i->result_kind=TG_RESULT_INTEGER;
    }
    if(n==2&&(eq(op,"xview")||eq(op,"yview"))&&
       (w->kind==TG_CANVAS||w->kind==TG_LISTBOX||w->kind==TG_TEXT||w->kind==TG_TREEVIEW))
        i->result_kind=TG_RESULT_DOUBLE_LIST;
}
