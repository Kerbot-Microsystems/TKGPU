"""Fail-closed qualification for the independent TKGPU source-mode host."""
import argparse
import json
import os
import platform
import re
import struct
from pathlib import Path
import subprocess
import sys


def ctest_passed(returncode, stdout, stderr):
    # CTest can return zero even when tests use SKIP_RETURN_CODE.
    output = stdout + "\n" + stderr
    if returncode or re.search(r"\*\*\*\s*(?:Skipped|Not Run)|\(Skipped\)|No tests were found", output, re.I):
        return False
    summary = re.search(r"100% tests passed,\s*0 tests failed out of\s+([1-9][0-9]*)", output)
    return summary is not None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--bundle", type=Path, required=True)
    parser.add_argument("--config", default="Release")
    parser.add_argument("--require-pillow", action="store_true", help="Fail if optional image integration cannot be exercised")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    build, bundle = args.build.resolve(), args.bundle.resolve()
    env = os.environ.copy()
    env["KERBOT_TKGPU_HOST_BRIDGE"] = "1"
    env.pop("PYTHONPATH", None)
    env["PYTHONDONTWRITEBYTECODE"] = "1"
    env.pop("KERBOT_TKGPU_INDEPENDENT_ACTIVE", None)
    env.pop("TKGPU_LINEAR_WIDGET_LOOKUP", None)
    config = args.config or "Release"
    report = {"bridge": "direct", "checks": {}, "qualified": False,
              "platform": {"os": platform.system(), "release": platform.release(),
                           "machine": platform.machine(), "python": platform.python_version(),
                           "pointer_bits": struct.calcsize("P") * 8}, "config": config}
    # macOS can build a core library but is not yet a qualified GUI host.
    library_name = ("tkgp.dll" if os.name == "nt" else
                    "libTKGPUCompat.dylib" if sys.platform == "darwin" else "libTKGPUCompat.so")
    try:
        report["revision"] = subprocess.check_output(
            ["git", "-C", str(root), "rev-parse", "HEAD"], text=True, timeout=10
        ).strip()
    except (OSError, subprocess.SubprocessError):
        report["revision"] = None
    checks = [
        ("bundle_inventory", [sys.executable, "-B", str(root / "TKGPU/tools/TKGPUHostBundle.py"),
                              "--bundle", str(bundle), "--verify"]),
        ("native_abi", ["ctest", "--test-dir", str(build), "-C", config,
                        "-R", "^host_abi$", "--output-on-failure", "--no-tests=error"]),
        ("native_regression", ["ctest", "--test-dir", str(build), "-C", config,
                               "--output-on-failure", "--no-tests=error"]),
        ("python_host", [sys.executable, "-I", "-B", str(root / "TKGPU/tests/host/TKGPUHostPythonTests.py"),
                         str(bundle / library_name)]),
        ("tkinter_api", [sys.executable, "-I", "-B", str(root / "TKGPU/tests/host/TKGPUHostTkinterTests.py"),
                         str(bundle / library_name)]),
        ("gui_probe", [sys.executable, "-I", "-B", str(root / "src/kerbot_drone/lowlevel/tkgpu_runtime.py"),
                       "--probe", str(bundle)]),
        ("kervix_components", [sys.executable, "-I", "-B",
                               str(root / "TKGPU/tests/host/TKGPUKervixTests.py"), str(bundle)]),
        ("gui_probe_linear", [sys.executable, "-I", "-B", str(root / "src/kerbot_drone/lowlevel/tkgpu_runtime.py"),
                              "--probe", str(bundle)]),
    ]
    if args.require_pillow:
        checks.insert(0, ("pillow_available", [sys.executable, "-I", "-B", "-c",
                        "from PIL import Image, ImageTk"]))
    report_path = build / "tkgpu-host-qualification.json"
    log_dir = build / "tkgpu-qualification-logs"
    log_dir.mkdir(parents=True, exist_ok=True)
    # Invalidate any previous successful report before running new checks.
    report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    for name, cmd in checks:
        try:
            check_env = env.copy()
            if name == "gui_probe_linear":
                check_env["TKGPU_LINEAR_WIDGET_LOOKUP"] = "1"
            result = subprocess.run(cmd, env=check_env, capture_output=True, text=True, timeout=600 if name == "native_regression" else 120)
            ok = result.returncode == 0
            (log_dir / (name + ".log")).write_text(
                result.stdout + "\n--- stderr ---\n" + result.stderr, encoding="utf-8")
            if name in ("native_abi", "native_regression"):
                ok = ctest_passed(result.returncode, result.stdout, result.stderr)
            if name in ("gui_probe", "gui_probe_linear"):
                ok = ok and result.stdout.strip() == "TKGPU_PROBE_OK"
            if name == "kervix_components":
                ok = ok and "TKGPU_KERVIX_OK" in result.stdout.splitlines()
            report["checks"][name] = {"passed": ok, "returncode": result.returncode,
                                       "stdout": result.stdout[-6000:], "stderr": result.stderr[-6000:]}
        except (OSError, subprocess.TimeoutExpired) as exc:
            report["checks"][name] = {"passed": False, "error": str(exc)}
        report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
        # A corrupt/missing bundle blocks subsequent native loads.
        if name == "bundle_inventory" and not report["checks"][name]["passed"]:
            break
    report["qualified"] = bool(report["revision"]) and len(report["checks"]) == len(checks) and all(
        item["passed"] for item in report["checks"].values())
    report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps(report, indent=2))
    return 0 if report["qualified"] else 1


if __name__ == "__main__":
    sys.exit(main())
