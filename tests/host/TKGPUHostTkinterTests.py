"""Headless standard Tkinter API check with no original _tkinter binary."""
import importlib.util
import pathlib
import sys

if importlib.util.find_spec("tkinter") is None:
    sys.exit(77)
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "bindings" / "python"))
from tkgpu.tkinter_adapter import install

adapter = install(sys.argv[1])
import tkinter as tk
assert sys.modules["_tkinter"] is adapter
assert adapter.TKGPU_HOST_ABI == 1
root = tk.Tcl()
try:
    value = tk.StringVar(root, value="initial")
    assert value.get() == "initial"
    value.set("updated")
    assert root.getvar(str(value)) == "updated"
    called = []
    name = root.register(lambda text: called.append(text) or "ok")
    assert root.tk.call(name, "literal $ [test]") == "ok"
    assert called == ["literal $ [test]"]
    root.deletecommand(name)
    assert root.tk.splitlist(root.tk.call("list", "one", "two words")) == ("one", "two words")
    root.tk.wantobjects(True)
    assert root.tk.call("llength", "one two") == 2
    assert root.tk.call("expr", "2+3") == 5
    assert root.tk.call("expr", "2.5*2") == 5.0
    assert isinstance(root.tk.call("expr", "2.5*2"), float)
    assert root.tk.call("expr", "3/2+0.5") == 1.5
    assert root.tk.exprdouble("2.5*2") == 5.0
    assert root.tk.call("list", "one", "two words") == ("one", "two words")
    assert root.tk.call("set", "literal", "007") == "007"
    assert root.tk.call("incr", "counter") == 1
    assert root.tk.call("set", "counter") == 1
    assert root.tk.call("set", "counter", "001") == "001"
    assert root.tk.call("set", "counter") == "001"
    root.tk.wantobjects(False)
    assert root.tk.call("llength", "one two") == "2"
    assert root.tk.exprstring("2+3") == "5"
    assert root.tk.exprlong("2+3") == 5
    assert root.tk.exprdouble("2+3") == 5.0
    assert root.tk.exprboolean("2==2") == 1
    root.tk.wantobjects(True)
    if importlib.util.find_spec("PIL") is not None:
        import base64
        import io
        from PIL import Image
        from tkgpu.images import install_pillow
        install_pillow()
        from PIL import ImageTk
        source = Image.new("RGBA", (2, 1))
        source.putdata([(1, 2, 3, 0), (4, 5, 6, 255)])
        photo = ImageTk.PhotoImage(source, master=root)
        assert ImageTk.getimage(photo).tobytes() == source.tobytes()
        data = io.BytesIO()
        source.save(data, format="PNG")
        direct = tk.PhotoImage(master=root, data=base64.b64encode(data.getvalue()).decode("ascii"))
        assert root.tk.photo_read(str(direct), 2, 1) == source.tobytes()
        del direct, photo
    import threading
    import time
    errors, results = [], []
    def worker():
        try:
            root.tk.createcommand("thread_callback", lambda: threading.get_ident())
            results.append(root.tk.call("thread_callback"))
            root.tk.deletecommand("thread_callback")
            root.tk.eval("set thread_value ok")
        except BaseException as exc:
            errors.append(exc)
    producer = threading.Thread(target=worker)
    producer.start()
    deadline = time.monotonic() + 8
    while producer.is_alive() and time.monotonic() < deadline:
        root.update()
        time.sleep(0.001)
    producer.join(timeout=6)
    assert not producer.is_alive() and not errors, errors
    assert results == [str(threading.get_ident())]
    assert root.tk.call("set", "thread_value") == "ok"
    del value
finally:
    root.tk.close()
print("Independent Tkinter API adapter checks passed")
