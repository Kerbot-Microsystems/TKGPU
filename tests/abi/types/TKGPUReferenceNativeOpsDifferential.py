"""Compare native operations in two private temporary trees; no real user files."""
import ctypes as c
import ctypes.util
import os
import random
import stat
import sys
import tempfile

def main():
    p = c.c_void_p
    ref = ctypes.util.find_library("tcl8.6")
    if not ref:
        raise RuntimeError("Tcl 8.6 unavailable")
    reference = c.CDLL(ref, use_errno=True)
    candidate = c.CDLL(sys.argv[1], use_errno=True)
    reference.Tcl_FindExecutable.argtypes = [c.c_char_p]
    reference.Tcl_FindExecutable(b"python")

    class Times(c.Structure):
        _fields_ = [("actime", c.c_long), ("modtime", c.c_long)]

    libs = [candidate, reference]
    for lib, prefix in zip(libs, ["candidate_", "Tcl_"]):
        for name, args, result in [
            ("FSCreateDirectory", [p], c.c_int), ("FSDeleteFile", [p], c.c_int),
            ("FSRenameFile", [p, p], c.c_int), ("FSUtime", [p, c.POINTER(Times)], c.c_int),
            ("Chdir", [c.c_char_p], c.c_int), ("FSChdir", [p], c.c_int),
            ("FSGetCwd", [p], p),
        ]:
            fn = getattr(lib, prefix + name)
            fn.argtypes, fn.restype = args, result
            setattr(lib, name, fn)
    for name, args, result in [
        ("NewStringObj", [c.c_char_p, c.c_int], p),
        ("DbIncrRefCount", [p, c.c_char_p, c.c_int], None),
        ("DbDecrRefCount", [p, c.c_char_p, c.c_int], None),
        ("GetString", [p], c.c_char_p),
    ]:
        fn = getattr(reference, "Tcl_" + name)
        fn.argtypes, fn.restype = args, result
        setattr(reference, name, fn)

    def obj(value):
        o = reference.NewStringObj(value, len(value))
        reference.DbIncrRefCount(o, b"test", 1)
        return o

    def release(o):
        reference.DbDecrRefCount(o, b"test", 1)

    def call(lib, name, *args):
        c.set_errno(0)
        rc = getattr(lib, name)(*args)
        return rc, c.get_errno() if rc else 0

    def snapshot(root):
        result = []
        for name in sorted(os.listdir(root)):
            path = os.path.join(root, name)
            st = os.lstat(path)
            content = None
            if stat.S_ISLNK(st.st_mode):
                content = os.readlink(path)
            elif stat.S_ISREG(st.st_mode):
                with open(path, "rb") as f:
                    content = f.read()
            result.append((name, stat.S_IFMT(st.st_mode), stat.S_IMODE(st.st_mode), content))
        return result

    rng = random.Random(8341)
    total = 0
    with tempfile.TemporaryDirectory() as td:
        roots = [td + "/a", td + "/b"]
        for root in roots:
            os.mkdir(root)
            os.mkdir(root + "/dir")
            with open(root + "/file", "wb") as f:
                f.write(b"hello")
            os.symlink("file", root + "/link")
            os.symlink("missing", root + "/broken")
        paths = ["dir", "file", "link", "broken", "missing", "foo", "bar",
                 "dir/nested", "missing/child", "./foo", "foo/", "dir//"]
        for i in range(12000):
            name = rng.choice(["FSCreateDirectory", "FSDeleteFile", "FSRenameFile", "FSUtime"])
            src, dst = rng.choice(paths), rng.choice(paths)
            times = Times(1000000000+i, 1000000000+i)
            results = []
            for lib, root in zip(libs, roots):
                x = obj(os.fsencode(root + "/" + src))
                y = obj(os.fsencode(root + "/" + dst)) if name == "FSRenameFile" else None
                args = [x] + ([y] if y else [c.byref(times)] if name == "FSUtime" else [])
                results.append(call(lib, name, *args))
                release(x)
                if y:
                    release(y)
            assert results[0] == results[1], (i, name, src, dst, results)
            assert snapshot(roots[0]) == snapshot(roots[1]), (i, name, src, dst)
            if name == "FSUtime" and not results[0][0]:
                assert os.stat(roots[0]+"/"+src).st_mtime_ns == os.stat(roots[1]+"/"+src).st_mtime_ns
            total += 1
        old = os.getcwd()
        try:
            for i in range(3000):
                name = rng.choice(["Chdir", "FSChdir"])
                path = os.fsencode(rng.choice(["dir", "file", "missing", ".", "..", "foo", "link", ""]))
                results = []
                for lib, root in zip(libs, roots):
                    assert reference.Chdir(os.fsencode(root)) == 0
                    x = obj(path) if name == "FSChdir" else path
                    results.append(call(lib, name, x))
                    if name == "FSChdir":
                        release(x)
                    cwd = lib.FSGetCwd(None)
                    assert cwd and reference.GetString(cwd) == os.fsencode(os.getcwd())
                    release(cwd)
                    total += 1
                assert results[0] == results[1], (name, path, results)
                total += 1
        finally:
            assert reference.Chdir(os.fsencode(old)) == 0
    print(total, "native operation / cwd comparisons passed")

if __name__ == "__main__":
    main()
