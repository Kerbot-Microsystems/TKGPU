#include "TKGPUCompat.H"
#include <stdint.h>
#include <stdlib.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#endif

typedef struct exit_entry {
    Tcl_ExitProc *proc;
    void *data;
    struct exit_entry *next;
} exit_entry;

typedef struct {
    exit_entry *head;
} thread_exit_state;

static Tcl_Mutex exit_mutex;
static exit_entry *process_exits;
static Tcl_ExitProc *app_exit_proc;
static Tcl_ThreadDataKey thread_exit_key;

void Tcl_GetVersion(int *major,int *minor,int *patchLevel,int *type) {
    if(major) *major=8;
    if(minor) *minor=6;
    if(patchLevel) *patchLevel=14;
    if(type) *type=TKGPU_FINAL_RELEASE;
}

void Tcl_CreateExitHandler(Tcl_ExitProc *proc,void *data) {
    if(!proc) return;
    exit_entry *e=malloc(sizeof *e);
    if(!e) Tcl_Panic("cannot allocate exit handler");
    e->proc=proc; e->data=data;
    Tcl_MutexLock(&exit_mutex);
    e->next=process_exits;
    process_exits=e;
    Tcl_MutexUnlock(&exit_mutex);
}

void Tcl_DeleteExitHandler(Tcl_ExitProc *proc,void *data) {
    if(!proc) return;
    Tcl_MutexLock(&exit_mutex);
    exit_entry **link=&process_exits;
    while(*link) {
        exit_entry *e=*link;
        if(e->proc==proc&&e->data==data) {
            *link=e->next;
            free(e);
            break;
        }
        link=&e->next;
    }
    Tcl_MutexUnlock(&exit_mutex);
}

Tcl_ExitProc *Tcl_SetExitProc(Tcl_ExitProc *proc) {
    Tcl_MutexLock(&exit_mutex);
    Tcl_ExitProc *previous=app_exit_proc;
    app_exit_proc=proc;
    Tcl_MutexUnlock(&exit_mutex);
    return previous;
}

void Tcl_CreateThreadExitHandler(Tcl_ExitProc *proc,void *data) {
    if(!proc) return;
    thread_exit_state *state=Tcl_GetThreadData(&thread_exit_key,(int)sizeof *state);
    if(!state) Tcl_Panic("cannot allocate thread exit handler state");
    exit_entry *e=malloc(sizeof *e);
    if(!e) Tcl_Panic("cannot allocate thread exit handler");
    e->proc=proc; e->data=data; e->next=state->head;
    state->head=e;
}

void Tcl_DeleteThreadExitHandler(Tcl_ExitProc *proc,void *data) {
    if(!proc) return;
    thread_exit_state *state=Tcl_GetThreadData(&thread_exit_key,(int)sizeof *state);
    if(!state) return;
    exit_entry **link=&state->head;
    while(*link) {
        exit_entry *e=*link;
        if(e->proc==proc&&e->data==data) {
            *link=e->next;
            free(e);
            break;
        }
        link=&e->next;
    }
}

void Tcl_FinalizeThread(void) {
    thread_exit_state *state=Tcl_GetThreadData(&thread_exit_key,(int)sizeof *state);
    if(!state) return;
    while(state->head) {
        exit_entry *e=state->head;
        state->head=e->next;
        Tcl_ExitProc *proc=e->proc;
        void *data=e->data;
        free(e);
        if(proc) proc(data);
    }
}

void Tcl_Finalize(void) {
    Tcl_FinalizeThread();
    for(;;) {
        Tcl_MutexLock(&exit_mutex);
        exit_entry *e=process_exits;
        if(e) process_exits=e->next;
        Tcl_MutexUnlock(&exit_mutex);
        if(!e) break;
        Tcl_ExitProc *proc=e->proc;
        void *data=e->data;
        free(e);
        if(proc) proc(data);
    }
}

void Tcl_Exit(int status) {
    Tcl_MutexLock(&exit_mutex);
    Tcl_ExitProc *proc=app_exit_proc;
    Tcl_MutexUnlock(&exit_mutex);
    if(proc) {
        proc((void *)(intptr_t)status);
        exit(status);
    }
    Tcl_Finalize();
    exit(status);
}

void Tcl_ExitThread(int status) {
    Tcl_FinalizeThread();
#ifdef _WIN32
    ExitThread((DWORD)status);
#else
    pthread_exit((void *)(intptr_t)status);
#endif
}

static Tcl_MainLoopProc *main_loop_proc;
void Tcl_SetMainLoop(Tcl_MainLoopProc *proc) { main_loop_proc=proc; }
void Tcl_ServiceModeHook(int mode) { (void)mode; }
