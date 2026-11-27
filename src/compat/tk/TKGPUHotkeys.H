#ifndef TKGPU_HOTKEYS_H
#define TKGPU_HOTKEYS_H
#include "TKGPUCompat.H"
#include "TKGPUUi.H"

/* Global C hotkey, clipboard, and text-action module for all TKGPU text widgets
   (Text, Entry, TEntry, Combobox, Spinbox). Works across all keyboard layouts. */
int tg_hotkey_dispatch(tg_interp *i, tg_event e);
int tg_hotkey_command(Tcl_Interp *i, size_t n, char **a);

#endif
