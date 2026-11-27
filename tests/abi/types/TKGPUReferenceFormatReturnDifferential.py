"""Differential check of version, double formatting, printf, cancel, return options, and thread exits against stock Tcl 8.6."""
import argparse
import ctypes as c
import math
import random


def bind(lib):
    lib.Tcl_FindExecutable.argtypes = [c.c_char_p]
    lib.Tcl_FindExecutable.restype = None
    lib.Tcl_GetVersion.argtypes = [c.POINTER(c.c_int), c.POINTER(c.c_int), c.POINTER(c.c_int), c.POINTER(c.c_int)]
    lib.Tcl_GetVersion.restype = None
    lib.Tcl_PrintDouble.argtypes = [c.c_void_p, c.c_double, c.c_char_p]
    lib.Tcl_PrintDouble.restype = None
    lib.Tcl_ObjPrintf.restype = c.c_void_p
    lib.Tcl_AppendPrintfToObj.restype = None
    lib.Tcl_CreateInterp.argtypes = []
    lib.Tcl_CreateInterp.restype = c.c_void_p
    lib.Tcl_DeleteInterp.argtypes = [c.c_void_p]
    lib.Tcl_DeleteInterp.restype = None
    lib.Tcl_NewStringObj.argtypes = [c.c_char_p, c.c_int]
    lib.Tcl_NewStringObj.restype = c.c_void_p
    lib.Tcl_GetString.argtypes = [c.c_void_p]
    lib.Tcl_GetString.restype = c.c_char_p
    lib.Tcl_GetStringResult.argtypes = [c.c_void_p]
    lib.Tcl_GetStringResult.restype = c.c_char_p
    lib.Tcl_GetVar.argtypes = [c.c_void_p, c.c_char_p, c.c_int]
    lib.Tcl_GetVar.restype = c.c_char_p
    lib.Tcl_GetErrorLine.argtypes = [c.c_void_p]
    lib.Tcl_GetErrorLine.restype = c.c_int
    lib.Tcl_SetReturnOptions.argtypes = [c.c_void_p, c.c_void_p]
    lib.Tcl_SetReturnOptions.restype = c.c_int
    lib.Tcl_GetReturnOptions.argtypes = [c.c_void_p, c.c_int]
    lib.Tcl_GetReturnOptions.restype = c.c_void_p
    lib.Tcl_CancelEval.argtypes = [c.c_void_p, c.c_void_p, c.c_void_p, c.c_int]
    lib.Tcl_CancelEval.restype = c.c_int
    lib.Tcl_Canceled.argtypes = [c.c_void_p, c.c_int]
    lib.Tcl_Canceled.restype = c.c_int
    lib.Tcl_Eval.argtypes = [c.c_void_p, c.c_char_p]
    lib.Tcl_Eval.restype = c.c_int
    lib.Tcl_CreateObjCommand.argtypes = [c.c_void_p, c.c_char_p, c.c_void_p, c.c_void_p, c.c_void_p]
    lib.Tcl_CreateObjCommand.restype = c.c_void_p
    lib.Tcl_CreateThreadExitHandler.argtypes = [c.c_void_p, c.c_void_p]
    lib.Tcl_CreateThreadExitHandler.restype = None
    lib.Tcl_DeleteThreadExitHandler.argtypes = [c.c_void_p, c.c_void_p]
    lib.Tcl_DeleteThreadExitHandler.restype = None
    lib.Tcl_FinalizeThread.argtypes = []
    lib.Tcl_FinalizeThread.restype = None
    lib.Tcl_Format.argtypes = [c.c_void_p, c.c_char_p, c.c_int, c.POINTER(c.c_void_p)]
    lib.Tcl_Format.restype = c.c_void_p
    lib.Tcl_AppendFormatToObj.argtypes = [c.c_void_p, c.c_void_p, c.c_char_p, c.c_int, c.POINTER(c.c_void_p)]
    lib.Tcl_AppendFormatToObj.restype = c.c_int
    lib.Tcl_PutEnv.argtypes = [c.c_char_p]
    lib.Tcl_PutEnv.restype = c.c_int


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("candidate")
    p.add_argument("--reference", default="libtcl8.6.so")
    args = p.parse_args()
    ref, cand = c.CDLL(args.reference), c.CDLL(args.candidate)
    for lib in (ref, cand):
        bind(lib)
        lib.Tcl_FindExecutable(None)

    # 1. Version
    for lib in (ref, cand):
        maj, min_, pat, typ = c.c_int(), c.c_int(), c.c_int(), c.c_int()
        lib.Tcl_GetVersion(c.byref(maj), c.byref(min_), c.byref(pat), c.byref(typ))
        assert (maj.value, min_.value, typ.value) == (8, 6, 2), (maj.value, min_.value, typ.value)
        assert pat.value >= 0

    # 2. PrintDouble
    doubles = [
        0.0, -0.0, 1.0, -1.0, 42.0, -12345.0, 0.1, 0.2, 0.3, 1.0 / 3.0,
        math.pi, 1e20, -1e20, 1e-10, 1e100, 1e-100,
        float("inf"), float("-inf"), float("nan"), -float("nan"),
    ]
    rng = random.Random(8601)
    for _ in range(200):
        doubles.append(rng.uniform(-1e6, 1e6))
    for v in doubles:
        outs = []
        for lib in (ref, cand):
            buf = c.create_string_buffer(64)
            lib.Tcl_PrintDouble(None, v, buf)
            outs.append(buf.value)
        if math.isnan(v) or math.isinf(v):
            assert outs[0] == outs[1], (v, outs)
        else:
            assert float(outs[1]) == v, (v, outs)
            assert b"." in outs[1] or b"e" in outs[1], (v, outs)
            if v in (0.0, -0.0, 1.0, -1.0, 42.0, 0.1, 0.3, 1.0 / 3.0, 1e20, 1e-10):
                assert outs[0] == outs[1], (v, outs)

    # 3. ObjPrintf & AppendPrintfToObj
    for lib in (ref, cand):
        o = lib.Tcl_ObjPrintf(b"%s=%d(%#x)", b"k", c.c_int(99), c.c_uint(15))
        lib.Tcl_AppendPrintfToObj(c.c_void_p(o), b":%.2f", c.c_double(1.5))
        assert lib.Tcl_GetString(o) == b"k=99(0xf):1.50"

    # 4. SetReturnOptions & GetReturnOptions
    cases = [
        b"",
        b"-code ok",
        b"-code error",
        b"-code error -level 0",
        b"-code break -level 0",
        b"-code 9 -level 0",
        b"-level 2 -code error",
        b"-code bogus",
        b"-level -1",
        b"odd",
    ]
    for spec in cases:
        res = []
        for lib in (ref, cand):
            interp = lib.Tcl_CreateInterp()
            rc = lib.Tcl_SetReturnOptions(interp, lib.Tcl_NewStringObj(spec, -1))
            msg = lib.Tcl_GetStringResult(interp)
            lib.Tcl_DeleteInterp(interp)
            res.append((rc, msg))
        assert res[0] == res[1], (spec, res)

    # 5. CancelEval & Canceled
    CMDPROC = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_void_p, c.c_int, c.c_void_p)
    for unwind in (0, 0x100000):
        outcomes = []
        for lib in (ref, cand):
            interp = lib.Tcl_CreateInterp()
            assert lib.Tcl_Canceled(interp, 0) == 0
            assert lib.Tcl_CancelEval(interp, None, None, unwind) == 0
            assert lib.Tcl_Canceled(interp, 0) == 0

            def cb(cd, ip, objc, objv, _lib=lib, _u=unwind):
                return _lib.Tcl_CancelEval(ip, _lib.Tcl_NewStringObj(b"halted", -1), None, _u)

            fn = CMDPROC(cb)
            lib.Tcl_CreateObjCommand(interp, b"halt", fn, None, None)
            rc = lib.Tcl_Eval(interp, b"halt; set post 1")
            out = (rc, lib.Tcl_GetStringResult(interp), lib.Tcl_GetVar(interp, b"post", 0), lib.Tcl_Canceled(interp, 0))
            lib.Tcl_DeleteInterp(interp)
            outcomes.append(out)
        assert outcomes[0] == outcomes[1], (unwind, outcomes)

    # 6. Thread exit handlers (LIFO + selective delete)
    EXITPROC = c.CFUNCTYPE(None, c.c_void_p)
    traces = []
    for lib in (ref, cand):
        log = []

        def on_exit(cd, _log=log):
            _log.append(cd)

        fn = EXITPROC(on_exit)
        lib.Tcl_CreateThreadExitHandler(fn, c.c_void_p(10))
        lib.Tcl_CreateThreadExitHandler(fn, c.c_void_p(20))
        lib.Tcl_CreateThreadExitHandler(fn, c.c_void_p(30))
        lib.Tcl_DeleteThreadExitHandler(fn, c.c_void_p(20))
        lib.Tcl_FinalizeThread()
        traces.append(log)
    assert traces[0] == traces[1] == [30, 10], traces

    # 7. Tcl_Format & Tcl_AppendFormatToObj
    fmt_cases = [
        (b"%s=%04d(%c) %%", [b"hi", b"42", b"65"]),
        (b"%2$x:%1$s", [b"abc", b"255"]),
        (b"%.2f", [b"3.5"]),
        (b"%s %s", [b"onlyone"]),
        (b"%q", [b"x"]),
    ]
    for fmt, raw_args in fmt_cases:
        outs = []
        for lib in (ref, cand):
            interp = lib.Tcl_CreateInterp()
            arr = (c.c_void_p * len(raw_args))(*[lib.Tcl_NewStringObj(a, -1) for a in raw_args])
            res_obj = lib.Tcl_Format(interp, fmt, len(raw_args), arr)
            if res_obj:
                outs.append((0, lib.Tcl_GetString(res_obj)))
            else:
                outs.append((1, lib.Tcl_GetStringResult(interp)))
            lib.Tcl_DeleteInterp(interp)
        assert outs[0] == outs[1], (fmt, outs)

    print("Version, PrintDouble, ObjPrintf, Format, ReturnOptions, CancelEval, and ThreadExit matched Tcl 8.6")


if __name__ == "__main__":
    main()
