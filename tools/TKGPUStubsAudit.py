"""Reference TclStubs inventory; does not advertise a stub table."""
import argparse
import json
from pathlib import Path
import re
import subprocess
from TKGPUAbiAudit import symbols


def declared_slots(preprocessed):
    match = re.search(r"typedef\s+struct\s+TclStubs\s*\{(.*?)\}\s*TclStubs\s*;", preprocessed, re.S)
    if not match:
        raise ValueError("TclStubs declaration not found")
    names = re.findall(r"\(\s*\*\s*(tcl[A-Z_][A-Za-z0-9_]*)\s*\)", match.group(1))
    if not names:
        raise ValueError("TclStubs contains no recognized slots")
    return sorted({name[0].upper() + name[1:] for name in names})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--include", type=Path, required=True)
    parser.add_argument("--library", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--cc", default="cc")
    args = parser.parse_args()
    header = subprocess.run([args.cc, "-E", "-P", "-x", "c", "-I", str(args.include), "-"],
                            input="#include <tcl.h>\n", text=True, capture_output=True,
                            check=True, timeout=30).stdout
    slots = declared_slots(header)
    exports = symbols(args.library, "--defined-only")
    present = sorted(set(slots) & exports)
    report = {"reference": "TclStubs", "declared_slot_count": len(slots),
              "export_name_count": len(present), "present_names": present,
              "missing_names": sorted(set(slots) - exports),
              "stub_table_published": False, "qualified": False,
              "note": "Name overlap is not slot layout, calling convention, or behavior."}
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Reference TclStubs names exported: {len(present)}/{len(slots)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
