"""Exercise the development inventory without loading any native library."""
import pathlib
import sys
import tempfile
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "tools"))
from TKGPUHostBundle import build, verify, MODULES

with tempfile.TemporaryDirectory() as directory:
    root = pathlib.Path(directory)
    source = root / "TKGPU"
    for name in MODULES:
        path = source / "bindings/python/tkgpu" / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("# test\n")
    for name in ("include/TKGPUHost.H", "docs/legal/DEVELOPMENT_AND_DISTRIBUTION.md"):
        path = source / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("test\n")
    (root / "LICENSE").write_text("test license\n")
    binary = root / "dummy.dll"
    binary.write_bytes(b"test binary")
    bundle = root / "bundle"
    build(source, binary, bundle, "tkgp.dll")
    assert verify(bundle)["release_authorized"] is False
    (bundle / "tkgp.dll").write_bytes(b"changed")
    try:
        verify(bundle)
    except ValueError:
        pass
    else:
        raise AssertionError("changed binary accepted")
    build(source, binary, bundle, "tkgp.dll")
    (bundle / "unexpected.dll").write_bytes(b"test")
    try:
        build(source, binary, bundle, "tkgp.dll")
    except ValueError:
        pass
    else:
        raise AssertionError("unexpected binary accepted")
print("Development bundle inventory checks passed")
