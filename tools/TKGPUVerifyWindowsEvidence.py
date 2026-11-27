"""Check target-native smoke evidence; never issue a full platform qualification."""
import argparse
import hashlib
import json
from pathlib import Path
import re

TARGETS = {"95": (1, 4, 0), "98": (1, 4, 10), "xp": (2, 5, 1)}


def problems(report, profile, revision):
    issues = []
    if not isinstance(report, dict):
        return ["report is not an object"]
    if profile not in TARGETS:
        return ["unsupported historical profile"]
    if not isinstance(revision, str) or not re.fullmatch(r"[0-9a-f]{40}", revision):
        issues.append("expected a full source revision")
    if report.get("schema") != 1 or report.get("profile") != profile or report.get("revision") != revision:
        issues.append("schema, profile or revision mismatch")
    if tuple(report.get(k) for k in ("os_platform", "os_major", "os_minor")) != TARGETS[profile]:
        issues.append("wrong target OS")
    if report.get("pointer_bits") != 32:
        issues.append("not x86")
    if report.get("abi") != 1 or report.get("abi_revision") != 4:
        issues.append("wrong ABI")
    for key in ("target_matches", "interpreter", "objects", "events", "gui", "cleanup"):
        if type(report.get(key)) is not int or report[key] != 1:
            issues.append("missing or failed native check: " + key)
    if report.get("smoke_passed") is not True or report.get("qualified") is not False:
        issues.append("invalid smoke/qualification status")
    return issues


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--evidence", type=Path, required=True)
    p.add_argument("--profile", choices=tuple(TARGETS), required=True)
    p.add_argument("--revision", required=True)
    p.add_argument("--library", type=Path, required=True)
    p.add_argument("--probe", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    args = p.parse_args()
    summary = {"accepted_smoke": False, "qualified": False, "profile": args.profile,
               "revision": args.revision, "issues": []}
    # Do not leave a stale successful report after a failed rerun.
    args.output.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    try:
        report = json.loads(args.evidence.read_text(encoding="utf-8-sig"))
        summary["issues"] = problems(report, args.profile, args.revision)
        from platforms.windows_legacy import audit_library
        for key, path in (("library", args.library), ("probe", args.probe)):
            audit_library(path, args.profile)
            summary[key + "_sha256"] = hashlib.sha256(path.read_bytes()).hexdigest()
        summary["evidence_sha256"] = hashlib.sha256(args.evidence.read_bytes()).hexdigest()
    except (OSError, ValueError, RuntimeError, ImportError) as exc:
        summary["issues"].append(str(exc))
    summary["accepted_smoke"] = not summary["issues"]
    summary["note"] = "Static PE preflight and target-reported smoke only; VM identity, exact binary provenance, service pack, transitive imports, Python ABI and regression suite remain separate gates."
    args.output.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(summary, indent=2))
    return 0 if summary["accepted_smoke"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
