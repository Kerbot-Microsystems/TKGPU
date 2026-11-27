"""Completeness checks must never evaluate a script or recurse on its nesting."""
import argparse
import ctypes as c
import itertools
import random


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("candidate")
    p.add_argument("--reference", default="libtcl8.6.so")
    args = p.parse_args()
    libs = [c.CDLL(args.reference), c.CDLL(args.candidate)]
    for lib in libs:
        lib.Tcl_CommandComplete.argtypes = [c.c_char_p]
        lib.Tcl_CommandComplete.restype = c.c_int

    def check(text):
        result = [lib.Tcl_CommandComplete(text) for lib in libs]
        assert result[0] == result[1], (text, result)

    fixtures = [b"", b"\\", b"\\\n", b"# comment\\\n", b"{a}x{", b'"a"x[',
                b"$a(", b"$(", b"${a", b"[set a #x", b'$a(x(y)', b'${a\\}',
                b"set x a\"", b"\\a\"", b"{a}\\[ {", b"$a:(", b"$::a("]
    for text in fixtures:
        check(text)
    for length in range(5):
        for chars in itertools.product(b"{}[]$()\\\";#a \n", repeat=length):
            check(bytes(chars))
    rng = random.Random(629)
    alphabet = b"{}[]$()\\\";#ab_::19 \t\n\r\v\f" + "Ж🚀".encode()
    for _ in range(150000):
        check(bytes(rng.choice(alphabet) for _ in range(rng.randrange(100))))
    for text in (b"[" * 100, b"[" * 100 + b"]" * 100, b"{" * 100 + b"}" * 100):
        check(text)
    # Stock Tcl's parser can exhaust a Windows process stack at this depth.
    # Test the independent scanner's heap stack without invoking that parser.
    for depth in (1000, 10000):
        assert libs[1].Tcl_CommandComplete(b"[" * depth) == 0
        assert libs[1].Tcl_CommandComplete(b"[" * depth + b"]" * depth) == 1
        assert libs[1].Tcl_CommandComplete(b"{" * depth + b"}" * depth) == 1
    print("Exhaustive short inputs and 150000 random scripts matched Tcl; deep heap-stack checks passed")


if __name__ == "__main__":
    main()
