# ABI audit

Initial observed target: CPython 3.13.5 on Linux x86_64, Tcl/Tk 8.6.
The 71-symbol fixture is a platform-specific observation, not a universal Tcl/Tk interface.

    python tools/TKGPUAbiAudit.py --manifest tests/abi/CPython313LinuxX64.txt
    python tools/TKGPUAbiAudit.py --extension /path/to/_tkinter.so --library /path/to/candidate.so --require-all

Audit Windows import tables and macOS Mach-O separately; include Tcl_Obj/Tcl_Interp layout, exported data, thread semantics, calling conventions, error codes and behavior in separate tests. Internal tg_* functions are not counted as Tcl/Tk exports. At v0.9 the separate TKGPUCompat shared library provides five initial Tcl C API exports. This is a source-level facade using opaque TKGPU types; the real Tcl struct layouts and binary ABI are not implemented. No drop-in swap is allowed.

Public reference documentation:
https://www.tcl-lang.org/man/tcl8.6/TclLib/Object.htm
https://www.tcl-lang.org/man/tcl8.6/TclLib/contents.htm
https://docs.python.org/3.13/library/tkinter.html

The implemented names are Tcl_CreateInterp, Tcl_DeleteInterp, Tcl_Eval, Tcl_GetStringResult and Tcl_ResetResult. Tcl_Init and Tk_Init intentionally remain absent rather than returning fake success.

## v0.10

Eight additional entry points: Tcl_GetVar2, Tcl_SetVar2, Tcl_UnsetVar2, Tcl_ExprLong, Tcl_ExprBoolean, Tcl_ExprString, Tcl_AttemptAlloc and Tcl_Free. Variables are scalar-only with global/error flags; arrays, namespace resolution and traces are not implemented. Expressions are limited to signed 64-bit arithmetic, including numeric scalar substitutions. Allocation/free must be paired within the same library. These exports remain a partial source-level interface, not binary compatibility.

Also provides Tcl_SplitList and Tcl_GetBoolean (partial list syntax and standard Boolean words, unambiguous prefixes and 0/1). Split-list allocation is released with Tcl_Free. Total current source-level export contract: 15 names.

## v0.11

Nine additional Tcl-named functions for string and integer objects and argument-vector evaluation. Four TKGPU-owned helpers provide retain/release and copying results. The objects are opaque and reference-counted with one owned initial reference. Do not pass them to real Tcl or use Tcl refcount macros: the binary layout and reference semantics are not compatible. Counted byte strings may contain NUL; the existing command evaluator rejects embedded NUL explicitly. Standard UTF-8 helpers are separate from Tcl 8.6 modified UTF-8. 24 Tcl-named source exports are declared; audit the actual binary before claiming exported symbols.

## v0.12

Adds object result and scalar variable snapshots, object Boolean conversion, byte-array values, and immutable object-list construction and access. These objects use independent TKGPU ownership rules; result and variable snapshots are not Tcl identity-preserving objects, and list children are borrowed while the parent is alive. Byte and string pointers are read-only under this preview contract despite Tcl-shaped signatures. Standard UTF-8 Latin-1 byte conversion is not the modified UTF-8 of Tcl 8.6; embedded NUL cannot yet be passed through the command interpreter. List serialization is limited to the supported syntax and 4095 bytes. There are 35 Tcl-named declarations in the source export contract, not verified binary compatibility.

## v0.13 ABI prefix

This supersedes the previous owned-reference convention for Tcl-named constructors only. Internal tg_* constructors still return owned values. TclFreeObj is added to the source export contract. Test both the internal field offsets and the original-header consumer; the latter is built and linked against TKGPUCompat without linking Tcl. The reference header is a development-only dependency. The Tcl_ObjType pointer remains null; actual type semantics and complete Tcl ABI are not implemented. Original public field contract: https://www.tcl-lang.org/man/tcl8.6/TclLib/Object.htm

Tcl_SetObjResult now retains the supplied object and preserves identity when read back. Other result changes still use string snapshots and may defer releasing the old object until the next lookup or interpreter destruction. Object-variable identity remains incomplete.

## v0.14

Adds four Tcl-named functions: Tcl_GetObjType, Tcl_NewDoubleObj, Tcl_GetDouble and Tcl_GetDoubleFromObj. Type identity descriptors have the public member sequence but no conversion or mutation callbacks; finite double semantics are a restricted subset. The current source manifest has 40 names, not a verified binary export or full ABI claim.

## v0.15

Adds three Tcl-named timer/notifier functions (43 total source contract names). Tcl_DoOneEvent supports timer events only, with a nonblocking flag and bounded sleep until a pending timer deadline. The native notifier does not support older Windows systems, thread alerts, file handlers, window/idle events, or arbitrary library unload. Tests must run before treating these symbols as usable.

## v0.16

Eight additional Tcl-named synchronization entry points, seven from the observed CPython import manifest. A two-thread test exercises lazy initialization, notification, timed wait, TLS isolation and finalization. Source declarations are not verified binary exports. Thread queue/alert and full notifier remain absent.

## v0.17

Three new names: Tcl_QueueEvent, Tcl_ThreadQueueEvent and Tcl_ThreadAlert. Public event prefix (callback then next pointer), ownership transfer, ordered queue insertion and cross-thread wakeup use the documented Tcl 8.6 notifier interface: https://www.tcl-lang.org/man/tcl8.6/TclLib/Notifier.htm. A two-thread test covers posted event delivery, flags, retention and queue order. This does not implement the full notifier or establish binary compatibility.

## v0.20

Four more imported names: Tcl_CreateObjCommand, Tcl_DeleteCommand, Tcl_EvalFile, Tcl_AddErrorInfo. Object callback arguments are retained during execution; raw script arguments remain strings. These are limited implementations, not full Tcl semantics. The binary has not been verified by this source manifest alone.

## v0.21

Seven additional observed imports: five TclBN operations plus Tcl_NewBignumObj and Tcl_GetBignumFromObj. The independent 28-bit configuration is based on the documented Tcl 8.6 mp_int public contract and local reference measurement, not copied arithmetic source. Reference: https://tcl-lang.com/man/tcl8.6/TclLib/IntObj.htm . Binary layout must be checked for each target toolchain. Bignum type identity does not imply compatible internal representation; integration tests remain mandatory.
