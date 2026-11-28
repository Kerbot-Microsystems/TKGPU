# About TKGPU

TKGPU is a desktop UI framework developed and maintained by **Kerbot
Microsystems**. Its public implementation combines a C99 runtime, a retained
widget tree, Canvas drawing, native window adapters and Python compatibility
bindings. It targets useful Tcl/Tk and Tkinter behavior while remaining an
independent implementation.

Project repository: <https://github.com/Kerbot-Microsystems/TKGPU>

Company and product information: <https://kerbotmicrosystems.com/>

## Kerbot Drone and the framework

**Kerbot Drone** is Kerbot Microsystems' desktop application for drone design
and engineering. Its modern Windows and Linux builds use TKGPU to support the
application interface: editing components and connections, navigating the
workspace, opening dialogs, displaying engineering views and hosting extension
windows.

TKGPU supplies the interface runtime. The application's engineering algorithms,
component catalog, account services and product licensing belong to the product
and are separate from this framework. Product integration is not a claim of
complete compatibility with arbitrary Tkinter programs.

The Windows XP legacy edition of Kerbot Drone retains stock Tkinter. Experimental
framework backends and historical compatibility targets should not be confused
with supported product editions.

## What the public framework contains

- A Tcl-compatible interpreter and object model implemented in C99, with an
  evolving compatibility API and Python binding layer.
- Retained widgets and Canvas items, layout management, text editing, focus,
  callbacks, event dispatch and timers.
- A software rendering path and platform-specific presentation options,
  including optional Direct3D 11 on Windows.
- Native window integration for Windows and X11, plus an experimental Cocoa
  backend. Features and dependencies vary by build and platform.
- Unicode character classification and simple case mappings derived from the
  Unicode Character Database, alongside optional native text shaping.
- Build tools, compatibility checks and tests that can be inspected and run
  from the repository.

## Platform scope

| Backend | Public scope |
| --- | --- |
| Windows / Win32 | Native windows, input and presentation; used by modern Kerbot Drone Windows builds. Optional acceleration depends on the build and system. |
| Linux / X11 | Native X11 windows and input; used by Kerbot Drone Linux builds. Optional Pango/Cairo text support has separate system dependencies. |
| BSD / X11 | An X11 backend target; requires validation on the intended BSD environment. |
| macOS / Cocoa | Experimental backend; not a general production support guarantee. |
| Historical Windows targets | Compatibility work and test targets; not an assertion that every legacy OS can run the framework or the current product. |

Platform parity must be established with actual builds and application tests.
A successful compile, one passing test or a single benchmark does not establish
complete application compatibility or a universal performance improvement.

## Public development priorities

Work focuses on stable window lifecycles and resizing, responsive input,
predictable layout, multilingual text, Canvas correctness and Tcl/Tk API
compatibility. Tests and reproducible reports help distinguish supported behavior
from remaining gaps. These are development priorities, not promised release
dates or guarantees of feature completeness.

## Reporting issues and contributing

Use the repository's [issue tracker](https://github.com/Kerbot-Microsystems/TKGPU/issues)
for framework issues and pull requests for proposed changes. Include the source
revision, OS, architecture, compiler or Python version where relevant, build
options, reproduction steps and expected versus actual behavior. A small example
that isolates the affected widget, event or rendering operation is especially
useful.

Remove account details, access tokens, personal paths and private project data
from logs and screenshots before posting. Product-specific account or licensing
questions belong with Kerbot Drone support rather than a public framework report.

Build instructions are in the [README](../README.md). The
[architecture notes](../ARCHITECTURE.md), [Python binding documentation](../bindings/python/README.md)
and [release notes](../RELEASE.md) provide deeper technical context; dated plans
and historical qualification notes are not current platform support guarantees.

## Licensing and names

Original framework code is licensed under [MIT](../LICENSE), with existing
copyright notices preserved. Bundled zlib code and Unicode-derived data retain
their own licenses; see [third-party notices](../THIRD_PARTY_NOTICES.md).
Optional system libraries and externally supplied fonts have their own terms.

Kerbot Microsystems and Kerbot Drone identify the maintainer and product. The
framework's open-source license does not license the separate product or grant
trademark rights. Compatibility with Tcl/Tk and Python does not imply endorsement
by their maintainers or by Unicode, Inc.
