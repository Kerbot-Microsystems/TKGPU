"""Exercise real Kervix controls, without starting the application or user profile."""
import os
from pathlib import Path
import sys
import tempfile


def main():
    bundle = Path(sys.argv[1]).resolve(strict=True)
    project = Path(__file__).resolve().parents[3]
    sys.path.insert(0, str(project / "src/kerbot_drone"))
    os.environ["KERBOT_TKGPU_HOST_BRIDGE"] = "1"
    os.environ["TKGPU_NATIVE_TEXT"] = "1"
    os.environ.setdefault("TKGPU_GPU_RASTER", "0")
    with tempfile.TemporaryDirectory(prefix="tkgpu-kervix-") as home:
        for name in ("HOME", "USERPROFILE", "APPDATA", "LOCALAPPDATA", "XDG_CONFIG_HOME"):
            os.environ[name] = home
        from lowlevel import tkgpu_runtime
        tkgpu_runtime._load_direct(bundle)
        import tkinter as tk
        from tkinter import ttk
        from ui.design.kervix_notebook_runtime import KervixNotebook
        from ui.design.kervix_controls.dropdown import TkKervixDropdown
        from ui.design.kervix_controls.spinbox import TkKervixSpinbox

        root = tk.Tk()
        errors = []
        root.report_callback_exception = lambda typ, value, tb: errors.append(repr(value))
        try:
            root.geometry("720x480")
            book = KervixNotebook(root)
            book.pack(fill="both", expand=True)
            first, second = ttk.Frame(book), ttk.Frame(book)
            book.add(first, text="Controls", padding=(4, 6), sticky="nsew")
            book.add(second, text="Second")
            root.update()
            events = []
            book.bind("<<NotebookTabChanged>>", lambda event: events.append(book.select()))
            for _ in range(20):
                book.select(second)
                root.update()
                book.select(first)
                root.update()
            assert len(events) == 40, events
            assert book.select() == str(first)
            chosen = []
            dropdown = TkKervixDropdown(first, values=["item %d" % n for n in range(10)],
                                        command=chosen.append)
            dropdown.pack(fill="x")
            spin = TkKervixSpinbox(first, min_val=0, max_val=10, step=0.5)
            spin.pack()
            root.update()
            dropdown.set("item 1")
            assert dropdown.get() == "item 1"
            dropdown._show_menu()
            root.update()
            assert dropdown._popup is not None, "Kervix popup did not open"
            assert len(dropdown._rows) == 10
            dropdown._select("item 7")
            root.update()
            assert dropdown.get() == "item 7" and chosen == ["item 7"]
            assert dropdown._popup is None
            dropdown.set_rtl(True)
            dropdown.set_rtl(False)
            spin.set(2)
            spin._change(0.5)
            assert spin.get() == "2.5"
            spin._change(99)
            assert spin.get() == "10"
            editor = tk.Text(second, undo=True)
            editor.pack()
            editor.insert("1.0", "one")
            editor.edit_separator()
            editor.insert("end", "two")
            editor.edit_undo()
            assert editor.get("1.0", "end-1c") == "one"
            editor.edit_redo()
            assert editor.get("1.0", "end-1c") == "onetwo"
            root.update()
            assert not errors, errors
            tkgpu_runtime._verify_mappings(tkgpu_runtime._runtime_library(bundle))
        finally:
            root.destroy()
    print("TKGPU_KERVIX_OK")


if __name__ == "__main__":
    main()
