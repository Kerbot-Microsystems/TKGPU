"""Real CPython/Pillow/application-dialog checks against the independent DLL.

No login, license activation, network submission or real credentials are used.
Run with the checkout's Python after tkgpu_kerbot_candidate has been built.
"""
from pathlib import Path
import os
import sys
import unittest
from unittest.mock import patch

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "src/kerbot_drone"))
os.environ["TKGPU_PROBE"] = "1"

from lowlevel.tkgpu_runtime import activate_requested_runtime
activate_requested_runtime()

import tkinter as tk
from PIL import Image, ImageTk
from utils.fonts.font_loader import load_fonts_from_assets
load_fonts_from_assets()


class DesktopSmoke(unittest.TestCase):
    def setUp(self):
        self.root = tk.Tk()
        self.root.withdraw()
        self.callbacks = []
        self.root.report_callback_exception = lambda *args: self.callbacks.append(args)

    def tearDown(self):
        self.root.destroy()
        self.assertEqual(self.callbacks, [])

    def test_independent_toplevels_and_photos(self):
        first, second = tk.Toplevel(self.root), tk.Toplevel(self.root)
        first.geometry("420x300+10+20")
        second.geometry("310x240+30+40")
        first.title("First")
        second.title("Second")
        self.assertEqual(first.title(), "First")
        self.assertEqual(second.title(), "Second")
        self.assertNotEqual(first.winfo_id(), second.winfo_id())
        first.resizable(False, False)
        image = ImageTk.PhotoImage(Image.new("RGBA", (20, 18), (17, 44, 99, 128)), master=first)
        label = tk.Label(first, image=image)
        label.pack()
        self.assertEqual(tuple(map(int, self.root.tk.splitlist(image._PhotoImage__photo.get(0, 0)))), (17, 44, 99))
        self.root.update()
        self.assertEqual((first.winfo_width(), first.winfo_height()), (420, 300))
        self.assertEqual((second.winfo_width(), second.winfo_height()), (310, 240))
        if sys.platform == "win32":
            import ctypes
            style = ctypes.windll.user32.GetWindowLongW(int(first.winfo_id()), -16)
            self.assertEqual(style & 0x10000, 0, "Nonresizable dialog must not offer maximize")
        first.destroy()
        self.assertTrue(second.winfo_exists())

    @unittest.skipUnless(sys.platform == "win32", "Windows wm attributes")
    def test_custom_window_alpha_fade_and_completion(self):
        from types import SimpleNamespace
        from ui.startup.loading_screen import _fade_out_then_run
        splash = tk.Toplevel(self.root)
        try:
            splash.overrideredirect(True)
            splash.attributes("-alpha", 1.0)
            self.assertIsInstance(splash.attributes("-alpha"), float)
            self.assertEqual(splash.attributes("-alpha"), 1.0)
            splash.attributes("-topmost", True)
            self.assertIsInstance(splash.attributes("-topmost"), int)
            splash.attributes("-alpha", 1.05)
            self.assertEqual(splash.attributes("-alpha"), 1.0)
            splash.attributes("-alpha", -0.05)
            self.assertEqual(splash.attributes("-alpha"), 0.0)
            splash.attributes("-alpha", 0.07)
            pending = []
            completed = []
            model = SimpleNamespace(master=splash, _after_ids=[],
                                    cancel_all_after=lambda: None)
            def capture_after(delay, callback, *args):
                pending.append((callback, args))
                return str(len(pending))
            with patch.object(splash, "after", side_effect=capture_after):
                _fade_out_then_run(model, lambda: completed.append(True))
                for _ in range(10):
                    if not pending:
                        break
                    callback, args = pending.pop(0)
                    callback(*args)
            self.assertEqual(completed, [True])
            self.assertEqual(splash.attributes("-alpha"), 0.0)
            self.assertFalse(pending)
        finally:
            splash.destroy()

    def test_recovery_actions_centered_and_unclipped(self):
        from ui.design.autosave_recovery_dialog import AutosaveRecoveryDialog
        from tkinter import font
        top = tk.Toplevel(self.root)
        try:
            top.geometry("800x600")
            self.root.update_idletasks()
            dialog = AutosaveRecoveryDialog(
                top, title="Відновити автоматично збережений проект?",
                message="Kerbot Drone знайшов автозбереження.",
                restore_text="Відновити", delete_text="Видалити автозбереження",
                skip_text="Пропустити", callback=lambda choice: None)
            dialog.show()
            self.root.update_idletasks()
            group = dialog.button_frame.winfo_children()[0]
            self.assertLessEqual(abs(2 * group.winfo_x() + group.winfo_width()
                                     - dialog.button_frame.winfo_width()), 2)
            self.assertEqual(len(group.winfo_children()), 3)
            for button in group.winfo_children():
                measured = font.Font(root=top, font=button.cget("font")).measure(button.cget("text"))
                self.assertGreaterEqual(button.winfo_width(), measured)
                self.assertLessEqual(button.winfo_x() + button.winfo_width(), group.winfo_width())
        finally:
            top.destroy()

    def test_real_base_dialog_geometry_lifecycle(self):
        from ui.design.custom_dialogs import BaseDialog
        top = tk.Toplevel(self.root)
        top.geometry("800x600")
        dialog = None
        try:
            top.update_idletasks()
            dialog = BaseDialog(top, "BaseDialog regression", width=400,
                                height=200, titlebar_height=32)
            self.assertTrue(dialog._native_dialog_active)
            dialog.show()
            self.root.update_idletasks()
            def rect(widget):
                return (widget.winfo_x(), widget.winfo_y(),
                        widget.winfo_width(), widget.winfo_height())
            self.assertEqual(rect(dialog), (200, 200, 400, 200))
            self.assertEqual(rect(dialog.inner_frame), (1, 1, 398, 198))
            self.assertEqual(dialog.border_width, 1)
            border = dialog._border_canvas
            self.assertEqual(
                sorted(tuple(map(int, border.coords(item))) for item in border.find_withtag("miniframe")),
                sorted([(0, 0, 400, 1), (0, 199, 400, 200),
                        (0, 1, 1, 199), (399, 1, 400, 199)]),
            )
            self.assertEqual(dialog.title_bar.winfo_height(), 32)
            for button in (dialog._close_btn, dialog._min_btn):
                self.assertEqual(button._container.winfo_width(), 46)
                self.assertEqual(button.winfo_width(), 46)
                self.assertEqual(button.winfo_height(), 32)
            self.assertEqual(dialog.title_bar.cget("cursor"), "fleur")
            original = rect(dialog)
            for _ in range(3):
                dialog._min_btn.invoke()
                self.root.update_idletasks()
                self.assertEqual(rect(dialog), (*original[:3], 38))
                self.assertEqual(dialog._border_canvas.winfo_height(), 38)
                self.assertEqual(dialog.content_frame.winfo_manager(), "")
                dialog._min_btn.invoke()
                self.root.update_idletasks()
                self.assertEqual(rect(dialog), original)
                self.assertEqual(dialog._border_canvas.winfo_height(), 200)
                self.assertEqual(dialog.content_frame.winfo_manager(), "pack")
            dialog._close_btn.invoke()
            self.root.update_idletasks()
            self.assertFalse(dialog.winfo_ismapped())
            self.assertFalse(dialog.content_frame.winfo_viewable())
            dialog.show()
            self.root.update_idletasks()
            self.assertEqual(rect(dialog), original)
            self.assertIsNone(dialog.get_result())
            self.assertFalse(self.callbacks)
        finally:
            if dialog is not None:
                dialog.destroy()
            top.destroy()

    def test_modal_wait_after_splash_callback_has_returned(self):
        # Test only the UI lifecycle, without invoking license operations.
        root = self.root
        splash = tk.Toplevel(root)
        marker = []
        try:
            def finish_splash():
                marker.append("callback")
                root.quit()
            root.after(1, finish_splash)
            root.mainloop()
            self.assertEqual(marker, ["callback"])
            splash.destroy()
            modal = tk.Toplevel(root)
            root.after(1, modal.destroy)
            root.wait_window(modal)
            marker.append("modal closed")
            self.assertEqual(marker, ["callback", "modal closed"])
        finally:
            if splash.winfo_exists():
                splash.destroy()

    def test_splash_borderless_and_canvas_layer_order(self):
        splash = tk.Toplevel(self.root)
        try:
            splash.overrideredirect(True)
            splash.geometry("700x500+20+20")
            canvas = tk.Canvas(splash, highlightthickness=0)
            canvas.pack(fill="both", expand=True)
            track = canvas.create_line(145, 372, 555, 372, width=28,
                                       capstyle="round", fill="#0A6385")
            fill = canvas.create_line(145, 372, 300, 372, width=24,
                                      capstyle="round", fill="#71E1FF")
            gradient = canvas.create_rectangle(0, 0, 700, 500,
                                               fill="#008AC5", outline="")
            canvas.lift(track)
            canvas.lift(fill)
            self.root.update_idletasks()
            self.assertTrue(splash.overrideredirect())
            self.assertEqual(canvas.find_all(), (gradient, track, fill))
            self.assertEqual((canvas.winfo_width(), canvas.winfo_height()), (700, 500))
            canvas.coords(fill, 145, 372, 450, 372)
            self.assertEqual(canvas.coords(fill), [145.0, 372.0, 450.0, 372.0])
            if sys.platform == "win32":
                import ctypes
                style = ctypes.windll.user32.GetWindowLongW(int(splash.winfo_id()), -16)
                self.assertEqual(style & 0x00C40000, 0, "No caption or sizing frame")
                splash.overrideredirect(False)
                self.assertFalse(splash.overrideredirect())
                style = ctypes.windll.user32.GetWindowLongW(int(splash.winfo_id()), -16)
                self.assertEqual(style & 0x00C00000, 0x00C00000)
        finally:
            splash.destroy()

    def test_unicode_entry_trace_selection_and_bindtags(self):
        variable = tk.StringVar(self.root, "Життя")
        seen = []
        trace = variable.trace_add("write", lambda *_: seen.append(variable.get()))
        entry = tk.Entry(self.root, font=("IBM Plex Sans", 14), textvariable=variable, bd=8)
        entry.pack()
        entry.insert("end", " 🚀")
        self.assertEqual(variable.get(), "Життя 🚀")
        self.assertEqual(seen[-1], "Життя 🚀")
        entry.focus_force()
        entry.icursor("end")
        entry.event_generate("<Left>")
        self.root.update()
        self.assertEqual(entry.index("insert"), len("Життя "))
        entry.select_range(0, 2)
        self.assertTrue(entry.selection_present())
        entry.delete("sel.first", "sel.last")
        self.assertEqual(entry.get(), "ття 🚀")
        entry.icursor("end")
        entry.event_generate("<Shift-Left>")
        self.root.update()
        self.assertEqual(entry.index("sel.first"), len("ття "))
        self.assertEqual(entry.index("sel.last"), len("ття 🚀"))
        second = tk.Entry(self.root, show="•")
        second.pack()
        self.root.update()
        entry.event_generate("<Tab>")
        self.root.update()
        self.assertEqual(self.root.focus_get(), second)
        second.insert("end", "local-test")
        self.assertEqual(second.get(), "local-test")
        entry.focus_force()
        called = []
        self.root.bind("<Return>", lambda _: called.append("root"))
        entry.event_generate("<Return>")
        self.root.update()
        self.assertEqual(called, ["root"])
        variable.trace_remove("write", trace)
        before = len(seen)
        variable.set("changed")
        self.assertEqual(len(seen), before)

    def assert_system_dialog_frame(self, window):
        self.assertFalse(window.overrideredirect())
        if sys.platform == "win32":
            import ctypes
            style = ctypes.windll.user32.GetWindowLongW(int(window.winfo_id()), -16)
            self.assertEqual(style & 0x00C00000, 0x00C00000, "WS_CAPTION missing")
            self.assertTrue(style & 0x00080000, "WS_SYSMENU missing")
            self.assertFalse(style & 0x00040000, "Unexpected resize border")

    def test_activation_choice_constructs_long_translated_descriptions(self):
        from ui.design.activation_choice_dialog import ActivationChoiceDialog
        dialog = ActivationChoiceDialog(self.root)
        self.root.update()
        self.assertTrue(dialog.dialog.winfo_exists())
        self.assertTrue(dialog._option_cards)
        self.assertTrue(dialog._system_frame)
        self.assert_system_dialog_frame(dialog.dialog)
        # Exercise the same translated multi-line card layout seen in the
        # Windows screenshots. No license action is performed.
        self.assertEqual(len(dialog._option_cards), 3)
        self.assertTrue(dialog._cards_canvas.bbox("all"))
        for card in dialog._option_cards:
            for widget in (card["header"], card["description"], card["button"]):
                self.assertGreaterEqual(widget.winfo_height(), widget.winfo_reqheight())
        dialog._cards_canvas.yview_moveto(1.0)
        self.root.update()
        for card in dialog._option_cards:
            button = card["button"]
            bottom = button.winfo_rooty() + button.winfo_height()
            viewport_bottom = (dialog._cards_canvas.winfo_rooty()
                               + dialog._cards_canvas.winfo_height())
            self.assertLessEqual(bottom, viewport_bottom)
        dialog._cards_canvas.yview_moveto(0.0)
        self.root.update()

        for card in dialog._option_cards:
            self.assertGreater(card["frame"].winfo_height(), 200)
            self.assertGreater(card["description"].winfo_height(), 30)
            self.assertGreater(card["button"].winfo_height(), 35)
            button = card["button"]
            self.assertLessEqual(button.winfo_y() + button.winfo_height(),
                                 button.master.winfo_height())

        dialog.dialog.geometry("900x700")
        self.root.update()
        for card in dialog._option_cards:
            self.assertGreater(card["frame"].winfo_height(), 200)
        dialog.dialog.destroy()

    def test_frame_padding_in_requested_and_actual_geometry(self):
        top = tk.Toplevel(self.root)
        try:
            top.geometry("300x200")
            parent = tk.Frame(top, padx=12, pady=7, borderwidth=2,
                              highlightthickness=1)
            parent.pack()
            child = tk.Frame(parent, width=40, height=20)
            child.pack()
            self.root.update_idletasks()
            self.assertEqual((parent.winfo_reqwidth(), parent.winfo_reqheight()),
                             (70, 40))
            self.assertEqual((child.winfo_x(), child.winfo_y()), (15, 10))
            self.assertEqual((child.winfo_width(), child.winfo_height()), (40, 20))
        finally:
            top.destroy()

    def test_virtual_events_and_themed_notebook_layout(self):
        from tkinter import ttk
        top = tk.Toplevel(self.root)
        try:
            top.geometry("500x300")
            style = ttk.Style(top)
            style.configure("Regression.TNotebook.Tab", padding=(14, 9),
                            font=("IBM Plex Sans", 10),
                            background="#eeeeee", foreground="#222222")
            style.map("Regression.TNotebook.Tab",
                      background=[("selected", "#2277cc")])
            empty = ttk.Frame(top)
            empty.pack(fill="x")
            tabs = ttk.Notebook(top, style="Regression.TNotebook")
            tabs.pack(fill="both", expand=True)
            first, second = ttk.Frame(tabs), ttk.Frame(tabs)
            tabs.add(first, text="First")
            tabs.add(second, text="Second")
            self.root.update()
            self.assertLessEqual(empty.winfo_height(), 1)
            self.assertGreaterEqual(first.winfo_y(), 28)
            seen = []
            tabs.bind("<<NotebookTabChanged>>", lambda event: seen.append(tabs.select()))
            tabs.select(second)
            self.root.update()
            self.assertEqual(seen, [str(second)])
            self.assertFalse(first.winfo_viewable())
            self.assertTrue(second.winfo_viewable())
            custom = []
            tabs.bind("<<RegressionEvent>>", lambda event: custom.append(str(event.widget)))
            tabs.event_generate("<<RegressionEvent>>", when="tail")
            self.root.update()
            self.assertEqual(custom, [str(tabs)])
            self.assertIn("TFrame", first.bindtags())
        finally:
            top.destroy()

    def test_rapid_notebook_switches_and_hit_testing(self):
        from tkinter import ttk
        top = tk.Toplevel(self.root)
        try:
            top.geometry("600x320")
            book = ttk.Notebook(top)
            book.pack(fill="both", expand=True)
            pages = [ttk.Frame(book) for _ in range(4)]
            for index, page in enumerate(pages):
                book.add(page, text=f"Page {index}")
                ttk.Label(page, text=f"Content {index}").pack()
            self.root.update()
            seen = []
            book.bind("<<NotebookTabChanged>>",
                      lambda event: seen.append(book.select()))
            for index in range(40):
                target = index % len(pages)
                box = tuple(map(int, book.tk.splitlist(
                    book.tk.call(str(book), "bbox", target))))
                self.assertEqual(len(box), 4)
                x, y, width, height = box
                self.assertGreater(width, 0)
                self.assertGreater(height, 0)
                self.assertEqual(book.index(f"@{x+width//2},{y+height//2}"), target)
                self.assertTrue(book.identify(x+width//2, y+height//2))
                book.select(pages[target])
                self.root.update()
                self.assertEqual(book.select(), str(pages[target]))
                self.assertEqual([bool(p.winfo_viewable()) for p in pages],
                                 [j == target for j in range(len(pages))])
            self.assertEqual(len(seen), 39)
            self.assertEqual(seen[-1], str(pages[-1]))
            # Burst of requests without waiting between selections.
            for index in range(100):
                book.select(pages[index % len(pages)])
            self.root.update()
            self.assertEqual(book.select(), str(pages[-1]))
            self.assertFalse(self.callbacks)
        finally:
            top.destroy()

    def test_ttk_asymmetric_padding_and_live_geometry(self):
        from tkinter import ttk
        top = tk.Toplevel(self.root)
        try:
            top.geometry("320x240")
            parent = ttk.Frame(top, padding=(10, 3, 20, 7))
            parent.pack(fill="both", expand=True)
            child = ttk.Frame(parent)
            child.pack(fill="both", expand=True)
            self.root.update_idletasks()
            self.assertEqual((child.winfo_x(), child.winfo_y()), (10, 3))
            self.assertEqual((child.winfo_width(), child.winfo_height()), (290, 230))
            top.geometry("440x340")
            self.root.update_idletasks()
            self.assertEqual((child.winfo_width(), child.winfo_height()), (410, 330))
        finally:
            top.destroy()

    def test_wheel_custom_bindtag_and_canvas_fallback(self):
        top = tk.Toplevel(self.root)
        try:
            top.geometry("320x240")
            canvas = tk.Canvas(top, scrollregion=(0, 0, 300, 1200))
            canvas.pack(fill="both", expand=True)
            child = tk.Label(canvas, text="Wheel")
            canvas.create_window((10, 10), window=child, anchor="nw")
            self.root.update()
            received = []
            tag = "RegressionWheelTag"
            child.bindtags((str(child), tag, *child.bindtags()[1:]))
            child.bind_class(tag, "<MouseWheel>",
                             lambda event: received.append(event.delta) or "break")
            child.bind_class(tag, "<Button-5>",
                             lambda event: received.append("wrong binding"))
            child.event_generate("<MouseWheel>", delta=-120, when="tail")
            child.event_generate("<MouseWheel>", delta=120, when="tail")
            self.root.update()
            self.assertEqual(received, [-120, 120])
            child.unbind_class(tag, "<MouseWheel>")
            child.unbind_class(tag, "<Button-5>")
            before = canvas.yview()[0]
            child.event_generate("<MouseWheel>", delta=-120, when="tail")
            self.root.update()
            self.assertGreater(canvas.yview()[0], before)
        finally:
            top.destroy()

    def test_rich_summary_failed_creation_and_repeated_updates(self):
        from ui.panels.rich_details import RichDetailsPanel
        top = tk.Toplevel(self.root)
        try:
            top.geometry("400x600")
            panel = RichDetailsPanel(top)
            panel.pack(fill="both", expand=True)
            payload = {"title": "Gesamtkosten",
                       "pills": ["0 Komponenten", "0 Verbindungen"],
                       "rows": [{"label": "USD", "value": "$0.00"}]}
            before = list(panel.content.winfo_children())
            with patch.object(panel, "_update_summary_view",
                              side_effect=tk.TclError("injected render failure")):
                with self.assertRaises(tk.TclError):
                    panel._render_summary(payload)
            self.assertEqual(panel.content.winfo_children(), before)
            self.assertIsNone(panel._summary_view)
            panel._render_summary(payload)
            self.root.update_idletasks()
            first = panel._summary_view["card"]["wrap"]
            for value in ("$1.00", "$2.00", "$3.00"):
                panel._render_summary(dict(payload, rows=[{"label": "USD", "value": value}]))
                self.root.update_idletasks()
                self.assertIs(panel._summary_view["card"]["wrap"], first)
                self.assertEqual(panel.content.winfo_children(), before + [first])
                self.assertEqual(panel._summary_view["row_views"][0]["value"].cget("text"), value)
            panel._clear_summary()
            self.root.update_idletasks()
            self.assertEqual(panel.content.winfo_children(), before)
        finally:
            top.destroy()

    def test_nested_widget_paths_and_capacity(self):
        top = tk.Toplevel(self.root)
        try:
            parent = top
            for index in range(10):
                parent = tk.Frame(parent, name=f"nested_container_{index:02d}")
            child = tk.Label(parent, name="final_label", text="Nested")
            self.assertGreater(len(str(child)), 127)
            self.assertTrue(child.winfo_exists())
            self.assertEqual(str(child.winfo_parent()), str(parent))
            self.assertEqual(child.bindtags()[0], str(child))
            for group in range(25):
                frame = tk.Frame(top, name=f"group{group}")
                for index in range(48):
                    self.assertTrue(tk.Label(frame, name=f"label{index}", text="x").winfo_exists())
            top.destroy()
            self.assertFalse(top.winfo_exists())
            replacement = tk.Toplevel(self.root)
            try:
                self.assertTrue(tk.Label(replacement, text="After cleanup").winfo_exists())
            finally:
                replacement.destroy()
        finally:
            if top.winfo_exists():
                top.destroy()

    def test_programmatic_focus_notifications_and_search_hint(self):
        top = tk.Toplevel(self.root)
        try:
            top.geometry("420x200")
            hint = "Projekt, Details und Hilfe durchsuchen"
            value = tk.StringVar(top, value=hint)
            search = tk.Entry(top, textvariable=value)
            other = tk.Entry(top)
            search.pack()
            other.pack()
            seen = []
            def focus_in(event):
                seen.append("in")
                if search.get() == hint:
                    search.delete(0, tk.END)
            def focus_out(event):
                seen.append("out")
                if not search.get():
                    search.insert(0, hint)
            search.bind("<FocusIn>", focus_in)
            search.bind("<FocusOut>", focus_out)
            self.root.update()
            other.focus_set()
            self.root.update()
            seen.clear()
            # Match the real titlebar: Button-1 calls focus_set before
            # the normal entry handler sets its cursor position.
            search.bind("<Button-1>", lambda event: search.focus_set(), add="+")
            search.event_generate("<Button-1>", x=4, y=4, when="tail")
            self.root.update()
            self.assertEqual(seen, ["in"])
            self.assertEqual(value.get(), "")
            search.insert(tk.END, "test")
            self.assertEqual(value.get(), "test")
            search.focus_set()
            self.root.update()
            self.assertEqual(seen, ["in"])
            other.focus_set()
            self.root.update()
            self.assertEqual(seen, ["in", "out"])
            self.assertEqual(value.get(), "test")
            search.focus_set()
            self.root.update()
            search.delete(0, tk.END)
            other.focus_set()
            self.root.update()
            self.assertEqual(value.get(), hint)
        finally:
            top.destroy()

    @unittest.skipUnless(sys.platform == "win32", "Native Windows message")
    def test_native_wheel_scrollregion_and_callback(self):
        import ctypes
        from ctypes import wintypes
        top = tk.Toplevel(self.root)
        try:
            top.geometry("320x240+100+100")
            fractions = []
            canvas = tk.Canvas(top, bg="white", highlightthickness=1,
                               scrollregion=(0, 0, 300, 1200),
                               yscrollcommand=lambda a, b: fractions.append((float(a), float(b))))
            canvas.pack(fill="both", expand=True)
            child = tk.Label(canvas, text="Native wheel target")
            canvas.create_window((10, 10), window=child, anchor="nw")
            self.root.update()
            self.assertEqual(tuple(map(int, top.tk.splitlist(canvas.cget("scrollregion")))),
                             (0, 0, 300, 1200))
            seen = []
            def wheel(event):
                seen.append(event.delta)
                canvas.yview_scroll(-int(event.delta / 120), "units")
                return "break"
            child.bind("<MouseWheel>", wheel)
            user32 = ctypes.windll.user32
            send = user32.SendMessageW
            send.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
            send.restype = ctypes.c_ssize_t
            before = canvas.yview()[0]
            point = (child.winfo_rootx() + 2, child.winfo_rooty() + 2)
            coords = (point[0] & 0xffff) | ((point[1] & 0xffff) << 16)
            send(int(top.winfo_id()), 0x020A, ((-120) & 0xffff) << 16, coords)
            self.root.update()
            self.assertEqual(seen, [-120])
            self.assertGreater(canvas.yview()[0], before)
            self.assertTrue(fractions)
            self.assertGreater(fractions[-1][0], before)
        finally:
            top.destroy()

    def test_widget_error_identifies_option_and_path(self):
        top = tk.Toplevel(self.root)
        try:
            path = str(top) + ".bad_option"
            with self.assertRaisesRegex(tk.TclError, "-unknown_regression_option"):
                top.tk.call("frame", path, "-unknown_regression_option", "1")
            self.assertFalse(int(top.tk.call("winfo", "exists", path)))
            child = tk.Frame(top)
            with self.assertRaisesRegex(tk.TclError, "-unknown_regression_option"):
                child.configure(unknown_regression_option=1)
        finally:
            top.destroy()

    @unittest.skipUnless(sys.platform == "win32", "Windows native file dialog")
    def test_file_dialog_resolves_non_root_parent_before_options(self):
        top = tk.Toplevel(self.root)
        try:
            child = tk.Frame(top)
            with self.assertRaisesRegex(tk.TclError, "unsupported file dialog option"):
                top.tk.call("tk_getOpenFile", "-parent", str(child),
                            "-unknown_regression_option", "1")
            with self.assertRaisesRegex(tk.TclError, "dialog parent does not exist"):
                top.tk.call("tk_getOpenFile", "-parent", str(top) + ".missing",
                            "-unknown_regression_option", "1")
        finally:
            top.destroy()

    def test_canvas_removed_items_and_image_anchors(self):
        top = tk.Toplevel(self.root)
        try:
            canvas = tk.Canvas(top, width=200, height=160)
            canvas.pack()
            item = canvas.create_rectangle(10, 10, 20, 20)
            canvas.delete(item)
            self.assertEqual(canvas.coords(item), [])
            canvas.coords(item, 30, 30, 40, 40)
            canvas.itemconfigure(item, fill="red")
            photo = tk.PhotoImage(master=top, width=20, height=10)
            for anchor, expected in (
                ("nw", (50, 50, 70, 60)),
                ("n", (40, 50, 60, 60)),
                ("ne", (30, 50, 50, 60)),
                ("w", (50, 45, 70, 55)),
                ("center", (40, 45, 60, 55)),
                ("e", (30, 45, 50, 55)),
                ("sw", (50, 40, 70, 50)),
                ("s", (40, 40, 60, 50)),
                ("se", (30, 40, 50, 50)),
            ):
                image = canvas.create_image(50, 50, image=photo, anchor=anchor)
                self.assertEqual(canvas.bbox(image), expected)
                canvas.delete(image)
            with self.assertRaisesRegex(tk.TclError, "unsupported canvas operation"):
                canvas.tk.call(str(canvas), "create", "rectangle",
                               1, 1, 5, 5, "-unknown_regression_option", "1")
        finally:
            top.destroy()

    @unittest.skipUnless(sys.platform == "win32", "Native Windows keyboard")
    def test_native_terminal_entry_and_text_scroll(self):
        import ctypes
        from ctypes import wintypes
        from tkinter import ttk
        top = tk.Toplevel(self.root)
        try:
            top.geometry("480x280")
            fractions = []
            entry = ttk.Entry(top, font=("Consolas", 10))
            entry.pack(fill="x")
            log = tk.Text(top, height=5, font=("Consolas", 10),
                          yscrollcommand=lambda a, b: fractions.append((float(a), float(b))))
            log.pack(fill="both", expand=True)
            received = []
            def submit(event):
                received.append((event.keysym, entry.get()))
                log.insert("end", entry.get() + "\n")
                entry.delete(0, "end")
                return "break"
            entry.bind("<Return>", submit)
            self.root.update()
            entry.focus_set()
            self.root.update()
            entry.insert(0, "test")
            send = ctypes.windll.user32.SendMessageW
            send.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
            send.restype = ctypes.c_ssize_t
            send(int(top.winfo_id()), 0x0100, 0x0d, 1)
            send(int(top.winfo_id()), 0x0101, 0x0d, 1)
            self.root.update()
            self.assertEqual(received, [("Return", "test")])
            self.assertEqual(entry.get(), "")
            self.assertEqual(log.get("1.0", "1.4"), "test")
            log.insert("end", "line\n" * 100)
            log.see("end")
            self.root.update()
            self.assertTrue(fractions)
            self.assertGreater(fractions[-1][0], 0)
            self.assertAlmostEqual(fractions[-1][1], 1.0)
            log.yview_moveto(0)
            self.root.update()
            self.assertAlmostEqual(fractions[-1][0], 0.0)
        finally:
            top.destroy()

    def test_native_pack_grid_defaults_and_hidden_children(self):
        top = tk.Toplevel(self.root)
        try:
            top.geometry("320x240")
            holder = tk.Frame(top, width=300, height=100)
            holder.pack(fill="x")
            holder.pack_propagate(False)
            child = tk.Frame(holder, width=40, height=20)
            child.pack()
            gridhost = tk.Frame(top, width=300, height=100)
            gridhost.pack(fill="x")
            gridhost.grid_propagate(False)
            gridhost.grid_columnconfigure(0, weight=1)
            gridhost.grid_rowconfigure(0, weight=1)
            cell = tk.Frame(gridhost, width=40, height=20)
            cell.grid(row=0, column=0)
            self.root.update_idletasks()
            self.assertEqual(child.pack_info()["fill"], "none")
            self.assertEqual((child.winfo_width(), child.winfo_height()), (40, 20))
            self.assertEqual((cell.winfo_width(), cell.winfo_height()), (40, 20))
            self.assertEqual(child.winfo_x(), (holder.winfo_width() - 40) // 2)
            self.assertEqual(cell.winfo_x(), (gridhost.winfo_width() - 40) // 2)
            child.pack_forget()
            self.root.update_idletasks()
            self.assertFalse(child.winfo_ismapped())
            self.assertFalse(child.winfo_viewable())
            self.assertEqual(top.bindtags().count(str(top)), 1)
        finally:
            top.destroy()

    def test_geometry_matches_reference_tk(self):
        # Expected pixel geometry was measured on Tk 8.6 with Xvfb.
        # Plain frames isolate layout algorithms from font/theme differences.
        top = tk.Toplevel(self.root)
        try:
            top.geometry("400x400")
            def geometry(widget):
                return (widget.winfo_x(), widget.winfo_y(),
                        widget.winfo_width(), widget.winfo_height())
            for case, expected in enumerate((
                ((0, 0, 60, 20), (60, 0, 60, 20)),
                ((0, 0, 40, 20), (40, 0, 60, 20)),
                ((0, 0, 55, 20), (55, 0, 45, 20)),
                ((0, 0, 55, 20), (55, 0, 45, 20)),
                ((0, 0, 75, 20), (75, 0, 25, 20)),
            )):
                host = tk.Frame(top, width=100, height=60, borderwidth=0,
                                highlightthickness=0)
                host.place(x=0, y=case * 70)
                host.grid_propagate(False)
                a = tk.Frame(host, width=60, height=20)
                b = tk.Frame(host, width=60, height=20)
                a.grid(row=0, column=0, sticky="nsew")
                b.grid(row=0, column=1, sticky="nsew")
                if case in (1, 2, 3):
                    host.grid_columnconfigure(0, weight=1)
                if case in (2, 3):
                    host.grid_columnconfigure(1, weight=3)
                if case == 3:
                    host.grid_columnconfigure(0, minsize=55)
                if case == 4:
                    host.grid_columnconfigure(0, pad=15)
                    host.grid_columnconfigure(1, weight=1)
                self.root.update_idletasks()
                self.assertEqual((geometry(a), geometry(b)), expected)
            host = tk.Frame(top, width=101, height=61,
                            borderwidth=5, highlightthickness=2)
            host.place(x=150, y=0)
            child = tk.Frame(host, width=20, height=10)
            child.place(relx=0.5, rely=0.5, relwidth=0.5, relheight=0.5)
            self.root.update_idletasks()
            self.assertEqual(geometry(child), (51, 31, 43, 23))
            child.place(x=-10, y=-5, width=20, height=10, relwidth="",
                        relheight="", relx=0, rely=0)
            self.root.update_idletasks()
            self.assertEqual(geometry(child), (-3, 2, 20, 10))
            child.place(bordermode="outside")
            self.root.update_idletasks()
            self.assertEqual(geometry(child), (-10, -5, 20, 10))
            child.configure(background="red")
            self.root.update_idletasks()
            self.assertEqual(geometry(child), (-10, -5, 20, 10))
            packhost = tk.Frame(top, width=100, height=50)
            packhost.place(x=270, y=0)
            packhost.pack_propagate(False)
            first = tk.Frame(packhost, width=20, height=10)
            second = tk.Frame(packhost, width=20, height=10)
            first.pack(side="left", padx=5, ipadx=3)
            second.pack()
            first.pack_forget()
            first.pack()
            self.root.update_idletasks()
            self.assertEqual(packhost.pack_slaves(), [second, first])
            self.assertEqual(first.pack_info()["side"], "top")
            self.assertEqual(int(first.pack_info()["padx"]), 0)
            self.assertEqual(int(first.pack_info()["ipadx"]), 0)
            host = tk.Frame(top, width=100, height=100)
            host.place(x=150, y=100)
            host.grid_propagate(False)
            a = tk.Frame(host, width=20, height=10)
            b = tk.Frame(host, width=30, height=10)
            a.grid()
            b.grid()
            host.grid_anchor("se")
            self.root.update_idletasks()
            self.assertEqual(geometry(a), (75, 80, 20, 10))
            self.assertEqual(geometry(b), (70, 90, 30, 10))
            host = tk.Frame(top, width=100, height=100)
            host.place(x=150, y=220)
            child = tk.Frame(host, width=20, height=30)
            child.pack()
            self.root.update_idletasks()
            self.assertEqual((host.winfo_reqwidth(), host.winfo_reqheight()), (20, 30))
        finally:
            top.destroy()

    def test_borderless_tooltip_uses_content_size(self):
        top = tk.Toplevel(self.root)
        try:
            top.overrideredirect(True)
            top.geometry("+20+40")
            child = tk.Frame(top, width=120, height=30)
            child.pack()
            self.root.update_idletasks()
            self.assertEqual((top.winfo_width(), top.winfo_height()), (120, 30))
            top.geometry("200x100+20+40")
            self.root.update_idletasks()
            self.assertEqual((top.winfo_width(), top.winfo_height()), (200, 100))
        finally:
            top.destroy()

    def test_home_overlay_and_asymmetric_grid_padding(self):
        top = tk.Toplevel(self.root)
        try:
            top.geometry("800x600")
            overlay = tk.Frame(top)
            overlay.place(relx=0, rely=0, relwidth=1, relheight=1, y=32, height=-32)
            canvas = tk.Canvas(overlay, highlightthickness=0)
            canvas.pack(fill="both", expand=True)
            content = tk.Frame(canvas)
            item = canvas.create_window(0, 0, window=content, anchor="nw", tags="content")
            canvas.bind("<Configure>", lambda e: canvas.itemconfigure(item, width=e.width, height=e.height))
            content.grid_columnconfigure(0, weight=1)
            card = tk.Frame(content, width=100, height=40)
            card.grid(row=0, column=0, padx=(0, 20), pady=(5, 0), sticky="ew")
            self.root.update()
            self.assertEqual(overlay.winfo_y(), 32)
            self.assertEqual(overlay.winfo_height(), 568)
            self.assertEqual(canvas.winfo_width(), 800)
            self.assertEqual(card.winfo_width(), 780)
            self.assertEqual(card.winfo_y(), 5)
        finally:
            top.destroy()

    def test_idle_geometry_recalculates_nested_containers(self):
        frame = tk.Frame(self.root)
        frame.pack()
        child = tk.Frame(frame)
        child.grid(row=0, column=0)
        label = tk.Label(child, text="Short")
        label.pack()
        self.root.update_idletasks()
        before = frame.winfo_reqwidth()
        label.configure(text="A considerably longer label changes the container width")
        self.root.update_idletasks()
        self.assertGreater(frame.winfo_reqwidth(), before + 100)

    def test_layout_updates_during_mainloop_without_pointer_input(self):
        from ui.design.activation_choice_dialog import ActivationChoiceDialog
        dialog = ActivationChoiceDialog(self.root)
        observed = []
        def resize():
            dialog.dialog.geometry("900x700")
            dialog.dialog.after(120, inspect_layout)
        def inspect_layout():
            observed.extend(card["frame"].winfo_height() for card in dialog._option_cards)
            self.root.quit()
        dialog.dialog.after(30, resize)
        watchdog = self.root.after(2000, self.root.quit)
        self.root.mainloop()
        self.root.after_cancel(watchdog)
        self.assertEqual(len(observed), 3)
        self.assertTrue(all(height > 200 for height in observed), observed)
        dialog.dialog.destroy()

    def test_license_input_constructs_without_activation(self):
        from ui.design.activation_dialog import ActivationDialog
        with patch("ui.design.activation_dialog.is_activation_valid", return_value=False):
            dialog = ActivationDialog(self.root)
        self.root.update()
        self.assertTrue(dialog.dialog.winfo_exists())
        self.assert_system_dialog_frame(dialog.dialog)
        dialog.dialog.destroy()

    def test_moodle_form_constructs_without_authentication(self):
        from ui.design.moodle_login_dialog import MoodleLoginDialog
        with patch.object(MoodleLoginDialog, "check_saved_session", lambda _: None), patch.object(MoodleLoginDialog, "_fetch_institution_logo_bytes", lambda *_: b""):
            dialog = MoodleLoginDialog(self.root)
            self.root.update()
            self.assertTrue(dialog.username_entry.winfo_exists())
            self.assertTrue(dialog.password_entry.winfo_exists())
            self.assertTrue(dialog.password_entry.cget("show"))
            dialog.safe_destroy()


if __name__ == "__main__":
    unittest.main(verbosity=2)
