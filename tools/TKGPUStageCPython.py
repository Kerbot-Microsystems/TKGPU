"""Stage experimental Linux ELF or Windows PE CPython candidates in new directories."""
from __future__ import annotations

import argparse
import hashlib
import json
import importlib.util
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

from TKGPUAbiAudit import symbols


def run(*args: str) -> str:
    return subprocess.run(args, check=True, capture_output=True, text=True, timeout=60).stdout


def stage_windows(extension: Path, library: Path, output: Path) -> None:
    # pefile is a development-only dependency; runtime has no dependency on it.
    import pefile
    ext = pefile.PE(str(extension))
    dll = pefile.PE(str(library))
    if ext.FILE_HEADER.Machine != dll.FILE_HEADER.Machine:
        raise RuntimeError("PE architecture mismatch")
    if ext.OPTIONAL_HEADER.Magic != dll.OPTIONAL_HEADER.Magic:
        raise RuntimeError("PE bitness mismatch")
    if ext.OPTIONAL_HEADER.DATA_DIRECTORY[11].VirtualAddress:
        raise RuntimeError("Bound PE imports are unsupported")
    if ext.OPTIONAL_HEADER.DATA_DIRECTORY[13].VirtualAddress:
        raise RuntimeError("Delay-loaded PE imports are unsupported")
    imports = getattr(ext, "DIRECTORY_ENTRY_IMPORT", [])
    target = [entry for entry in imports if re.fullmatch(rb"(?:tcl|tk)86t?\.dll", entry.dll.lower())]
    if len(target) != 2 or {entry.dll.lower()[:2] for entry in target} != {b"tc", b"tk"}:
        raise RuntimeError("Expected one Tcl and one Tk 8.6 DLL import; historical 8.5/8.4 hosts need a separately validated ABI adapter")
    if any(entry not in target and entry.dll.lower().startswith((b"tcl", b"tk", b"libtcl", b"libtk")) for entry in imports):
        raise RuntimeError("Unexpected additional Tcl/Tk import")
    required = {imp.name for entry in target for imp in entry.imports}
    if None in required or not required:
        raise RuntimeError("Ordinal-only or empty Tcl/Tk import set")
    available = {sym.name for sym in dll.DIRECTORY_ENTRY_EXPORT.symbols if sym.name and not sym.forwarder}
    missing = required - available
    if missing:
        raise RuntimeError("Windows ABI exports missing: " + ", ".join(sorted(x.decode("ascii") for x in missing)))
    name = b"tkgp.dll"
    data = bytearray(extension.read_bytes())
    for entry in target:
        offset = ext.get_offset_from_rva(entry.struct.Name)
        original = entry.dll + b"\0"
        if data[offset:offset+len(original)] != original or len(name)+1 > len(original):
            raise RuntimeError("Unsafe PE import-name rewrite")
        data[offset:offset+len(original)] = name.ljust(len(original), b"\0")
        # Clear binding timestamp; imports must resolve against the new DLL.
        timestamp = entry.struct.get_field_absolute_offset("TimeDateStamp")
        data[timestamp:timestamp+4] = b"\0"*4
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="tkgpu-stage-", dir=output.parent) as tmp:
        candidate = Path(tmp) / "candidate"
        candidate.mkdir()
        staged = candidate / extension.name
        staged.write_bytes(data)
        shutil.copy2(library, candidate / name.decode("ascii"))
        check = pefile.PE(str(staged))
        names = [entry.dll.lower() for entry in check.DIRECTORY_ENTRY_IMPORT]
        if names.count(name) != 2 or any(x.startswith((b"tcl", b"tk8", b"tk9", b"libtcl", b"libtk")) for x in names):
            raise RuntimeError("Unexpected PE imports after staging")
        check.close()
        candidate.rename(output)
    ext.close()
    dll.close()
    print(f"Staged Windows candidate: {output}; {len(required)} imported names matched (not ABI certification)")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--library", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--extension", type=Path, help="Explicit reference _tkinter binary for cross-staging")
    parser.add_argument("--target", choices=("windows", "linux"), help="Artifact platform (defaults to host)")
    parser.add_argument("--windows-profile", choices=("modern", "98", "95", "xp"), default="modern")
    args = parser.parse_args()
    target = args.target or ("windows" if sys.platform == "win32" else
                             "linux" if sys.platform.startswith("linux") else None)
    if not target:
        parser.error("only Linux ELF and Windows PE are supported")
    if args.windows_profile != "modern" and (target != "windows" or not args.extension):
        parser.error("legacy staging requires --target windows and an explicit reference --extension")
    if args.extension:
        extension = args.extension.resolve(strict=True)
    else:
        if (target == "windows") != (sys.platform == "win32") or (target == "linux" and not sys.platform.startswith("linux")):
            parser.error("cross-staging requires --extension")
        spec = importlib.util.find_spec("_tkinter")
        if not spec or not spec.origin:
            parser.error("this CPython does not provide a reference _tkinter extension")
        extension = Path(spec.origin).resolve()
    library = args.library.resolve(strict=True)
    output = args.output.resolve()
    if target == "windows":
        if args.windows_profile != "modern":
            from platforms.windows_legacy import audit_library
            audit_library(library, args.windows_profile)
        # A loaded PE cannot be overwritten. Publish immutable side-by-side
        # candidates, then atomically select the completed pair for the next run.
        source_hash = hashlib.sha256(extension.read_bytes() + library.read_bytes() +
                                     args.windows_profile.encode("ascii")).hexdigest()[:16]
        staged = output.with_name(output.name + "-" + source_hash)
        if not staged.exists():
            stage_windows(extension, library, staged)
        elif (staged / "tkgp.dll").read_bytes() != library.read_bytes():
            raise RuntimeError("Existing versioned runtime does not match its build")
        pointer = output.with_suffix(".current.json")
        temporary = pointer.with_suffix(".tmp")
        temporary.write_text(json.dumps({"directory": staged.name, "windows_profile": args.windows_profile}) + "\n", encoding="utf-8")
        temporary.replace(pointer)
        print(f"Selected Windows candidate: {staged}")
        return 0
    if output.exists():
        parser.error("output must be a new directory; remove an old candidate explicitly")
    if not shutil.which("patchelf") or not shutil.which("nm"):
        parser.error("patchelf and nm are required")
    required = symbols(extension, "--undefined-only")
    available = symbols(library, "--defined-only")
    missing = sorted(required - available)
    if not required or missing:
        parser.error("ABI name audit failed: " + ", ".join(missing))
    needed = run("patchelf", "--print-needed", str(extension)).splitlines()
    tcl = [s for s in needed if re.fullmatch(r"libtcl8\.6\.so(?:\.\d+)*", s)]
    tk = [s for s in needed if re.fullmatch(r"libtk8\.6\.so(?:\.\d+)*", s)]
    if len(tcl) != 1 or len(tk) != 1:
        parser.error("expected exactly one Tcl 8.6 and one Tk 8.6 ELF dependency")
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="tkgpu-stage-", dir=output.parent) as tmp:
        candidate = Path(tmp) / "candidate"
        candidate.mkdir()
        staged_extension = candidate / extension.name
        staged_library = candidate / "libTKGPUCompat.so"
        shutil.copy2(extension, staged_extension)
        shutil.copy2(library, staged_library)
        for dependency in tcl + tk:
            run("patchelf", "--remove-needed", dependency, str(staged_extension))
        run("patchelf", "--add-needed", staged_library.name, str(staged_extension))
        run("patchelf", "--set-rpath", "$ORIGIN", str(staged_extension))
        after = run("patchelf", "--print-needed", str(staged_extension)).splitlines()
        if after.count(staged_library.name) != 1 or any(s.startswith(("libtcl", "libtk8", "libtk9")) for s in after):
            raise RuntimeError("unexpected ELF dependencies after staging")
        candidate.rename(output)
    print(f"Staged experimental candidate: {output}")
    print(f"Name coverage: {len(required)}/{len(required)}; behavior NOT certified.")
    print("Run the isolated compatibility probe before attempting application launch.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
