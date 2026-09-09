# Architecture

CTRLZer0 libobjc2 follows current GNUstep libobjc2 while keeping runtime
implementation details organized by subsystem.

## Source layout

- `objc/` — public Objective-C runtime API headers.
- `src/runtime/` — class, protocol, metadata and runtime lifecycle logic.
- `src/dispatch/` — selectors, dispatch tables and message lookup.
- `src/dispatch/asm/` — architecture-specific native messenger assembly.
- `src/memory/` — ARC, weak references, associated objects and GC support.
- `src/blocks/` — blocks runtime and block-to-IMP support.
- `src/encoding/` — Objective-C type encoding support.
- `src/exceptions/` — Objective-C / Objective-C++ exception runtime.
- `src/internal/` — private headers grouped by the same subsystems.
- `tests/` — GNUstep upstream runtime test suite.
- `tests/windows/` — Mosaic-facing Windows integration contracts.

## Build model

The root CMake project is the canonical GNUstep runtime build. Windows shared
and static targets use the same C, C++, Objective-C, Objective-C++ and native
assembly implementation.

`msvc/mosaic/` is a thin integration adapter. It creates an isolated LLVM 23
Ninja build of the current GNUstep static runtime and exposes it unchanged as
`Mosaic::ObjCRuntime`. It also publishes the generated `objc-config.h`.

Mosaic-specific initialization is limited to
`mosaic_objc_runtime_initialize()`, a public wrapper over the normal GNUstep
runtime initialization path. There is no separate C-only dispatch runtime or
parallel source inventory for Mosaic.
