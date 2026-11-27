"""Counted list parsing and serialization, including NUL and long elements."""
import argparse
import ctypes as c
import itertools
import random


def bind(path):
    lib = c.CDLL(path)
    pointer, integer, string = c.c_void_p, c.c_int, c.c_char_p
    for name, args, result in [
        ("FindExecutable", [string], None), ("CreateInterp", [], pointer),
        ("DeleteInterp", [pointer], None),
        ("NewStringObj", [string, integer], pointer),
        ("NewListObj", [integer, c.POINTER(pointer)], pointer),
        ("ListObjGetElements", [pointer, pointer, c.POINTER(integer), c.POINTER(pointer)], integer),
        ("GetStringFromObj", [pointer, c.POINTER(integer)], pointer),
    ]:
        fn = getattr(lib, "Tcl_" + name)
        fn.argtypes, fn.restype = args, result
        setattr(lib, name, fn)
    lib.TclFreeObj.argtypes, lib.TclFreeObj.restype = [pointer], None
    lib.FindExecutable(b"list-differential")
    return lib


def hold(obj):
    assert obj
    c.c_int.from_address(obj).value += 1
    return obj


def drop(lib, obj):
    refs = c.c_int.from_address(obj)
    refs.value -= 1
    if refs.value == 0:
        lib.TclFreeObj(obj)


def raw(lib, obj):
    length = c.c_int()
    value = lib.GetStringFromObj(obj, c.byref(length))
    return c.string_at(value, length.value)


def elements(lib, obj):
    count, values = c.c_int(), c.c_void_p()
    status = lib.ListObjGetElements(None, obj, c.byref(count), c.byref(values))
    if status:
        return status, None
    if not count.value:
        return status, []
    return status, [raw(lib, item) for item in (c.c_void_p * count.value).from_address(values.value)]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("candidate")
    p.add_argument("--reference", default="libtcl8.6.so")
    args = p.parse_args()
    libs = [bind(args.reference), bind(args.candidate)]
    interps = [lib.CreateInterp() for lib in libs]

    def parse(text):
        results = []
        for lib in libs:
            obj = hold(lib.NewStringObj(text, len(text)))
            results.append(elements(lib, obj))
            drop(lib, obj)
        assert results[0] == results[1], (text[:100], results)

    try:
        for text in (b"{a\x00b}", b'"a\x00b"', b"\\\x00x", b"\\0", b"{a\\\nb}",
                     b"a\\\nb", b"{" + b"x" * 20000 + b"}"):
            parse(text)
        for length in range(5):
            for chars in itertools.product(b'{}"\\a \n\x00', repeat=length):
                parse(bytes(chars))
        rng = random.Random(629)
        alphabet = '{}"\\a19xuU \n\t\r\x0b\x0c\x00Ж🚀'
        for _ in range(50000):
            parse("".join(rng.choice(alphabet) for _ in range(rng.randrange(100))).encode())
        for _ in range(10000):
            words = ["".join(rng.choice(alphabet) for _ in range(rng.randrange(50))).encode()
                     for _ in range(rng.randrange(8))]
            results = []
            for lib in libs:
                objects = [hold(lib.NewStringObj(word, len(word))) for word in words]
                vector = (c.c_void_p * len(objects))(*objects)
                obj = hold(lib.NewListObj(len(objects), vector))
                serialized = raw(lib, obj)
                assert elements(lib, obj) == (0, words)
                results.append(serialized)
                drop(lib, obj)
                for item in objects:
                    drop(lib, item)
            assert results[0] == results[1], (words, results)
        print("50000 counted list parses and 10000 serialized lists matched Tcl, plus short exhaustive/NUL/large cases")
    finally:
        for lib, interp in zip(libs, interps):
            lib.DeleteInterp(interp)


if __name__ == "__main__":
    main()
