"""Compare limit configuration/checks; command-count enforcement has a native C test."""
import argparse
import ctypes as c
import random


class Time(c.Structure):
    _fields_ = [("sec", c.c_long), ("usec", c.c_long)]


def bind(path):
    lib = c.CDLL(path)
    pointer, integer = c.c_void_p, c.c_int
    signatures = {
        "CreateInterp": ([], pointer), "DeleteInterp": ([pointer], None),
        "LimitSetCommands": ([pointer, integer], None),
        "LimitGetCommands": ([pointer], integer),
        "LimitSetTime": ([pointer, c.POINTER(Time)], None),
        "LimitGetTime": ([pointer, c.POINTER(Time)], None),
        "LimitSetGranularity": ([pointer, integer, integer], None),
        "LimitGetGranularity": ([pointer, integer], integer),
        "LimitTypeSet": ([pointer, integer], None),
        "LimitTypeReset": ([pointer, integer], None),
        "LimitTypeEnabled": ([pointer, integer], integer),
        "LimitTypeExceeded": ([pointer, integer], integer),
        "LimitExceeded": ([pointer], integer),
        "LimitCheck": ([pointer], integer),
    }
    for name, (args, result) in signatures.items():
        fn = getattr(lib, "Tcl_" + name)
        fn.argtypes, fn.restype = args, result
        setattr(lib, name, fn)
    return lib


def snapshot(lib, interp):
    time = Time()
    lib.LimitGetTime(interp, c.byref(time))
    return (
        lib.LimitGetCommands(interp), time.sec, time.usec,
        *(lib.LimitGetGranularity(interp, kind) for kind in (1, 2)),
        *(lib.LimitTypeEnabled(interp, kind) for kind in (1, 2)),
        *(lib.LimitTypeExceeded(interp, kind) for kind in (1, 2)),
        lib.LimitExceeded(interp),
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("candidate")
    parser.add_argument("--reference", default="libtcl8.6.so")
    args = parser.parse_args()
    libs = [bind(args.reference), bind(args.candidate)]
    interps = [lib.CreateInterp() for lib in libs]
    rng = random.Random(629)
    try:
        for step in range(20000):
            op, kind = rng.randrange(6), rng.choice((1, 2))
            value = rng.choice((-1, 1000000000))
            granularity = rng.randrange(1, 1001)
            time = Time(rng.choice((1, 2000000000)), rng.randrange(1000000))
            result = []
            for lib, interp in zip(libs, interps):
                if op == 0:
                    lib.LimitSetCommands(interp, value)
                elif op == 1:
                    lib.LimitSetTime(interp, c.byref(time))
                elif op == 2:
                    lib.LimitSetGranularity(interp, kind, granularity)
                elif op == 3:
                    lib.LimitTypeSet(interp, kind)
                elif op == 4:
                    lib.LimitTypeReset(interp, kind)
                else:
                    result.append(lib.LimitCheck(interp))
            assert len(result) != 2 or result[0] == result[1], (step, result)
            states = [snapshot(lib, interp) for lib, interp in zip(libs, interps)]
            assert states[0] == states[1], (step, op, states)
        print("20000 limit configuration/check sequences matched stock Tcl")
    finally:
        for lib, interp in zip(libs, interps):
            lib.DeleteInterp(interp)


if __name__ == "__main__":
    main()
