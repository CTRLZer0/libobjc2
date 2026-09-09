# Architecture

CTRLZer0 libobjc2 follows current GNUstep libobjc2 while keeping runtime
implementation details organized by subsystem.

## Source layout

- `objc/` - public Objective-C runtime API headers.
- `src/runtime/` - class, protocol, metadata, properties and lifecycle logic.
- `src/dispatch/` - selectors, dispatch tables and message lookup.
- `src/dispatch/asm/` - architecture-specific native messenger assembly.
- `src/memory/` - ARC, weak references, associated objects and allocation.
- `src/blocks/` - Blocks runtime and block-to-IMP support.
- `src/encoding/` - Objective-C type encoding support.
- `src/exceptions/` - Objective-C / Objective-C++ exception runtime.
- `src/internal/` - private headers grouped by the same subsystems.
- `tests/` - GNUstep runtime regression suite.
- `tests/windows/` - CTRLZer0 / Mosaic Windows integration contracts.
- `benchmarks/` - focused performance regression coverage.

## Build model

The root CMake project is the canonical GNUstep runtime build. Shared and static
targets use the same C, C++, Objective-C, Objective-C++ and native assembly
implementation.

`msvc/mosaic/` is a thin integration adapter. It creates an isolated LLVM 23
Ninja build of the static runtime and exposes it as `Mosaic::ObjCRuntime`. The
adapter also publishes the generated `objc-config.h` required by public headers.

Mosaic-specific initialization is limited to
`mosaic_objc_runtime_initialize()`, a public wrapper over the normal GNUstep
runtime initialization path. There is no second C-only runtime and no parallel
source inventory.

## Upstream integration

GNUstep is the semantic upstream for core runtime work. Upstream code is adapted
into the existing subsystem paths rather than flattening the CTRLZer0 layout.
The Git history contains both the GNUstep lineage and the earlier CTRLZer0 /
Microsoft-derived lineage through an explicit history-only merge.
