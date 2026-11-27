"""Experimental Win95/98/XP PE acceptance preflight (not ABI certification)."""
from pathlib import Path
import sys


def audit_library(path, profile):
    if profile not in ("95", "98", "xp"):
        raise ValueError("unknown legacy Windows profile")
    # Reuse the existing platform-specific import gates.
    base = Path(__file__).resolve().parents[3] / "scripts" / "release" / "windows" / "x86"
    audit_dir = base if profile == "xp" else base / "win98l"
    sys.path.insert(0, str(audit_dir))
    try:
        if profile == "xp":
            from xp_audit import audit_binary
        else:
            from win98_audit import audit_binary
        audit_binary(path)
    finally:
        sys.path.remove(str(audit_dir))
    if profile == "95":
        import pefile
        with pefile.PE(str(path)) as image:
            h = image.OPTIONAL_HEADER
            if (h.MajorOperatingSystemVersion, h.MinorOperatingSystemVersion) > (4, 0):
                raise RuntimeError("Windows 95 requires PE OS version no newer than 4.0")
    # Win98 checks are only a subset of Win95 acceptance; runtime tests remain mandatory.
