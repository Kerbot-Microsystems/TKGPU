# Release gates

For current project identity, Kerbot Drone integration and platform scope, see
[About TKGPU](docs/PROJECT.md). The dated preview plans below are historical
qualification notes, not a current statement that Kerbot Drone does not use
TKGPU, nor a guarantee of complete Tcl/Tk or cross-platform compatibility.

## December 1, 2026: experimental developer preview target

The preview is a standalone C runtime, not a replacement DLL for Tcl/Tk or a production Kerbot Drone GUI. Do not modify or enable the main application's existing Tcl/Tk integration.

Required before publishing:
- Clean Release build using the C compiler (uppercase .C explicitly treated as C).
- All core, expression, list, widget, control, object, native command and compatibility facade tests pass.
- Run the optional Tcl 8.6 differential suite and publish exact results, not a coverage percentage.
- On Windows, build and manually exercise the native demo (startup, resize, click, close).
- Record compiler, OS, architecture, exact commit and results.
- Package only the independently built preview; no Tcl/Tk or other third-party runtime bundled.
- Document known gaps: Tcl ABI, Python _tkinter integration, real widget behavior, ttk, Unicode, native Linux/macOS windows and GPU acceleration.

## January 1, 2027: next integration milestone target

Aim for an opt-in compatibility prototype with a separately tested host interface, expanded Tcl semantics, widget commands and event handling. Full Tcl/Tk 8.6 compatibility is a goal, not a date-based guarantee. Block any default switch until the original application's regression suite, native platforms, ABI checks and benchmark criteria pass.

## Reproducible local build

    cmake -P TKGPU/tools/BuildRelease.cmake

CMake and compiler are build tools only. Optional reference Python/Tcl are development test tools only. A successful compile does not establish compatibility or performance.

## Evidence

Pending: no verified build result or native GUI result recorded for v0.39 in this file. The compatibility shared library is an experimental artifact and not a system replacement.

## Required Windows target matrix

Windows 98/98 SE, XP, Vista, 7, 8 and 8.1 are explicit targets, in addition to current Windows. See [Windows compatibility contract](docs/platform/WindowsCompatibility.md). The matrix does not imply that these targets already work. Require target-specific PE/CRT audit and actual runtime evidence. Legacy OS support must not be dropped silently.

## ABI 1.4 maintenance regression rule

Before switching Kerbot to the direct host by default, require a successful native build and complete existing test suite, both direct-host GUI smoke modes (indexed and linear), and a comparison against the last manually verified Kerbot build. Record the exact known-good commit rather than assuming an untested branch head is stable. Manually exercise startup, switching tabs rapidly, moving and resizing dialogs, opening images and the terminal, editing text, and closing windows. Check CPU usage, presented frame timing and memory under the same workload. Do not infer application speed or parity from a lookup microbenchmark. Keep the existing staged bridge available until these checks pass. `TKGPU_LINEAR_WIDGET_LOOKUP=1` is a diagnostic switch for widget path lookup only; it does not restore a previous application build.
