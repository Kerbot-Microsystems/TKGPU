# TKGPU Windows compatibility contract

Requested Windows targets are release acceptance criteria, not claims of existing support.

| OS | Architecture target | Current evidence |
|---|---|---|
| Windows 98 / 98 SE | x86 | Not built or run; legacy thread/TLS adapter in source; requires Win9x-safe toolchain and CRT |
| Windows XP / XP SP3 | x86 | Not built or run; Legacy adapter implemented in source; compiler/CRT and runtime unverified |
| Windows Vista | x86, x64 | Native API generation targeted; no native test recorded |
| Windows 7 | x86, x64 | Not built or run |
| Windows 8 | x86, x64 | Not built or run |
| Windows 8.1 | x86, x64 | Not built or run |
| Windows 10 / 11 | x86, x64 | Keep modern support; no native test recorded for this revision |

## Architecture

Keep common ISO C code independent of OS versions. Keep platform-specific alternatives in separate directories. The legacy software framebuffer and GDI presentation remain independent of modern GPU APIs. Modern GPU paths must be optional and cannot introduce unconditional imports into the legacy image.

Clock: resolve GetTickCount64 at runtime on Windows and fall back to GetTickCount with a serialized wrap extension. The fallback must be sampled more often than once per 49.7 days. This removes one direct Vista-only import, not all of them.

A separate experimental build option `-DTKGPU_LEGACY_WINDOWS=ON` selects x86 CriticalSection/event conditions and TlsAlloc instead of SRWLOCK, FLS and InitOnceExecuteOnce in the notifier. It requires the host to destroy its interpreters and call TKGPU_FinalizeLegacyThread before thread exit. No callbacks run from DllMain. Without explicit cleanup thread exit is not supported; this is not yet a drop-in legacy ABI. The option sets target header versions but cannot make an incompatible compiler/CRT or PE subsystem compatible. Inspect the actual resulting image and transitive imports. The modern backend stays the default.

## Acceptance gates for every OS and architecture

1. Build the intended binary using a toolchain/CRT proven for that target; audit PE headers and every static/delay-load import against a baseline installation (including transitive DLLs).
2. Boot on a real installation or VM; load the library, run interpreter, object, memory and notifier tests, and start, resize, interact with and close the native GUI.
3. Verify the actual host Python and Tcl/Tk ABI version/digit layout for that target. A current CPython build is not a valid Win98/XP host test. Test the supported historical host separately.
4. Record OS edition/service pack, CPU architecture, compiler/CRT, commit, import report and test results. Distinguish build-only, runtime-tested and unsupported configurations.
5. Never merge or advertise a binary replacement until its own platform gates pass. A modern Windows CI runner does not prove older Windows compatibility.

No original Tcl/Tk runtime or third-party runtime is required by the independent design; a reference Tcl/Tk may be used in differential development tests only.

## Experimental Windows 95 profile and cross-staging

Select the standalone x86 legacy target at configure time with `-DTKGPU_LEGACY_WINDOWS=ON -DTKGPU_LEGACY_WINDOWS_TARGET=95` (or `98`, the default). The portable core shares the target definitions; MinGW receives explicit PE OS/subsystem versions. Neither the flags nor the reused Win98 audit establish Win95 API compatibility.

On a modern build host, prepare a matching candidate with:

```text
python TKGPU/tools/TKGPUStageCPython.py --target windows --windows-profile 98 --extension PATH_TO_REFERENCE_86_TKINTER --library PATH_TO_LEGACY_TKGPU --output PATH_TO_CANDIDATE
```

Use profile 95 for that separate target. The tool requires Tcl/Tk 8.6 imports and checks names before rewriting a private copy. The existing 98L Tcl/Tk 8.5 adapter is NOT accepted as 8.6. A historical host needs its own verified ABI, CRT, bootstrap and VM acceptance. Windows 95 is an additional experimental target, not a supported Kerbot release. The current application loader remains source-mode Windows/Linux only; no macOS or Win9x application activation is claimed.

For repeated build-host staging, configure `-DTKGPU_LEGACY_REFERENCE_EXTENSION=PATH_TO_REFERENCE_86_TKINTER` and build the existing `tkgpu_kerbot_candidate` target. It publishes a separate audited candidate without editing the target Python installation. This is not an automatic 98L application installation; that requires the historical ABI and bootstrap qualification described above.

## Separate XP target and Win9x import repairs

Configure the XP x86 preview using `-DTKGPU_LEGACY_WINDOWS=ON -DTKGPU_LEGACY_WINDOWS_TARGET=xp`. It uses XP 5.1 header and PE version metadata and the existing XP import audit instead of the Win98 gate. The stage command accepts `--windows-profile xp` with an explicit target-compatible reference extension. The common legacy Win32 window code now avoids direct wide-window style setters and resolves layered-window opacity dynamically. ANSI clipboard and title conversion are used by the legacy path; characters unavailable in the active system code page can be lossy. No native Win9x/XP runtime qualification is implied by these source changes.

The native smoke probe now rejects a mismatched OS profile and pointer size. On a modern build host, verify returned evidence with `python TKGPU/tools/TKGPUVerifyWindowsEvidence.py --evidence target-evidence.json --profile 98 --revision FULL_COMMIT_SHA --library TKGPUCompat.dll --probe tkgpu_windows_evidence.exe --output checked-evidence.json`. Profiles 95 and xp are separate. The verifier checks both PE images and records their SHA-256 digests; these hashes identify the submitted artifacts, not independently prove what ran in the VM. Full qualification still requires the documented remaining gates. The CMake target `tkgpu_windows_evidence` is restored for Windows builds.
