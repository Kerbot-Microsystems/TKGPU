"""Qualification must not mistake CTest skips or missing tests for success."""
import pathlib
import sys
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "tools"))
from TKGPUQualifyHost import ctest_passed

summary = "100% tests passed, 0 tests failed out of 12"
assert ctest_passed(0, summary, "")
assert not ctest_passed(0, "1/12 Test #1: host_api ...***Skipped\n" + summary, "")
assert not ctest_passed(0, summary, "The following tests did not run: 1 - host_api (Skipped)")
assert not ctest_passed(0, "No tests were found!!!", "")
assert not ctest_passed(0, "100% tests passed, 0 tests failed out of 0", "")
assert not ctest_passed(1, summary, "")
assert not ctest_passed(0, "", "")
from TKGPUVerifyWindowsEvidence import problems
revision = "a" * 40
report = dict(schema=1, profile="98", revision=revision, os_platform=1, os_major=4,
              os_minor=10, pointer_bits=32, abi=1, abi_revision=4, target_matches=1,
              interpreter=1, objects=1, events=1, gui=1, cleanup=1, smoke_passed=True,
              qualified=False)
assert not problems(report, "98", revision)
for key, value in (("profile", "xp"), ("revision", "b" * 40), ("os_platform", 2),
                   ("os_major", 10), ("os_minor", 0), ("pointer_bits", 64), ("abi_revision", 3),
                   ("target_matches", 0), ("objects", 0), ("gui", 0), ("cleanup", 0),
                   ("smoke_passed", False), ("qualified", True), ("events", True)):
    assert problems(dict(report, **{key: value}), "98", revision), key
assert problems(report, "98", "unknown")
assert problems(None, "98", revision)
assert not problems(dict(report, profile="xp", os_platform=2, os_major=5, os_minor=1), "xp", revision)
assert not problems(dict(report, profile="95", os_minor=0), "95", revision)
print("Qualification evidence gate checks passed")
