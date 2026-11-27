#include "TKGPUHotkeys.H"
#include "TKGPUWindow.H"
#include "../../core/TKGPUCoreInternal.H"
#include "../../widgets/TKGPUWidgetInternal.H"
#include "../../widgets/controls/TKGPUControls.H"
#include "../../widgets/text/TKGPUTextWidget.H"
#include "TKGPUUtf8.H"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static int hotkey_is_entry_like(tg_kind kind) {
    return kind == TG_ENTRY || kind == TG_COMBOBOX || kind == TG_SPINBOX;
}

static int hotkey_is_text_widget(const tg_widget *w) {
    return w && (w->kind == TG_TEXT || hotkey_is_entry_like(w->kind));
}

static size_t hotkey_utf8_byte_at(const char *text, size_t index) {
    size_t at = 0, n = strlen(text);
    while (at < n && index--) {
        uint32_t scalar;
        size_t used;
        if (tg_utf8_decode((const unsigned char *)text + at, n - at, &scalar, &used) != TG_OK) return n;
        at += used;
    }
    return at;
}

static void *hotkey_native_hwnd(Tcl_Interp *i, const char *path) {
    tg_window *w = path && path[0] ? tg_window_for_widget(i, path) : NULL;
    if (!w) w = Tk_MainWindow(i);
    return w ? w->native : NULL;
}

#ifdef _WIN32
static int open_native_clipboard(HWND hwnd) {
    if (hwnd && OpenClipboard(hwnd)) return 1;
    return OpenClipboard(NULL) ? 1 : 0;
}

static int hotkey_clipboard_put(void *native_hwnd, const char *text) {
    HWND hwnd = (HWND)native_hwnd;
    if (!text || !text[0]) {
        if (!open_native_clipboard(hwnd)) return 0;
        int ok = EmptyClipboard() != 0;
        CloseClipboard();
        return ok;
    }
#ifdef TKGPU_LEGACY_WINDOWS
    int wcount = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    if (wcount < 1) return 0;
    WCHAR *wide = malloc((size_t)wcount * sizeof(WCHAR));
    if (!wide) return 0;
    MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, wcount);
    int bytes = WideCharToMultiByte(CP_ACP, 0, wide, -1, NULL, 0, NULL, NULL);
    if (bytes < 1) { free(wide); return 0; }
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, (size_t)bytes);
    if (!memory) { free(wide); return 0; }
    char *ansi = GlobalLock(memory);
    if (!ansi) { GlobalFree(memory); free(wide); return 0; }
    int converted = WideCharToMultiByte(CP_ACP, 0, wide, -1, ansi, bytes, NULL, NULL);
    GlobalUnlock(memory);
    free(wide);
    if (!converted || !open_native_clipboard(hwnd)) { GlobalFree(memory); return 0; }
    int ok = EmptyClipboard() && SetClipboardData(CF_TEXT, memory) != NULL;
    CloseClipboard();
    if (!ok) GlobalFree(memory);
    return ok;
#else
    int count = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    if (count < 1) return 0;
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, (size_t)count * sizeof(WCHAR));
    if (!memory) return 0;
    WCHAR *wide = GlobalLock(memory);
    if (!wide) { GlobalFree(memory); return 0; }
    MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, count);
    GlobalUnlock(memory);
    if (!open_native_clipboard(hwnd)) { GlobalFree(memory); return 0; }
    int ok = EmptyClipboard() && SetClipboardData(CF_UNICODETEXT, memory) != NULL;
    CloseClipboard();
    if (!ok) GlobalFree(memory);
    return ok;
#endif
}

static char *hotkey_clipboard_get_alloc(void *native_hwnd, size_t *out_len) {
    if (out_len) *out_len = 0;
    HWND hwnd = (HWND)native_hwnd;
    if (!open_native_clipboard(hwnd)) return NULL;
#ifndef TKGPU_LEGACY_WINDOWS
    HGLOBAL memory = GetClipboardData(CF_UNICODETEXT);
    if (memory) {
        const WCHAR *wide = GlobalLock(memory);
        if (!wide) { CloseClipboard(); return NULL; }
        size_t max_units = GlobalSize(memory) / sizeof(WCHAR);
        size_t length = 0;
        while (length < max_units && wide[length]) length++;
        if (length == 0) {
            GlobalUnlock(memory);
            CloseClipboard();
            char *empty = malloc(1);
            if (empty) empty[0] = 0;
            return empty;
        }
        int needed = WideCharToMultiByte(CP_UTF8, 0, wide, (int)length, NULL, 0, NULL, NULL);
        if (needed <= 0) { GlobalUnlock(memory); CloseClipboard(); return NULL; }
        if ((size_t)needed >= TG_RESULT_CAP) needed = TG_RESULT_CAP - 1;
        char *buf = malloc((size_t)needed + 1);
        if (!buf) { GlobalUnlock(memory); CloseClipboard(); return NULL; }
        int written = WideCharToMultiByte(CP_UTF8, 0, wide, (int)length, buf, needed, NULL, NULL);
        GlobalUnlock(memory);
        CloseClipboard();
        if (written < 0) { free(buf); return NULL; }
        buf[written] = 0;
        if (out_len) *out_len = (size_t)written;
        return buf;
    }
#endif
    HGLOBAL ansi_mem = GetClipboardData(CF_TEXT);
    if (!ansi_mem) { CloseClipboard(); return NULL; }
    const char *ansi = GlobalLock(ansi_mem);
    if (!ansi) { CloseClipboard(); return NULL; }
    size_t max_bytes = GlobalSize(ansi_mem), alen = 0;
    while (alen < max_bytes && ansi[alen]) alen++;
    if (alen == 0) {
        GlobalUnlock(ansi_mem);
        CloseClipboard();
        char *empty = malloc(1);
        if (empty) empty[0] = 0;
        return empty;
    }
    int wunits = MultiByteToWideChar(CP_ACP, 0, ansi, (int)alen, NULL, 0);
    if (wunits <= 0) { GlobalUnlock(ansi_mem); CloseClipboard(); return NULL; }
    WCHAR *wide = malloc((size_t)wunits * sizeof(WCHAR));
    if (!wide) { GlobalUnlock(ansi_mem); CloseClipboard(); return NULL; }
    MultiByteToWideChar(CP_ACP, 0, ansi, (int)alen, wide, wunits);
    GlobalUnlock(ansi_mem);
    CloseClipboard();
    int needed = WideCharToMultiByte(CP_UTF8, 0, wide, wunits, NULL, 0, NULL, NULL);
    if (needed <= 0) { free(wide); return NULL; }
    if ((size_t)needed >= TG_RESULT_CAP) needed = TG_RESULT_CAP - 1;
    char *buf = malloc((size_t)needed + 1);
    if (!buf) { free(wide); return NULL; }
    int written = WideCharToMultiByte(CP_UTF8, 0, wide, wunits, buf, needed, NULL, NULL);
    free(wide);
    if (written < 0) { free(buf); return NULL; }
    buf[written] = 0;
    if (out_len) *out_len = (size_t)written;
    return buf;
}
#elif defined(TKGPU_COCOA)
static int hotkey_clipboard_put(void *unused, const char *text) {
    (void)unused;
    return tg_cocoa_clipboard_put(text ? text : "");
}
static char *hotkey_clipboard_get_alloc(void *unused, size_t *out_len) {
    (void)unused;
    if (out_len) *out_len = 0;
    size_t cap = 65536;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    if (!tg_cocoa_clipboard_get(buf, cap)) { free(buf); return NULL; }
    if (out_len) *out_len = strlen(buf);
    return buf;
}
#else
static int hotkey_clipboard_put(void *unused, const char *text) {
    (void)unused; (void)text; return 0;
}
static char *hotkey_clipboard_get_alloc(void *unused, size_t *out_len) {
    (void)unused; if (out_len) *out_len = 0; return NULL;
}
#endif

static void normalize_newlines_multiline(char *s) {
    char *r = s, *w = s;
    while (*r) {
        if (*r == '\r') {
            if (r[1] == '\n') r++;
            *w++ = '\n';
            r++;
        } else {
            *w++ = *r++;
        }
    }
    *w = 0;
}

static void normalize_newlines_singleline(char *s) {
    char *r = s, *w = s;
    while (*r) {
        if (*r == '\r') {
            r++;
        } else if (*r == '\n') {
            *w++ = ' ';
            r++;
        } else {
            *w++ = *r++;
        }
    }
    *w = 0;
}

static void truncate_utf8_inplace(char *s, size_t max_bytes) {
    size_t n = strlen(s);
    if (n <= max_bytes) return;
    size_t pos = 0;
    while (pos < max_bytes) {
        uint32_t scalar;
        size_t used;
        if (tg_utf8_decode((const unsigned char *)s + pos, n - pos, &scalar, &used) != TG_OK || !used) break;
        if (pos + used > max_bytes) break;
        pos += used;
    }
    s[pos] = 0;
}

static int hotkey_sync_entry_var(tg_interp *i, tg_widget *w) {
    if (!w || !hotkey_is_entry_like(w->kind) || !w->textvariable[0]) return TG_OK;
    return tg_var_store(i, w->textvariable, w->text);
}

static void hotkey_notify_changed(tg_interp *i, tg_widget *w) {
    if (w && w->kind == TG_TEXT) (void)tg_interp_dispatch_scroll(i);
    if (i->tk_invalidate && i->tk_host) i->tk_invalidate(i->tk_host);
}

static int hotkey_text_has_selection(tg_widget *w) {
    char range[128];
    char *args[] = { w->path, "tag", "ranges", "sel" };
    return tg_document_command(w, 4, args, range, sizeof range) == TG_OK && range[0] != 0;
}

static int hotkey_action_copy(tg_interp *i, tg_widget *w) {
    if (!hotkey_is_text_widget(w)) return TG_ERROR;
    void *hwnd = hotkey_native_hwnd(i, w->path);
    if (w->kind == TG_TEXT) {
        if (!hotkey_text_has_selection(w)) return TG_OK;
        char *buf = malloc(TG_RESULT_CAP);
        if (!buf) return TG_ERROR;
        char *args[] = { w->path, "get", "sel.first", "sel.last" };
        int rc = tg_document_command(w, 4, args, buf, TG_RESULT_CAP);
        if (rc == TG_OK && buf[0]) (void)hotkey_clipboard_put(hwnd, buf);
        free(buf);
        return rc == TG_OK ? TG_OK : TG_ERROR;
    }
    if (w->show[0]) return TG_OK;
    size_t first = hotkey_utf8_byte_at(w->text, w->sel_first);
    size_t last = hotkey_utf8_byte_at(w->text, w->sel_last);
    if (last <= first) return TG_OK;
    char buf[sizeof w->text];
    size_t len = last - first;
    if (len >= sizeof buf) len = sizeof buf - 1;
    memcpy(buf, w->text + first, len);
    buf[len] = 0;
    return hotkey_clipboard_put(hwnd, buf) ? TG_OK : TG_ERROR;
}

static int hotkey_action_cut(tg_interp *i, tg_widget *w) {
    if (!hotkey_is_text_widget(w)) return TG_ERROR;
    if (w->state != TG_STATE_NORMAL) return TG_OK;
    (void)hotkey_action_copy(i, w);
    char out[256];
    if (w->kind == TG_TEXT) {
        if (!hotkey_text_has_selection(w)) return TG_OK;
        char *del_args[] = { w->path, "delete", "sel.first", "sel.last" };
        if (tg_document_command(w, 4, del_args, out, sizeof out) != TG_OK) return TG_ERROR;
        char *see_args[] = { w->path, "see", "insert" };
        (void)tg_document_command(w, 3, see_args, out, sizeof out);
        hotkey_notify_changed(i, w);
        return TG_OK;
    }
    size_t first = hotkey_utf8_byte_at(w->text, w->sel_first);
    size_t last = hotkey_utf8_byte_at(w->text, w->sel_last);
    if (last <= first) return TG_OK;
    char *del_args[] = { w->path, "delete", "sel.first", "sel.last" };
    if (tg_entry_command(w, 4, del_args, out, sizeof out) != TG_OK) return TG_ERROR;
    if (hotkey_sync_entry_var(i, w) != TG_OK) return TG_ERROR;
    hotkey_notify_changed(i, w);
    return TG_OK;
}

static int hotkey_action_paste(tg_interp *i, tg_widget *w) {
    if (!hotkey_is_text_widget(w)) return TG_ERROR;
    if (w->state != TG_STATE_NORMAL) return TG_OK;
    void *hwnd = hotkey_native_hwnd(i, w->path);
    size_t clip_len = 0;
    char *clip = hotkey_clipboard_get_alloc(hwnd, &clip_len);
    if (!clip) return TG_OK;
    if (!clip[0]) { free(clip); return TG_OK; }
    char out[256];
    if (w->kind == TG_TEXT) {
        normalize_newlines_multiline(clip);
        if (hotkey_text_has_selection(w)) {
            char *del_args[] = { w->path, "delete", "sel.first", "sel.last" };
            (void)tg_document_command(w, 4, del_args, out, sizeof out);
        }
        char *ins_args[] = { w->path, "insert", "insert", clip };
        int rc = tg_document_command(w, 4, ins_args, out, sizeof out);
        free(clip);
        if (rc != TG_OK) return TG_ERROR;
        char *see_args[] = { w->path, "see", "insert" };
        (void)tg_document_command(w, 3, see_args, out, sizeof out);
        hotkey_notify_changed(i, w);
        return TG_OK;
    }
    normalize_newlines_singleline(clip);
    size_t first = hotkey_utf8_byte_at(w->text, w->sel_first);
    size_t last = hotkey_utf8_byte_at(w->text, w->sel_last);
    size_t current_len = strlen(w->text);
    size_t remaining = current_len - (last > first ? (last - first) : 0);
    size_t avail = remaining < sizeof w->text - 1 ? (sizeof w->text - 1 - remaining) : 0;
    if (!avail) { free(clip); return TG_OK; }
    truncate_utf8_inplace(clip, avail);
    if (!clip[0]) { free(clip); return TG_OK; }
    if (last > first) {
        char *del_args[] = { w->path, "delete", "sel.first", "sel.last" };
        if (tg_entry_command(w, 4, del_args, out, sizeof out) != TG_OK) { free(clip); return TG_ERROR; }
    }
    char *ins_args[] = { w->path, "insert", "insert", clip };
    int rc = tg_entry_command(w, 4, ins_args, out, sizeof out);
    free(clip);
    if (rc != TG_OK) return TG_ERROR;
    if (hotkey_sync_entry_var(i, w) != TG_OK) return TG_ERROR;
    hotkey_notify_changed(i, w);
    return TG_OK;
}

static int hotkey_action_select_all(tg_interp *i, tg_widget *w) {
    if (!hotkey_is_text_widget(w)) return TG_ERROR;
    char out[256];
    if (w->kind == TG_TEXT) {
        char *rm_args[] = { w->path, "tag", "remove", "sel", "1.0", "end" };
        (void)tg_document_command(w, 6, rm_args, out, sizeof out);
        char *add_args[] = { w->path, "tag", "add", "sel", "1.0", "end-1c" };
        (void)tg_document_command(w, 6, add_args, out, sizeof out);
        char *mark_args[] = { w->path, "mark", "set", "insert", "end-1c" };
        (void)tg_document_command(w, 5, mark_args, out, sizeof out);
        hotkey_notify_changed(i, w);
        return TG_OK;
    }
    char *sel_args[] = { w->path, "selection", "range", "0", "end" };
    if (tg_entry_command(w, 5, sel_args, out, sizeof out) != TG_OK) return TG_ERROR;
    char *cur_args[] = { w->path, "icursor", "end" };
    (void)tg_entry_command(w, 3, cur_args, out, sizeof out);
    hotkey_notify_changed(i, w);
    return TG_OK;
}

static int hotkey_action_undo_redo(tg_interp *i, tg_widget *w, int redo) {
    if (!w || w->kind != TG_TEXT) return TG_OK;
    if (w->state != TG_STATE_NORMAL) return TG_OK;
    char out[256];
    char *args[] = { w->path, "edit", redo ? "redo" : "undo" };
    if (tg_document_command(w, 3, args, out, sizeof out) == TG_OK) {
        hotkey_notify_changed(i, w);
    }
    return TG_OK;
}

static int hotkey_run_virtual_then_default(tg_interp *i, tg_widget *w, const char *vname, int (*action)(tg_interp *, tg_widget *)) {
    tg_id wid = w->id;
    tg_event ve = { tg_virtual_event(vname), wid, 0, 0, 0 };
    if (tg_binding_dispatch(i, ve)) return TG_OK;
    w = tg_lookup(tg_interp_ui(i), wid);
    if (!w) return TG_OK;
    return action(i, w);
}

int tg_hotkey_dispatch(tg_interp *i, tg_event e) {
    if (!i || e.kind != TG_KEY_DOWN) return 0;
    tg_ui *u = tg_interp_ui(i);
    if (!u) return 0;
    tg_widget *w = tg_lookup(u, e.target);
    if (!hotkey_is_text_widget(w)) {
        w = tg_lookup(u, tg_ui_focused(u));
    }
    if (!hotkey_is_text_widget(w)) return 0;

    unsigned ctrl = (e.key & TG_MOD_CONTROL) != 0;
    unsigned shift = (e.key & TG_MOD_SHIFT) != 0;
    unsigned alt = (e.key & TG_MOD_ALT) != 0;
    if (alt) return 0; /* Preserve AltGr and Alt+key accelerators. */

    uint32_t k = e.key & TG_KEY_VALUE_MASK;
    if (k >= 'A' && k <= 'Z') k = k - 'A' + 'a';

    if ((ctrl && !shift && k == 'c') || (ctrl && !shift && k == TG_KEY_INSERT)) {
        (void)hotkey_run_virtual_then_default(i, w, "Copy", hotkey_action_copy);
        return 1;
    }
    if ((ctrl && !shift && k == 'x') || (!ctrl && shift && k == TG_KEY_DELETE)) {
        (void)hotkey_run_virtual_then_default(i, w, "Cut", hotkey_action_cut);
        return 1;
    }
    if ((ctrl && !shift && k == 'v') || (!ctrl && shift && k == TG_KEY_INSERT)) {
        (void)hotkey_run_virtual_then_default(i, w, "Paste", hotkey_action_paste);
        return 1;
    }
    if (ctrl && !shift && k == 'a') {
        (void)hotkey_run_virtual_then_default(i, w, "SelectAll", hotkey_action_select_all);
        return 1;
    }
    if (ctrl && !shift && k == 'z') {
        if (w->kind == TG_TEXT) {
            tg_id wid = w->id;
            if (!tg_binding_dispatch(i, (tg_event){ tg_virtual_event("Undo"), wid, 0, 0, 0 })) {
                w = tg_lookup(u, wid);
                if (w) (void)hotkey_action_undo_redo(i, w, 0);
            }
            return 1;
        }
        return 0;
    }
    if ((ctrl && !shift && k == 'y') || (ctrl && shift && k == 'z')) {
        if (w->kind == TG_TEXT) {
            tg_id wid = w->id;
            if (!tg_binding_dispatch(i, (tg_event){ tg_virtual_event("Redo"), wid, 0, 0, 0 })) {
                w = tg_lookup(u, wid);
                if (w) (void)hotkey_action_undo_redo(i, w, 1);
            }
            return 1;
        }
        return 0;
    }
    return 0;
}

static int ieq(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == 0 && *b == 0;
}

static int handle_clipboard_tcl(Tcl_Interp *i, size_t n, char **a) {
    if (n < 2) return tg_set_result(i, "wrong # args: should be \"clipboard option ?arg ...?\"", TG_ERROR);
    const char *sub = a[1];
    const char *displayof = NULL;
    size_t k = 2;

    if (!strcmp(sub, "clear")) {
        while (k < n) {
            if (!strcmp(a[k], "-displayof") && k + 1 < n) { displayof = a[k + 1]; k += 2; }
            else return tg_set_result(i, "unsupported clipboard clear option", TG_ERROR);
        }
        (void)hotkey_clipboard_put(hotkey_native_hwnd(i, displayof), "");
        return tg_set_result(i, "", TG_OK);
    }

    if (!strcmp(sub, "append")) {
        while (k < n && a[k][0] == '-') {
            if (!strcmp(a[k], "--")) { k++; break; }
            if ((!strcmp(a[k], "-displayof") || !strcmp(a[k], "-format") || !strcmp(a[k], "-type")) && k + 1 < n) {
                if (!strcmp(a[k], "-displayof")) displayof = a[k + 1];
                k += 2;
            } else {
                break;
            }
        }
        if (k + 1 != n) return tg_set_result(i, "wrong # args: should be \"clipboard append ?options? data\"", TG_ERROR);
        const char *data = a[k];
        void *hwnd = hotkey_native_hwnd(i, displayof);
        size_t old_len = 0;
        char *old = hotkey_clipboard_get_alloc(hwnd, &old_len);
        size_t add_len = strlen(data);
        size_t total = old_len + add_len;
        if (total >= TG_RESULT_CAP) total = TG_RESULT_CAP - 1;
        char *combined = malloc(total + 1);
        if (!combined) { free(old); return tg_set_result(i, "out of memory", TG_ERROR); }
        size_t copy_old = old_len <= total ? old_len : total;
        if (old && copy_old) memcpy(combined, old, copy_old);
        size_t copy_add = total - copy_old;
        if (copy_add) memcpy(combined + copy_old, data, copy_add);
        combined[total] = 0;
        free(old);
        int ok = hotkey_clipboard_put(hwnd, combined);
        free(combined);
        if (!ok) return tg_set_result(i, "clipboard is unavailable", TG_ERROR);
        return tg_set_result(i, "", TG_OK);
    }

    if (!strcmp(sub, "get")) {
        while (k < n) {
            if ((!strcmp(a[k], "-displayof") || !strcmp(a[k], "-type")) && k + 1 < n) {
                if (!strcmp(a[k], "-displayof")) displayof = a[k + 1];
                k += 2;
            } else {
                return tg_set_result(i, "unsupported clipboard get option", TG_ERROR);
            }
        }
        void *hwnd = hotkey_native_hwnd(i, displayof);
        char *buf = hotkey_clipboard_get_alloc(hwnd, NULL);
        if (!buf) return tg_set_result(i, "CLIPBOARD selection doesn't exist or form \"STRING\" not defined", TG_ERROR);
        int rc = tg_set_result(i, buf, TG_OK);
        free(buf);
        return rc;
    }

    return tg_set_result(i, "unsupported clipboard subcommand", TG_ERROR);
}

static int handle_selection_tcl(Tcl_Interp *i, size_t n, char **a) {
    if (n < 2) return tg_set_result(i, "wrong # args: should be \"selection option ?arg ...?\"", TG_ERROR);
    const char *sub = a[1];
    const char *displayof = NULL;
    const char *sel_name = "PRIMARY";
    size_t k = 2;
    while (k < n) {
        if ((!strcmp(a[k], "-displayof") || !strcmp(a[k], "-selection") || !strcmp(a[k], "-type")) && k + 1 < n) {
            if (!strcmp(a[k], "-displayof")) displayof = a[k + 1];
            else if (!strcmp(a[k], "-selection")) sel_name = a[k + 1];
            k += 2;
        } else {
            break;
        }
    }

    if (!strcmp(sub, "get")) {
        if (ieq(sel_name, "CLIPBOARD")) {
            char *buf = hotkey_clipboard_get_alloc(hotkey_native_hwnd(i, displayof), NULL);
            if (!buf) return tg_set_result(i, "CLIPBOARD selection doesn't exist or form \"STRING\" not defined", TG_ERROR);
            int rc = tg_set_result(i, buf, TG_OK);
            free(buf);
            return rc;
        }
        tg_ui *u = tg_interp_ui(i);
        tg_widget *w = displayof ? tg_lookup(u, tg_ui_find(u, displayof)) : NULL;
        if (!hotkey_is_text_widget(w)) w = tg_lookup(u, tg_ui_focused(u));
        if (w && w->kind == TG_TEXT && hotkey_text_has_selection(w)) {
            char *buf = malloc(TG_RESULT_CAP);
            if (!buf) return tg_set_result(i, "out of memory", TG_ERROR);
            char *args[] = { w->path, "get", "sel.first", "sel.last" };
            int ok = tg_document_command(w, 4, args, buf, TG_RESULT_CAP) == TG_OK;
            int rc = ok ? tg_set_result(i, buf, TG_OK) : tg_set_result(i, "PRIMARY selection doesn't exist", TG_ERROR);
            free(buf);
            return rc;
        }
        if (w && hotkey_is_entry_like(w->kind) && !w->show[0]) {
            size_t first = hotkey_utf8_byte_at(w->text, w->sel_first);
            size_t last = hotkey_utf8_byte_at(w->text, w->sel_last);
            if (last > first) {
                char buf[sizeof w->text];
                size_t len = last - first;
                if (len >= sizeof buf) len = sizeof buf - 1;
                memcpy(buf, w->text + first, len);
                buf[len] = 0;
                return tg_set_result(i, buf, TG_OK);
            }
        }
        return tg_set_result(i, "PRIMARY selection doesn't exist", TG_ERROR);
    }

    if (!strcmp(sub, "clear")) {
        if (ieq(sel_name, "CLIPBOARD")) {
            (void)hotkey_clipboard_put(hotkey_native_hwnd(i, displayof), "");
        }
        return tg_set_result(i, "", TG_OK);
    }

    return tg_set_result(i, "", TG_OK);
}

int tg_hotkey_command(Tcl_Interp *i, size_t n, char **a) {
    if (!i || n < 1 || !a || !a[0]) return -1;
    if (!strcmp(a[0], "clipboard")) return handle_clipboard_tcl(i, n, a);
    if (!strcmp(a[0], "selection")) return handle_selection_tcl(i, n, a);

    if (n >= 4 && !strcmp(a[0], "event") && !strcmp(a[1], "generate")) {
        int is_copy = !strcmp(a[3], "<<Copy>>");
        int is_cut = !strcmp(a[3], "<<Cut>>");
        int is_paste = !strcmp(a[3], "<<Paste>>");
        int is_select_all = !strcmp(a[3], "<<SelectAll>>");
        int is_undo = !strcmp(a[3], "<<Undo>>");
        int is_redo = !strcmp(a[3], "<<Redo>>");
        if (!is_copy && !is_cut && !is_paste && !is_select_all && !is_undo && !is_redo) return -1;

        tg_ui *u = tg_interp_ui(i);
        tg_widget *w = tg_lookup(u, tg_ui_find(u, a[2]));
        if (!hotkey_is_text_widget(w)) return -1;

        int rc = TG_OK;
        if (is_copy) rc = hotkey_action_copy(i, w);
        else if (is_cut) rc = hotkey_action_cut(i, w);
        else if (is_paste) rc = hotkey_action_paste(i, w);
        else if (is_select_all) rc = hotkey_action_select_all(i, w);
        else if (is_undo) rc = hotkey_action_undo_redo(i, w, 0);
        else if (is_redo) rc = hotkey_action_undo_redo(i, w, 1);

        if (rc != TG_OK) return tg_set_result(i, "clipboard action failed", TG_ERROR);
        return tg_set_result(i, "", TG_OK);
    }
    return -1;
}
