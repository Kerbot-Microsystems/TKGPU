"""Optional development-only comparison against a Tcl 8.6 reference shell."""
import argparse
import subprocess
import sys

CASES = (
    "set x 40; incr x 2",
    "set x 5; append x 6",
    "set x 7; set y [incr x 2]",
    "expr {2+3*4}",
    "expr {-7/3}",
    "expr {7%-3}",
    "info exists missing",
    "set x a; info exists x",
    "list alpha {two words} {}",
    "llength [list alpha {two words} {}]",
    "lindex {a b c} end",
    "lindex {a b c} -1",
    "lindex {a b c} 9",
    "llength {}",
    "set x 8; expr {$x*2}",
    "if {0} {error bad} else {set y no}",
    "set x 3; if {$x} {set y yes} else {set y no}",
    "catch {error failed} message; set message",
    "catch {expr {2+2}} message; set message",
    "eval {set a 2; incr a}",
    "set x 2147483647; incr x",
)

def run_reference(executable, script):
    # Fixed local fixtures; Tcl catches errors and exits explicitly.
    wrapper = "if {[catch {\n" + script + "\n} result]} {\n"
    wrapper += "puts stderr $result; exit 1\n} else {puts $result; exit 0}\n"
    return subprocess.run([executable], input=wrapper, text=True,
                          capture_output=True, timeout=10, check=False)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("shell")
    parser.add_argument("--tclsh", default="tclsh")
    args = parser.parse_args()
    version = subprocess.run([args.tclsh], input="puts [info patchlevel]\n",
                             text=True, capture_output=True, timeout=10, check=True)
    if not version.stdout.strip().startswith("8.6."):
        print("Tcl 8.6 required for reference comparison", file=sys.stderr)
        return 2
    failures = 0
    for script in CASES:
        reference = run_reference(args.tclsh, script)
        actual = subprocess.run([args.shell, "-c", script], text=True,
                                capture_output=True, timeout=10, check=False)
        match = reference.returncode == actual.returncode and reference.stdout == actual.stdout
        print(("PASS" if match else "FAIL"), repr(script))
        if not match:
            failures += 1
            print(" reference:", repr(reference.stdout), reference.returncode)
            print(" actual:", repr(actual.stdout), actual.returncode)
    print(f"{len(CASES)-failures}/{len(CASES)} matched; reference {version.stdout.strip()}")
    return int(bool(failures))

if __name__ == "__main__":
    sys.exit(main())
