"""Compare real TKGPU and Tcl idle ordering, cancellation and large queues."""
import ctypes as c
import ctypes.util
import random
import sys

def main():
    ref = ctypes.util.find_library("tcl8.6")
    if not ref:
        raise RuntimeError("Tcl 8.6 unavailable")
    candidate, reference = c.CDLL(sys.argv[1]), c.CDLL(ref)
    reference.Tcl_FindExecutable.argtypes = [c.c_char_p]
    reference.Tcl_FindExecutable(b"python")
    callback_type = c.CFUNCTYPE(None, c.c_void_p)
    libs = [candidate, reference]
    for lib in libs:
        for name in ("Tcl_DoWhenIdle", "Tcl_CancelIdleCall"):
            fn = getattr(lib, name)
            fn.argtypes = [callback_type, c.c_void_p]
            fn.restype = None
        lib.Tcl_DoOneEvent.argtypes = [c.c_int]
        lib.Tcl_DoOneEvent.restype = c.c_int
    logs = [[], []]
    callbacks = []
    def factory(j, lib):
        @callback_type
        def callback(value):
            v = value or 0
            logs[j].append(v)
            if 1 <= v <= 12:
                lib.Tcl_DoWhenIdle(callbacks[j], v + 100)
            if v == 13:
                lib.Tcl_CancelIdleCall(callbacks[j], 42)
        return callback
    for j, lib in enumerate(libs):
        callbacks.append(factory(j, lib))
    rng = random.Random(7613)
    for i in range(20000):
        for log in logs:
            log.clear()
        for _ in range(rng.randrange(40)):
            v = rng.randrange(1, 65)
            name = "Tcl_DoWhenIdle" if rng.randrange(4) else "Tcl_CancelIdleCall"
            for j, lib in enumerate(libs):
                getattr(lib, name)(callbacks[j], v)
        for k in range(3):
            a = candidate.Tcl_DoOneEvent(34)
            b = reference.Tcl_DoOneEvent(34)
            if a != b or logs[0] != logs[1]:
                raise AssertionError((i, k, a, b, logs))
    print("60000 idle passes matched")
    for log in logs:
        log.clear()
    # Exercise queue growth beyond the historical 256 entries.
    for j, lib in enumerate(libs):
        for v in range(1000, 21000):
            lib.Tcl_DoWhenIdle(callbacks[j], v)
        for v in range(1000, 21000, 10):
            lib.Tcl_CancelIdleCall(callbacks[j], v)
    for k in range(3):
        a = candidate.Tcl_DoOneEvent(34)
        b = reference.Tcl_DoOneEvent(34)
        assert a == b and logs[0] == logs[1], (k, a, b)
    assert len(logs[0]) == 18000
    print("20000 queued / 18000 delivered large-queue callbacks matched")

if __name__ == "__main__":
    main()
