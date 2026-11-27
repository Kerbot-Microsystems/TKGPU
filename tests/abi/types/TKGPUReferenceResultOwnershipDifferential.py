"""Compare legacy result ownership/conversion/transfer against the Tcl C ABI."""
import argparse
import ctypes as c
import random


def bind(path):
    lib = c.CDLL(path)
    pointer, integer, string = c.c_void_p, c.c_int, c.c_char_p
    signatures = {
        "CreateInterp": ([], pointer), "DeleteInterp": ([pointer], None),
        "SetResult": ([pointer, pointer, pointer], None),
        "SetObjResult": ([pointer, pointer], None),
        "GetObjResult": ([pointer], pointer),
        "NewStringObj": ([string, integer], pointer),
        "GetStringFromObj": ([pointer, c.POINTER(integer)], pointer),
        "GetStringResult": ([pointer], string),
        "ResetResult": ([pointer], None), "FreeResult": ([pointer], None),
        "TransferResult": ([pointer, integer, pointer], None),
        "SetVar": ([pointer, string, string, integer], string),
        "GetVar": ([pointer, string, integer], string),
        "UnsetVar": ([pointer, string, integer], integer),
        "AppendResult": ([pointer], None),
    }
    for name, (args, result) in signatures.items():
        fn = getattr(lib, "Tcl_" + name)
        fn.argtypes, fn.restype = args, result
        setattr(lib, name, fn)
    return lib


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("candidate")
    p.add_argument("--reference", default="libtcl8.6.so")
    args = p.parse_args()
    libs = [bind(args.reference), bind(args.candidate)]
    interps = [[lib.CreateInterp(), lib.CreateInterp()] for lib in libs]
    releases = [0, 0]
    callback_type = c.CFUNCTYPE(None, c.c_void_p)

    def callback(index):
        def release(_):
            releases[index] += 1
        return callback_type(release)

    callbacks = [callback(0), callback(1)]
    # The Python buffers own memory; custom free procedures count ownership
    # releases without calling a foreign allocator on Python allocations.
    buffers = []
    rng = random.Random(629)
    try:
        for step in range(20000):
            op, target = rng.randrange(12), rng.randrange(2)
            text = (f"value {step} Ж".encode()) * rng.choice((1, 3, 400))
            if op in (0, 1, 2):
                buffer = c.create_string_buffer(text)
                buffers.append(buffer)
            results = []
            for n, (lib, pair) in enumerate(zip(libs, interps)):
                interp = pair[target]
                if op in (0, 1, 2):
                    dispose = (None if op == 0 else c.c_void_p(1) if op == 1
                               else c.cast(callbacks[n], c.c_void_p))
                    lib.SetResult(interp, c.cast(buffer, c.c_void_p), dispose)
                elif op == 3:
                    length = c.c_int()
                    obj = lib.GetObjResult(interp)
                    data = lib.GetStringFromObj(obj, c.byref(length))
                    results.append(c.string_at(data, length.value))
                elif op == 4:
                    lib.FreeResult(interp)
                    # FreeResult intentionally leaves a dangling legacy result
                    # for owned buffers. Reset before any subsequent query.
                    lib.ResetResult(interp)
                elif op == 5:
                    lib.ResetResult(interp)
                elif op == 6:
                    lib.TransferResult(interp, 0, pair[1 - target])
                elif op == 7:
                    binary = b"a\x00b" + text
                    lib.SetObjResult(interp, lib.NewStringObj(binary, len(binary)))
                elif op == 8:
                    results.append(lib.SetVar(interp, b"variable", text, 0))
                elif op == 9:
                    results.append(lib.UnsetVar(interp, b"variable", 0))
                elif op == 10:
                    results.append(lib.GetVar(interp, b"variable", 0))
                else:
                    lib.AppendResult(interp, c.c_char_p(text), c.c_void_p())
            assert len(results) != 2 or results[0] == results[1], (step, op, [((len(v), v[:40]) if isinstance(v, bytes) else v) for v in results])
            snapshots = [tuple(lib.GetStringResult(ip) for ip in pair)
                         for lib, pair in zip(libs, interps)]
            assert snapshots[0] == snapshots[1], (step, op, [[(len(v), v[:40]) for v in pair] for pair in snapshots])
            assert releases[0] == releases[1], (step, op, releases)
    finally:
        for lib, pair in zip(libs, interps):
            for interp in pair:
                lib.DeleteInterp(interp)
    assert releases[0] == releases[1], releases
    print("20000 result ownership/conversion/transfer sequences matched stock Tcl")


if __name__ == "__main__":
    main()
