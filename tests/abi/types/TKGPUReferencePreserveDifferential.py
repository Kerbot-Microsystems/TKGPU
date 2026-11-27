import ctypes as c
import ctypes.util
import random
import sys

ref = c.CDLL(ctypes.util.find_library("tcl8.6"))
ref.Tcl_FindExecutable.argtypes = [c.c_char_p]
ref.Tcl_FindExecutable(b"python")
can = c.CDLL(sys.argv[1])
F = c.CFUNCTYPE(None, c.c_void_p)
logs = [[], []]
callbacks = [F(lambda p: logs[0].append(p)), F(lambda p: logs[1].append(p))]
for lib, prefix in [(can, "candidate_"), (ref, "Tcl_")]:
    for name in ["Preserve", "Release", "EventuallyFree"]:
        fn = getattr(lib, prefix + name)
        fn.argtypes = [c.c_void_p] + ([F] if name == "EventuallyFree" else [])
        fn.restype = None
        setattr(lib, name, fn)
rng = random.Random(6713)
count = 0
for i in range(20000):
    n = rng.randrange(1, 9)
    keys = list(range(1, n + 1))
    refs = {k: 0 for k in keys}
    pending = set()
    logs[0].clear()
    logs[1].clear()
    for j in range(30):
        k = rng.choice(keys)
        action = rng.randrange(3)
        if action == 0 and k not in pending:
            for lib in [can, ref]:
                lib.Preserve(k)
            refs[k] += 1
            count += 1
        elif action == 1 and refs[k]:
            for lib in [can, ref]:
                lib.Release(k)
            refs[k] -= 1
            count += 1
        elif action == 2 and k not in pending:
            for lib, cb in zip([can, ref], callbacks):
                lib.EventuallyFree(k, cb)
            pending.add(k)
            count += 1
        assert logs[0] == logs[1], (i, j, logs)
    for k in keys:
        for _ in range(refs[k]):
            for lib in [can, ref]:
                lib.Release(k)
            count += 1
    assert logs[0] == logs[1]
print("matched", count, "operations in 20000 scenarios")
