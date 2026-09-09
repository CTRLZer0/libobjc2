# Architecture

CTRLZer0 libobjc2 keeps public Objective-C runtime headers separate from
implementation details and groups implementation sources by subsystem.

## Source layout

- `objc/` — public runtime API headers.
- `src/runtime/` — class, protocol, metadata and runtime lifecycle logic.
- `src/dispatch/` — selectors, dispatch tables and message lookup.
- `src/dispatch/asm/` — architecture-specific native messenger assembly.
- `src/memory/` — ARC, weak references, associated objects and GC support.
- `src/blocks/` — blocks runtime and block-to-IMP support.
- `src/encoding/` — Objective-C type encoding support.
- `src/exceptions/` — Objective-C / Objective-C++ exception runtime.
- `src/platform/windows/` — Windows-specific runtime integration.

## Build model

`CMake/RuntimeSources.cmake` is the canonical source manifest. Build entry
points consume the same subsystem lists rather than maintaining duplicate file
inventories.

The Mosaic Windows build intentionally omits native guest message-send
assembly. Guest ARM64 dispatch is owned by Mosaic / Dynarmic while libobjc2
provides the host-side Objective-C runtime model.
