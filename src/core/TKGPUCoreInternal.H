#ifndef TKGPU_CORE_INTERNAL_H
#define TKGPU_CORE_INTERNAL_H
#include "TKGPUUi.H"
#include "TKGPUCommand.H"
#include "TKGPUObject.H"
#include "history/TKGPUHistory.H"
enum { MAX_WORD=4096, TG_RESULT_CAP=1048640, MAX_ARGS=1024, MAX_VARS=16384, MAX_DEPTH=16, MAX_COMMANDS=16384 };
typedef struct { char *key,*value; tg_object *object; int kind; } var;
typedef struct {
    char *name;
    tg_command_fn fn;
    void *context;
    tg_command_dispose_fn dispose;
    int (*object_fn)(void *,tg_interp *,size_t,tg_object *const []);
} tg_registered_command;
struct tg_after_task;
typedef struct tg_var_wait {
    struct tg_var_wait *next;
    const char *key;
    int changed;
} tg_var_wait;
struct tg_binding;
struct tg_canvas_binding;
struct tg_var_trace;
typedef void tg_interp_delete_proc(void *,tg_interp *);
struct tg_assoc;
struct tg_delete_hook;
typedef struct tg_ime_edit { tg_id target; size_t first,last,length; char bytes[]; } tg_ime_edit;
struct tg_interp {
    /* Tcl 8.6 bootstrap prefix. NULL deliberately refuses unsupported Stubs.
       The independent Host ABI still treats the interpreter as opaque. */
    char *legacy_result;
    void (*legacy_free)(char *);
    int legacy_error_line;
    const void *legacy_stub_table;
    char result[TG_RESULT_CAP];
    int result_kind;
    void *host_owner; /* Independent host API only; legacy facade leaves this unset. */
    var *vars;
    size_t count,var_capacity;
    tg_ui *ui;
    char *scratch;
    size_t scratch_words;
    unsigned depth,compat_callback_depth,event_pump_depth;
    unsigned recursion_limit;
    int (*eval_check)(tg_interp *);
    tg_registered_command *commands;
    size_t command_count,command_capacity;
    uint32_t *command_index;
    size_t command_index_capacity,command_index_used;
    int disposing,tcl_initialized,deleted,destroying;
    unsigned compat_lifetime_depth;
    tg_history *history;
    struct tg_after_task *after_tasks;
    tg_var_wait *var_waits;
    struct tg_var_trace *traces; unsigned trace_next;
    struct tg_binding *bindings;
    struct tg_canvas_binding *canvas_bindings;
    struct tg_bindtag_entry *bindtags;
    tg_id binding_capture,last_click_target;
    unsigned binding_capture_button;
    int last_click_x,last_click_y;
    uint64_t last_click_ms;
    int (*tk_origin)(void *,tg_id,int *,int *);
    int (*tk_clock)(uint64_t *);
    int (*event_command)(tg_interp *,size_t,char **);
    void (*event_dispose)(tg_interp *);
    void *tk_host;
    int (*tk_command)(tg_interp *,size_t,char **);
    int (*tk_hotkey)(tg_interp *,tg_event);
    void (*tk_dispose)(void *),(*tk_invalidate)(void *),(*tk_close)(void *);
    void (*tk_repaint)(void *,tg_id);
    tg_object *compat_result,*compat_variable,*compat_channel_error;
    int compat_legacy_lookup;
    struct Tcl_Namespace *compat_global_namespace;
    tg_object *compat_namespace_exports;
    tg_ime_edit *ime_pending;
    struct tg_assoc *assoc;
    struct tg_delete_hook *delete_hooks;
    unsigned assoc_callback_depth;
};
enum { TG_VAR_ARRAY=100 };
enum { TG_RESULT_STRING=0,TG_RESULT_INTEGER=1,TG_RESULT_BOOLEAN=2,TG_RESULT_LIST=3,
       TG_RESULT_DOUBLE=4,TG_RESULT_INT_LIST=5,TG_RESULT_DOUBLE_LIST=6 };
#ifdef TKGPU_REFERENCE_STUBS
void tg_compat_lifetime_enter(tg_interp *);
void tg_compat_lifetime_leave(tg_interp *);
#endif
int tg_assoc_set(tg_interp *,const char *,tg_interp_delete_proc *,void *);
void *tg_assoc_get(tg_interp *,const char *,tg_interp_delete_proc **);
void tg_assoc_delete(tg_interp *,const char *);
int tg_delete_hook_add(tg_interp *,tg_interp_delete_proc *,void *);
void tg_delete_hook_remove(tg_interp *,tg_interp_delete_proc *,void *);
void tg_legacy_result_release(tg_interp *);
int tg_set_result(tg_interp *i,const char *s,int code);
int tg_set_result_kind(tg_interp *i,const char *s,int kind);
char *tg_copy_string(const char *s);
size_t tg_var_find(tg_interp *i,const char *key);
int tg_var_store(tg_interp *i,const char *key,const char *value);
int tg_var_store_object(tg_interp *,const char *,const char *,tg_object *);
int tg_var_assign_object(tg_interp *,const char *,const char *,tg_object *);
int tg_var_store_kind(tg_interp *,const char *,const char *,int);
int tg_trace_command(tg_interp *,size_t,char **);
int tg_trace_notify(tg_interp *,const char *,int);
void tg_trace_clear(tg_interp *);
int tg_var_unset(tg_interp *i,const char *key);
int tg_control_invoke(tg_interp *,size_t,char **);
int tg_binding_command(tg_interp *,size_t,char **);
int tg_binding_dispatch(tg_interp *,tg_event);
void tg_binding_clear(tg_interp *);
void tg_binding_prune(tg_interp *);
int tg_control_refresh(tg_interp *,size_t,char **);
int tg_entry_validation_command(tg_interp *,size_t,char **);
int tg_entry_validation_input(tg_interp *,tg_event);
int tg_entry_validation_replace(tg_interp *,tg_id,const char *,const char *,int,int);
int tg_ime_queue(tg_interp *,tg_id,size_t,size_t,const char *,size_t,const char *);
void tg_entry_validation_focus(tg_interp *,tg_id,int);
void tg_entry_validation_variable(tg_interp *,const char *);
void tg_entry_validation_bound(tg_interp *,tg_id,const char *);
int tg_interp_dispatch_scroll(tg_interp *);
int tg_textvar_prepare(tg_interp *,size_t,char **);
int tg_control_command(tg_interp *i,size_t n,char **a);
int tg_eval_condition(tg_interp *i,const char *source,int64_t *value);
int tg_eval_numeric_result(tg_interp *,const char *);
int tg_list_command(tg_interp *i,size_t n,char **a);
int tg_command_try_objects(tg_interp *i,size_t n,tg_object *const v[]);
int tg_command_dispatch(tg_interp *i,size_t n,char **a);
void tg_command_clear(tg_interp *i);
size_t tg_command_lookup(const tg_interp *,const char *);
void tg_command_index_added(tg_interp *);
void tg_command_index_remove(tg_interp *,size_t);
void tg_command_index_clear(tg_interp *);
int tg_execute(tg_interp *i,size_t n,char **a);
void tg_host_ui_result_kind(tg_interp *,size_t,char **);
int tg_eval_script(tg_interp *i,const char **cursor,int nested);
#endif
