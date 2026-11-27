"""Compare old two-call FFI with ABI 1.2's single call on the SAME native core.
Headless microbenchmark. Does not measure GPU throughput or GUI frame latency."""
import argparse
import ctypes as C
import pathlib
import statistics
import sys
import time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "bindings" / "python"))
from tkgpu import Host


def run(library, iterations):
    with Host(library) as host:
        dll, handle = host._dll, host._handle
        args = (C.c_char_p * 3)(b"set", b"benchmark_value", b"literal")
        status = C.c_int()
        def old():
            if dll.TKGPU_HostCall(handle, 3, args):
                raise RuntimeError("old call failed")
            return dll.TKGPU_HostResult(handle)
        def new():
            result = dll.TKGPU_HostCallResult(handle, 3, args, C.byref(status))
            if status.value:
                raise RuntimeError("new call failed")
            return result
        assert old() == new() == b"literal"
        samples = [[], []]
        for k in range(8):
            for index in ((0, 1) if k % 2 == 0 else (1, 0)):
                fn = (old, new)[index]
                start = time.perf_counter()
                for _ in range(iterations):
                    result = fn()
                elapsed = time.perf_counter() - start
                assert result == b"literal"
                if k:
                    samples[index].append(elapsed * 1000)
        before, after = map(statistics.median, samples)
        print("scope: headless FFI set/get, same TKGPU native library")
        print("two round trips: %.3f ms" % before)
        print("one round trip: %.3f ms" % after)
        print("speedup: %.3fx" % (before / after))


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("library")
    p.add_argument("--iterations", type=int, default=10000)
    a = p.parse_args()
    if a.iterations < 1:
        p.error("iterations must be positive")
    run(a.library, a.iterations)
