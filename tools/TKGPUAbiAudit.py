"""Linux ELF import/export audit. Symbol matches alone are not ABI compatibility."""
import argparse
from pathlib import Path
import subprocess
import sys

def symbols(path, mode):
    result = subprocess.run(
        ["nm", "-D", mode, str(path)], check=True, text=True,
        capture_output=True, timeout=30)
    names = set()
    for line in result.stdout.splitlines():
        fields = line.split()
        if fields:
            name = fields[-1].split("@", 1)[0]
            if name.startswith(("Tcl", "Tk")):
                names.add(name)
    return names

def main():
    parser = argparse.ArgumentParser()
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--extension", type=Path)
    source.add_argument("--manifest", type=Path)
    parser.add_argument("--library", type=Path, action="append", default=[])
    parser.add_argument("--require-all", action="store_true")
    args = parser.parse_args()
    required = (symbols(args.extension, "--undefined-only") if args.extension else
                {s.strip() for s in args.manifest.read_text().splitlines()
                 if s.strip() and not s.lstrip().startswith("#")})
    available = set()
    for lib in args.library:
        available.update(symbols(lib, "--defined-only"))
    missing = sorted(required - available)
    print(f"Name coverage: {len(required)-len(missing)}/{len(required)}")
    for name in missing:
        print("MISSING", name)
    print("This checks exported names only; layout, calling convention and behavior require separate tests.")
    return int(args.require_all and (not required or bool(missing)))

if __name__ == "__main__":
    sys.exit(main())
