<p align="center">
  <img src="assets/tkgpu-logo.svg" alt="TKGPU Logo" width="140" />
</p>

<h1 align="center">TKGPU</h1>

<p align="center">
  <strong>Independent C99 Tcl/Tk-compatible runtime, retained UI/Canvas engine, and software/GPU rasterizer</strong>
</p>

---

> **Experimental Status:** TKGPU is an independent, original C99 implementation targeting Tcl/Tk 8.6 semantics and the `_tkinter` C ABI. It is **not** a fork of Tcl/Tk, does not link against stock Tcl/Tk at runtime, and is not yet a complete drop-in replacement for full Tcl/Tk.

## Overview

TKGPU is developed and maintained by **Kerbot Microsystems**. It is the UI
framework used by **Kerbot Drone**, the company's desktop drone design and
engineering application. The framework is maintained in this standalone
repository so its runtime, bindings and platform backends can be built and
examined independently of the product.

Kerbot Drone uses TKGPU for its desktop interface, component workspace, Canvas
editing, dialogs and extension windows in its modern Windows and Linux builds.
The legacy Windows XP edition retains stock Tkinter. An application's use of
the framework does not mean that every Tcl/Tk application or every target OS is
fully compatible.

Read [About the project](docs/PROJECT.md) for the product relationship,
platform scope, contribution process and public development priorities.

TKGPU provides:

- **Portable C99 Core:** An original Tcl-subset interpreter, reference-counted object/list/expression engine, UTF-8 text primitives, and monotonic timer notifier (`Tcl_CreateTimerHandler`, `Tcl_DoOneEvent` timer subset, `vwait`/`tkwait`).
- **Retained Widget & Canvas Engine:** Built-in widget tree (`frame`, `label`, `button`, `canvas`, `entry`, `listbox`, `text`, `treeview`, `notebook`, `scrollbar`, `toplevel`, `menu`, and `ttk` controls) with `pack`, `place`, and `grid` geometry managers.
- **CPU & GPU Rasterization:** Portable RGB software framebuffer with damage tracking, clipping, and optional hardware-accelerated rendering (Direct3D 11 on Windows with automatic CPU fallback).
- **Native Windowing Adapters:** Native Win32 (Unicode & legacy ANSI fallback), Linux/BSD X11 (with optional XIM), and experimental macOS Cocoa support.
- **International Text Shaping:** Native text shaping via Uniscribe (Windows) or Pango/Cairo (POSIX, when enabled with `TKGPU_NATIVE_TEXT=1`), plus a built-in fallback bitmap renderer and optional offline `.tgf` Unicode glyph pack loader.
- **Compatibility & Python Bindings:** `TKGPUCompat` shared library exposing a growing Tcl/Tk C API subset alongside Python `_tkinter`-compatible bindings in `bindings/python/tkgpu`.

## Dependency Policy

- **Core & Software Renderer:** Standard ISO C99 library only. No Tcl/Tk, Qt, SDL, GLFW, or Python required at runtime.
- **Compression:** Uses a statically compiled, unmodified subset of **zlib 1.3.1** (`third_party/zlib`) configured with `Z_SOLO`.
- **Platform APIs:** Isolated at the native adapter boundary (Win32/GDI/D3D11/Uniscribe on Windows; X11 and optional Pango/Cairo on POSIX).

## Building & Testing

### CMake (Recommended)

Requires CMake 3.20+ and a C99 compiler:

```bash
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

### Direct C99 Build (Minimal Core & Shell)

```bash
cc -x c -std=c99 -Wall -Wextra -Wpedantic -Iinclude \
  src/core/*.C src/core/commands/*.C src/memory/*.C src/text/*.C src/object/*.C \
  src/render/*.C src/render/raster/*.C src/render/image/*.C src/render/text/*.C \
  src/widgets/*.C src/widgets/canvas/*.C src/widgets/image/*.C \
  src/layout/*.C src/events/*.C src/events/timers/*.C \
  src/shell/TKGPUShell.C -o tkgpu_shell

./tkgpu_shell -c 'set x 40; incr x 2'
```

*(Note: Source files use `.C` / `.H` extensions and are compiled as **C99**, not C++.)*

### Default Windows application icon

New Windows windows use the **TKGPU repository logo** by default, including
when the runtime is loaded by Python or another host EXE. The icon is embedded
in the runtime DLL, so it does not depend on an external image or the working
directory. An application's explicit `wm iconbitmap` / `wm iconphoto` setting
can replace it; Kerbot Drone continues to use its own product icon.

The CMake-built shell and native demo also embed the logo in their EXE resources,
so Explorer and the window caption have a project icon. Downstream applications
can opt in without conflicting with their existing application resources:

```cmake
add_subdirectory(TKGPU)
add_executable(my_app main.c)
target_link_libraries(my_app PRIVATE tkgpu_compat)
tkgpu_set_application_icon(my_app)
# Or embed an application's own .ico:
# tkgpu_set_application_icon(my_app ICON "${CMAKE_CURRENT_SOURCE_DIR}/my-app.ico")
```

Call the helper once per target. It changes the EXE file icon; set an explicit
window icon as well if the application's windows should use its own branding.
The committed `assets/tkgpu-logo.ico` is generated from `assets/tkgpu-logo.svg`
with transparent, multi-size DIB frames. Regeneration only requires the
development tools documented in [TKGPUBrandIcon.py](tools/TKGPUBrandIcon.py);
normal builds do not require Python, Pillow or an SVG renderer for the icon.

### Optional Reference Conformance Check (Development Only)

If Python and Tcl 8.6 are installed on the development machine (neither is linked into TKGPU):

```bash
python tests/conformance/TKGPUCompare.py ./build/tkgpu_shell
```

### Optional Legacy Bitmap Font Pack

To generate an offline indexed glyph pack from a lawfully obtained `.hex` font:

```bash
python tools/TKGPUUnicodeFontPack.py source.hex unicode.tgf
```

Set `TKGPU_LEGACY_FONT_PACK=path/to/unicode.tgf` and `TKGPU_NATIVE_TEXT=1` at runtime. No third-party font data is bundled in this repository.

## Repository Structure

```text
TKGPU/
├── assets/          # Project logo and static assets
├── bindings/python/ # Python host & tkinter compatibility adapter
├── cmake/           # CMake build modules
├── docs/            # Architecture, platform notes, and legal checklists
├── include/         # Public C headers
├── src/             # C99 implementation (core, compat, widgets, render, platform)
├── tests/           # Unit, ABI, conformance, platform, and widget tests
├── third_party/     # Bundled third-party sources (zlib 1.3.1) with upstream notices
└── tools/           # ABI audit, font packager, and host qualification scripts
```

See [ARCHITECTURE.md](ARCHITECTURE.md) and [RELEASE.md](RELEASE.md) for internal design gates and release qualification details.

## License & Third-Party Notices

Original TKGPU code is available under the [MIT License](LICENSE). Existing
author and third-party copyright notices are retained. Kerbot Drone is a
separate product with its own distribution and licensing terms; TKGPU's MIT
license does not grant rights to that application's code, artwork or branding.
See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for bundled notices and
[docs/legal/DEVELOPMENT_AND_DISTRIBUTION.md](docs/legal/DEVELOPMENT_AND_DISTRIBUTION.md)
for distribution review guidance.

### Bundled Third-Party Software

| Component | Version | License | Location & Notice |
| :--- | :--- | :--- | :--- |
| **zlib** (unmodified subset, `Z_SOLO`) | 1.3.1 | [zlib License](third_party/zlib/LICENSE) | `third_party/zlib/` — Copyright (C) 1995–2022 Jean-loup Gailly and Mark Adler. See [TKGPU-NOTICE.txt](third_party/zlib/TKGPU-NOTICE.txt). |
| **Unicode Character Database-derived tables** | Unicode 16.0 | [Unicode License V3](third_party/unicode/LICENSE.txt) | General-category and simple case-mapping tables in `src/compat/TKGPUUnicodeClasses.H` and `src/compat/TKGPUUnicodeCase.H`. See [provenance](third_party/unicode/README.md). |

### External / Optional System & Development Dependencies (Not Bundled)

| Component | Role | Applicable Terms |
| :--- | :--- | :--- |
| **CPython / `_tkinter` ABI** | Interoperability target for `bindings/python` | [PSF License](https://docs.python.org/3/license.html) (no CPython source/binaries bundled) |
| **Tcl/Tk** | Optional dev-only conformance comparison | [Tcl/Tk License](https://www.tcl-lang.org/software/tcltk/license.html) (no Tcl/Tk binaries linked) |
| **X11 / Pango / Cairo** | Linux/BSD native windowing & optional text shaping | X11 (MIT/X Consortium); Pango/Cairo (**LGPL-2.1-or-later** — verify dynamic linking/relinking & notice obligations if distributing POSIX binaries built with Pango) |
| **Win32 / GDI / D3D11 / Uniscribe** | Windows native presentation & text shaping | Standard Windows OS system APIs & compiler CRT redistribution terms |
| **Pillow** | Optional Python image helper in bindings/tests | HPND License (not bundled) |

> **Trademark & Endorsement Disclaimer:** Tcl/Tk and Python/Tkinter compatibility is a technical interoperability target only and does not imply endorsement by the Python Software Foundation or the Tcl/Tk maintainers.
