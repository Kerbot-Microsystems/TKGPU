"""Count only callable Linux Tcl 8.6 slots, excluding reserved/unused entries."""
import argparse
import json
import pathlib
import re
import subprocess

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--include", required=True)
    p.add_argument("--slots", required=True)
    p.add_argument("--cc", default="cc")
    p.add_argument("--json-output", type=pathlib.Path)
    p.add_argument("--require-complete", action="store_true")
    args = p.parse_args()
    header = subprocess.check_output(
        [args.cc, "-E", "-P", "-x", "c", "-I", args.include, "-"],
        input="#include <tcl.h>\n", text=True,
    )
    match = re.search(r"typedef struct TclStubs\s*\{(.*?)\}\s*TclStubs\s*;", header, re.S)
    if not match:
        raise RuntimeError("reference TclStubs structure not found")
    public = set(re.findall(r"\(\s*\*\s*(tcl\w+)\s*\)\s*\(", match.group(1)))
    public.discard("tclUnusedStubEntry")
    manifest = pathlib.Path(args.slots).read_text(encoding="utf-8")
    chosen = ["tcl_" + n for n in re.findall(r"^\s*TKGPU_SLOT\((\w+)\)\s*$", manifest, re.M)]
    chosen += ["tcl_PkgRequireEx", "tclFreeObj"]
    if len(chosen) != len(set(chosen)):
        raise RuntimeError("duplicate preview slot")
    missing = set(chosen) - public
    if missing:
        raise RuntimeError("slots not in reference header: " + ", ".join(sorted(missing)))
    print(f"Reference preview manifest: {len(chosen)}/{len(public)} ({len(chosen)/len(public):.1%}); not a behavioral parity measure")
    remaining = sorted(public - set(chosen))
    if args.json_output:
        report = {
            "reference": "Tcl 8.6 public TclStubs",
            "callable_slot_count": len(public),
            "manifest_slot_count": len(chosen),
            "missing_count": len(remaining),
            "missing_slots": remaining,
            "manifest_slots": sorted(chosen),
            "complete_manifest": not remaining,
            "behavioral_parity_qualified": False,
        }
        args.json_output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if args.require_complete and remaining:
        raise SystemExit(f"Incomplete Tcl Stubs manifest: {len(remaining)} callable slots remain")

if __name__ == "__main__":
    main()
