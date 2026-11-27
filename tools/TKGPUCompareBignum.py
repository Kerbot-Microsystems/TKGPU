#!/usr/bin/env python3
"""Compare preview Bignum imports against a test-only Tcl 8.6 reference."""
import ctypes as c
import random
import sys

class Mp(c.Structure):
    _fields_ = [("used", c.c_int), ("alloc", c.c_int), ("sign", c.c_int),
                ("dp", c.POINTER(c.c_uint32))]

def bind(path):
    lib = c.CDLL(path)
    specs = {"TclBN_mp_init": ([c.POINTER(Mp)], c.c_int),
             "TclBN_mp_clear": ([c.POINTER(Mp)], None),
             "TclBN_mp_read_radix": ([c.POINTER(Mp), c.c_char_p, c.c_int], c.c_int),
             "TclBN_mp_unsigned_bin_size": ([c.POINTER(Mp)], c.c_size_t),
             "TclBN_mp_to_unsigned_bin_n": ([c.POINTER(Mp), c.c_void_p, c.POINTER(c.c_ulong)], c.c_int),
             "Tcl_NewStringObj": ([c.c_char_p, c.c_int], c.c_void_p),
             "Tcl_GetBignumFromObj": ([c.c_void_p, c.c_void_p, c.POINTER(Mp)], c.c_int),
             "TclFreeObj": ([c.c_void_p], None)}
    for name, (args, result) in specs.items():
        f = getattr(lib, name); f.argtypes = args; f.restype = result
    return lib

def snapshot(lib, a):
    n = lib.TclBN_mp_unsigned_bin_size(c.byref(a)); b = (c.c_ubyte * max(n, 1))()
    length = c.c_ulong(n); rc = lib.TclBN_mp_to_unsigned_bin_n(c.byref(a), b, c.byref(length))
    return a.sign, tuple(a.dp[k] for k in range(a.used)), n, rc, length.value, bytes(b[:n])

def probe(lib, text, radix):
    a = Mp(); o = None
    if radix is None:
        o = lib.Tcl_NewStringObj(text, len(text)); assert o
        rc = lib.Tcl_GetBignumFromObj(None, o, c.byref(a))
    else:
        assert lib.TclBN_mp_init(c.byref(a)) == 0
        assert lib.TclBN_mp_read_radix(c.byref(a), b"99", 10) == 0
        rc = lib.TclBN_mp_read_radix(c.byref(a), text, radix)
    try:
        return rc, snapshot(lib, a) if radix is not None or rc == 0 else None
    finally:
        if a.dp: lib.TclBN_mp_clear(c.byref(a))
        if o: lib.TclFreeObj(o)

def main():
    if len(sys.argv) != 3: raise SystemExit("usage: TKGPUCompareBignum.py PREVIEW REFERENCE")
    p, r = map(bind, sys.argv[1:]); r.Tcl_FindExecutable.argtypes = [c.c_char_p]
    r.Tcl_FindExecutable(b"tkgpu-bignum-test"); rng = random.Random(0x544b4750)
    alphabet = b"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz+/"
    cases = [(x, 10) for x in (b"", b"-", b"+12", b"123x", b"123\nXYZ", b"123\r\n", b" 12", b"-0", b"-999999999999999999999")]
    cases += [(b"3", 1), (b"3", 65), (b"+", 64), (b"/", 64)]
    for _ in range(1000):
        base = rng.randint(2, 64); text = bytes(alphabet[rng.randrange(base)] for _ in range(rng.randint(1, 90)))
        cases.append(((b"-" if rng.randrange(2) else b"") + text, base))
    cases += [(x, None) for x in (b"123", b" +123 ", b"0xFF", b"-0b101", b"0o77", b"077", b"08", b"0d12", b"12.0", b"", b"-0", b"  -0010\t", b"0Xabc", b"0b", b"0_1", b"123\nXYZ", b"123\x00garbage")]
    for text, base in cases:
        a, b = probe(p, text, base), probe(r, text, base)
        if a != b: raise AssertionError((text, base, a, b))
    print(f"PASS {len(cases)} cases; mp_int={c.sizeof(Mp)}, dp offset={Mp.dp.offset}")

if __name__ == "__main__": main()
