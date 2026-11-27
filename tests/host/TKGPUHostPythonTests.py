"""Run against an independently built TKGPU library; no stock Tk imports."""
import pathlib
import sys
import threading

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "bindings" / "python"))
from tkgpu import ABI, Host, TKGPUError
from tkgpu.dispatch import Mailbox

# A timed-out producer must not permanently consume mailbox capacity.
mailbox = Mailbox(capacity=2)
discarded = mailbox.submit(("discard",))
kept = mailbox.submit(("first",))
assert discarded.cancel()
last = mailbox.submit(("second",))
assert mailbox.drain(lambda value: value, 2) == 2
assert kept.result() == "first" and last.result() == "second"
mailbox.close()
assert ABI == 1
with Host(sys.argv[1]) as host:
    assert host.version
    host.call("image", "create", "photo", "testphoto", "-width", 2, "-height", 1)
    pixels = bytes((1, 2, 3, 0, 4, 5, 6, 255))
    host.photo_write("testphoto", 2, 1, pixels)
    assert host.photo_read("testphoto", 2, 1) == pixels
    try:
        host.photo_write("testphoto", 2, 1, pixels[:-1])
    except ValueError:
        pass
    else:
        raise AssertionError("short image buffer accepted")
    assert host.getint("0x10") == 16
    assert host.getdouble("2.5") == 2.5
    assert host.getboolean("off") is False
    assert host.call_typed("integer", "set", "count", "42") == 42
    assert host.call("set", "count") == "42"
    assert host.call_typed("list", "list", "a", "b c") == ("a", "b c")
    assert host.call("set", "tuple", ("a", "b c")) == "a {b c}"
    assert host._dll.TKGPU_HostIsOwner(host._handle) == 1
    assert host.call("set", "x", "literal $ [test]") == "literal $ [test]"
    assert host.eval("set x") == "literal $ [test]"
    assert host.splitlist(host.call("list", "one", "two words")) == ("one", "two words")
    host.createcommand("nested", lambda value: host.call("set", "inner", value))
    assert host.call("nested", "answer") == "answer"
    assert host.call("set", "inner") == "answer"
    host.createcommand("nested", lambda value: "new " + value)
    assert host.call("nested", "answer") == "new answer"
    host.deletecommand("nested")
    assert not host._commands
    errors = []
    def foreign():
        try:
            host.call("set", "x")
        except TKGPUError:
            errors.append("rejected")
    thread = threading.Thread(target=foreign)
    thread.start()
    thread.join()
    assert errors == ["rejected"]
    queued = []
    def producer():
        queued.append(host.submit_call("set", "queued", "from worker"))
    thread = threading.Thread(target=producer)
    thread.start()
    thread.join()
    assert not queued[0].done()
    assert host.drain_calls() == 1
    assert queued[0].result() == "from worker"
    cancelled = host.submit_call("set", "queued", "must not execute")
    assert cancelled.cancel()
    assert host.drain_calls() == 1
    assert host.call("set", "queued") == "from worker"
    failed = host.submit_call("error", "queued failure")
    host.drain_calls()
    assert isinstance(failed.exception(), TKGPUError)
    pending = host.submit_call("set", "queued", "after close")
assert pending.cancelled()
assert "_tkinter" not in sys.modules and "tkinter" not in sys.modules
print("Independent Python TKGPU host checks passed")
