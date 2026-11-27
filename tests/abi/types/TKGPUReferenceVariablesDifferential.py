"""Variable C API values, append/list flags and successful-result preservation."""
import argparse
import ctypes as c
import random


def bind(path):
    lib = c.CDLL(path)
    pointer, integer, string = c.c_void_p, c.c_int, c.c_char_p
    for name, args, result in [
        ("CreateInterp", [], pointer), ("DeleteInterp", [pointer], None),
        ("NewStringObj", [string, integer], pointer),
        ("SetObjResult", [pointer, pointer], None), ("GetObjResult", [pointer], pointer),
        ("GetStringFromObj", [pointer, c.POINTER(integer)], pointer),
        ("SetVar2", [pointer, string, string, string, integer], string),
        ("SetVar2Ex", [pointer, string, string, pointer, integer], pointer),
        ("GetVar2Ex", [pointer, string, string, integer], pointer),
        ("UnsetVar2", [pointer, string, string, integer], integer),
    ]:
        fn = getattr(lib, "Tcl_" + name)
        fn.argtypes, fn.restype = args, result
        setattr(lib, name, fn)
    lib.TclFreeObj.argtypes, lib.TclFreeObj.restype = [pointer], None
    return lib


def raw(lib, obj):
    if not obj:
        return None
    length = c.c_int()
    value = lib.GetStringFromObj(obj, c.byref(length))
    return c.string_at(value, length.value)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("candidate")
    p.add_argument("--reference", default="libtcl8.6.so")
    args = p.parse_args()
    libs = [bind(args.reference), bind(args.candidate)]
    interps = [lib.CreateInterp() for lib in libs]
    rng = random.Random(629)
    keys = [(b"scalar", None), (b"::scalar", None), (b"array", None),
            (b"array", b"k"), (b"array", b"other key"), (b"second", None)]
    try:
        for step in range(20000):
            op, key = rng.randrange(4), rng.choice(keys)
            flags = rng.choice((0, 4, 12)) | rng.choice((0, 1, 2))
            text = "".join(rng.choice('a{}[]"\\ \t\nЖ🚀') for _ in range(rng.randrange(30))).encode()
            if step % 100 == 0:
                text *= 500
            binary = b"a\x00b" + text if rng.randrange(4) == 0 else text
            kept = b"kept\x00" + text
            results, states = [], []
            for lib, interp in zip(libs, interps):
                lib.SetObjResult(interp, lib.NewStringObj(kept, len(kept)))
                ok = False
                if op == 0:
                    result = lib.SetVar2(interp, *key, text, flags)
                    ok = result is not None
                elif op == 1:
                    value = lib.NewStringObj(binary, len(binary))
                    c.c_int.from_address(value).value += 1
                    result = lib.SetVar2Ex(interp, *key, value, flags)
                    ok = bool(result)
                    result = raw(lib, result)
                    refs = c.c_int.from_address(value)
                    refs.value -= 1
                    if refs.value == 0:
                        lib.TclFreeObj(value)
                elif op == 2:
                    result = raw(lib, lib.GetVar2Ex(interp, *key, flags & 3))
                    ok = result is not None
                else:
                    result = lib.UnsetVar2(interp, *key, flags & 3)
                    ok = result == 0
                results.append(result)
                if ok:
                    assert raw(lib, lib.GetObjResult(interp)) == kept, (step, op, "result changed", lib._name)
                states.append([raw(lib, lib.GetVar2Ex(interp, *name, 0)) for name in keys])
            assert results[0] == results[1], (step, op, flags, key, text[:80], results)
            assert states[0] == states[1], (step, op, flags, "variable state")
        print("20000 string/object variable, append/list, array and result-preservation sequences matched Tcl")
    finally:
        for lib, interp in zip(libs, interps):
            lib.DeleteInterp(interp)


if __name__ == "__main__":
    main()
