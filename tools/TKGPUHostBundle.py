"""Create and verify a development-only direct host bundle with explicit inventory."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

MODULES = ("__init__.py", "native.py", "host.py", "dispatch.py", "images.py", "tkinter_adapter.py")
LIBRARIES = ("tkgp.dll", "libTKGPUCompat.so", "libTKGPUCompat.dylib")
MANIFEST = "DEVELOPMENT_MANIFEST.json"
NOTICES = ("THIRD_PARTY_NOTICES.md", "third_party/unicode/LICENSE.txt",
           "third_party/unicode/README.md", "third_party/zlib/LICENSE",
           "third_party/zlib/TKGPU-NOTICE.txt")


def expected(library):
    if library not in LIBRARIES:
        raise ValueError("Unsupported bundle library name")
    return {library, "LICENSE.repository.txt", "DEVELOPMENT_AND_DISTRIBUTION.md",
            "include/TKGPUHost.H"} | set(NOTICES) | {"python/tkgpu/" + name for name in MODULES}


def inventory(root):
    paths = set()
    for path in root.rglob("*"):
        if path.is_symlink():
            raise ValueError("Symlink in development bundle: " + str(path))
        if path.is_file():
            paths.add(path.relative_to(root).as_posix())
    return paths


def sha(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def verify(root):
    manifest = json.loads((root / MANIFEST).read_text(encoding="utf-8"))
    wanted = expected(manifest["library"])
    if manifest["status"] != "development-only" or manifest["release_authorized"] is not False:
        raise ValueError("Unexpected development manifest status")
    if inventory(root) != wanted | {MANIFEST} or set(manifest["sha256"]) != wanted:
        raise ValueError("Unexpected or missing development bundle files")
    for name in sorted(wanted):
        if sha(root / name) != manifest["sha256"][name]:
            raise ValueError("Bundle file mismatch: " + name)
    return manifest


def build(source, library, root, name):
    wanted = expected(name)
    if root.is_symlink():
        raise ValueError("Bundle directory may not be a symlink")
    root.mkdir(parents=True, exist_ok=True)
    if inventory(root) - (wanted | {MANIFEST}):
        raise ValueError("Bundle contains extra files; use a fresh output directory")
    mapping = {name: library, "LICENSE.repository.txt": source / "LICENSE",
               "DEVELOPMENT_AND_DISTRIBUTION.md": source / "docs/legal/DEVELOPMENT_AND_DISTRIBUTION.md",
               "include/TKGPUHost.H": source / "include/TKGPUHost.H"}
    mapping.update({"python/tkgpu/" + module: source / "bindings/python/tkgpu" / module for module in MODULES})
    mapping.update({notice: source / notice for notice in NOTICES})
    for key, origin in mapping.items():
        if not origin.is_file():
            raise ValueError("Missing bundle input: " + str(origin))
    # A failed copy must not leave a valid manifest for a partial bundle.
    (root / MANIFEST).unlink(missing_ok=True)
    for key, origin in mapping.items():
        dest = root / key
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(origin, dest)
    manifest = {"schema": 1, "status": "development-only", "release_authorized": False,
                "library": name, "sha256": {key: sha(root / key) for key in sorted(wanted)},
                "note": "File inventory only; dependency, provenance and rights review required."}
    (root / MANIFEST).write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    verify(root)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--bundle", type=Path, required=True)
    p.add_argument("--verify", action="store_true")
    p.add_argument("--source", type=Path)
    p.add_argument("--library", type=Path)
    p.add_argument("--name", choices=LIBRARIES)
    args = p.parse_args()
    if args.verify:
        verify(args.bundle)
    else:
        if not all((args.source, args.library, args.name)):
            p.error("build requires --source, --library and --name")
        build(args.source, args.library, args.bundle, args.name)


if __name__ == "__main__":
    main()
