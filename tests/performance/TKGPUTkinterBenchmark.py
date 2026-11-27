"""Compare command throughput with stock Tk, using the same Python and display.
Not a paint/FPS benchmark: async TKGPU may publish after update returns.
Every batch verifies results outside the timed section. No fallback permitted.
"""
from __future__ import annotations

import argparse
import importlib.util
import json
import os
from pathlib import Path
import statistics
import subprocess
import sys
import time


def load_candidate(path: Path):
    root = Path(__file__).resolve().parents[3]
    loader = root / "src/kerbot_drone/lowlevel/tkgpu_runtime.py"
    spec = importlib.util.spec_from_file_location("_tkgpu_benchmark_loader", loader)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    module._load(module._platforms.resolve_candidate(path).resolve())
    return module


def worker(candidate: Path | None):
    loader = load_candidate(candidate) if candidate else None
    import tkinter as tk
    from tkinter import ttk

    root = tk.Tk()
    root.geometry("640x480")
    widgets = []
    samples = {}
    try:
        start = time.perf_counter_ns()
        for n in range(200):
            label = ttk.Label(root, text="initial")
            label.place(x=(n % 10) * 62, y=(n // 10) * 23, width=60, height=22)
            widgets.append(label)
        samples["create_and_place_200_ms"] = (time.perf_counter_ns() - start) / 1e6
        root.update()
        def measure(name, operation, verify):
            times = []
            for iteration in range(8):
                start = time.perf_counter_ns()
                operation(iteration)
                ms = (time.perf_counter_ns() - start) / 1e6
                verify(iteration)
                root.update()
                if iteration:
                    times.append(ms)
            samples[name] = statistics.median(times)
        def modify(n):
            for label in widgets:
                label.configure(text=f"batch {n}")
        def check_modify(n):
            assert all(label.cget("text") == f"batch {n}" for label in widgets)
        measure("configure_200_ms", modify, check_modify)
        results = []
        def query(n):
            results.clear()
            for label in widgets:
                results.append(label.cget("text"))
                results.append(label.winfo_exists())
        def check_query(n):
            assert len(results) == 400
            assert all(results[k] == "batch 7" and results[k+1] == 1
                       for k in range(0, len(results), 2))
        measure("query_400_ms", query, check_query)
        start = time.perf_counter_ns()
        for label in widgets:
            label.destroy()
        samples["destroy_200_ms"] = (time.perf_counter_ns() - start) / 1e6
        assert not root.winfo_children()
        if loader:
            loader._verify_mappings(loader._runtime_library(loader._platforms.resolve_candidate(candidate).resolve()))
        return samples
    finally:
        root.destroy()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--candidate", type=Path)
    parser.add_argument("--worker", action="store_true")
    parser.add_argument("--direct", action="store_true", help="Use the independent host bundle")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.worker:
        os.environ["KERBOT_TKGPU_HOST_BRIDGE"] = "1" if args.direct else "0"
        print(json.dumps(worker(args.candidate)))
        return
    if not args.candidate:
        parser.error("--candidate is required for an actual comparison")
    def run(candidate):
        command = [sys.executable, "-I", str(Path(__file__).resolve()), "--worker"]
        if candidate:
            command += ["--candidate", str(candidate.resolve())]
            if args.direct:
                command.append("--direct")
        result = subprocess.run(command, capture_output=True, text=True, timeout=90, check=True)
        return json.loads(result.stdout)
    reference, candidate = run(None), run(args.candidate)
    report = {
        "scope": "Python command throughput; excludes completed-frame latency",
        "candidate_bridge": "direct" if args.direct else "patched-reference",
        "reference": reference,
        "tkgpu": candidate,
        "speedup_reference_over_tkgpu": {
            key: reference[key] / candidate[key] for key in reference
        },
    }
    output = json.dumps(report, indent=2)
    print(output)
    if args.output:
        args.output.write_text(output + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
