# Runtime architecture

Portable C core -> platform adapter -> OS window/input/presentation APIs.
Tcl-compatible language runtime -> Tk-compatible widget commands -> layout and event dispatch -> retained scene -> renderer.

## Milestones (not implemented unless marked)

1. Foundation: private C API, minimal script evaluation, software framebuffer, native Win32 presentation demo and core tests (initial implementation).
2. Tcl compatibility: object model, substitution, lists, expression evaluation, call frames, errors, namespaces, encoding, channels, timers and C API. Differential tests against reference Tcl 8.6.
3. Tk compatibility: widget tree, options, bindtags, focus, geometry managers, canvas, text, menus, images, fonts, accessibility and ttk. Compare observable command results, not just screenshots.
4. Native backends: Win32, Linux display protocol and macOS (native system APIs, with a thin platform-specific shim where required). Audit old-OS imports independently. Apple Silicon and x86 require native builds; no binary portability claimed.
5. Renderer: CPU baseline and optional native GPU implementations; retained scene, batched drawing, bounded damage tracking, resource lifetime, thread ownership and device-loss fallback.
6. CPython integration: compare required Tcl/Tk symbols and calling conventions for each supported CPython build; implement stubs, callbacks and interpreter lifecycle. Never replace system Tcl/Tk globally. Use explicit opt-in isolated artifacts.
7. Acceptance: compare Kerbot Drone workflows on stock and replacement runtime; verify document semantics, startup, input latency, frame-time percentiles, memory, CPU and correctness under load. Require reference tests and reproducible results before any default switch.

Unsupported behavior must return explicit errors. A successful launch is not evidence of full compatibility. No hidden fallback to Tcl/Tk is permitted in the independent runtime.

Current v0.5 foundation adds bounded nested command evaluation, isolated parser frames and checked signed 64-bit decimal arithmetic. This is still a private API, not a compatible Tcl ABI. All .C files are explicitly compiled as C99; TKGPU-prefixed source names distinguish the independent implementation.

## Independent host ABI (new primary direction)

TKGPU owns a separate, versioned native host contract in `include/TKGPUHost.H`. ABI 1 exposes opaque interpreter handles, literal UTF-8 command vectors, callbacks, list splitting and event processing. It does not use Tcl/Tk headers or require an existing _tkinter binary. The Python binding in `bindings/python/tkgpu` calls only this ABI. Existing Tcl-named exports remain a transitional compatibility facade and reference test target; Tcl 8.5/8.6 are not versions of the new host ABI.

The next integration gate is an independently validated Tkinter API adapter on top of the host, including typed results, thread marshalling, widget and application regression tests. Do not switch the default Kerbot launcher or claim Win9x runtime support until those gates pass. Native library build/CRT and Python host qualification remain separate per OS.
