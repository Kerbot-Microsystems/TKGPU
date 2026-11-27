"""Differential checks for the independent checksum facade (Python 3.9+)."""
import argparse
import ctypes as c
import ctypes.util
import random

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("candidate")
    parser.add_argument("--reference", default=ctypes.util.find_library("tcl8.6"))
    args = parser.parse_args()
    if not args.reference:
        parser.error("Tcl 8.6 runtime not found")
    candidate = c.CDLL(args.candidate)
    reference = c.CDLL(args.reference)
    rng = random.Random(1234)
    total = 0
    for name in ("CRC32", "Adler32"):
        f, g = [getattr(lib, "Tcl_Zlib" + name) for lib in (candidate, reference)]
        for fn in (f, g):
            fn.argtypes = [c.c_uint, c.c_void_p, c.c_int]
            fn.restype = c.c_uint
        for i in range(10000):
            n = rng.choice([0, 1, 2, 5551, 5552, 5553, 11104]) if i < 100 else rng.randrange(2048)
            data = c.create_string_buffer(rng.randbytes(n))
            seed = rng.getrandbits(32)
            k = rng.randrange(n + 1)
            for call in ((seed, data, n), (seed, None, 0), (seed, data, k),
                         (f(seed, data, k), c.byref(data, k), n - k)):
                x, y = f(*call), g(*call)
                if x != y:
                    raise AssertionError((name, i, n, call[0], x, y))
                total += 1
        print(name, "passed")
    print("comparisons", total)

if __name__ == "__main__":
    main()
