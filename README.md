# CTRLZer0 libobjc2

[![CI](https://github.com/CTRLZer0/libobjc2/actions/workflows/ci.yml/badge.svg)](https://github.com/CTRLZer0/libobjc2/actions/workflows/ci.yml)

CTRLZer0 libobjc2 is a modern Objective-C runtime maintained for portable
runtime work and for the Mosaic compatibility environment, with first-class
Windows support and LLVM 23 as the current compiler baseline.

The runtime now follows the current GNUstep libobjc2 implementation rather than
the historical Microsoft snapshot. The GNUstep and CTRLZer0 histories are both
preserved in Git, while CTRLZer0 keeps its subsystem-oriented repository layout,
Mosaic integration, validation, packaging, and portability work.

## Current status

- Based on current GNUstep libobjc2 runtime semantics and ABI work.
- Sources are organized by subsystem under `src/`.
- Windows x86-64 builds with LLVM 23.1.1.
- Shared and static Windows runtimes can be built together.
- Mosaic consumes the runtime through `Mosaic::ObjCRuntime`.
- Explicit initialization is available through
  `mosaic_objc_runtime_initialize()`.
- The GNUstep runtime suite and dedicated CTRLZer0 Windows contracts are both
  used as regression gates.
- Runtime and Windows contracts build with warnings treated as errors in CI.

## Repository layout

- `objc/` — public Objective-C runtime headers.
- `src/runtime/` — class, protocol, metadata, properties and lifecycle logic.
- `src/dispatch/` — selectors, dispatch tables and message lookup.
- `src/dispatch/asm/` — architecture-specific messenger assembly.
- `src/memory/` — ARC, weak references, associated objects and allocation.
- `src/blocks/` — Blocks runtime and block-to-IMP support.
- `src/encoding/` — Objective-C type encoding support.
- `src/exceptions/` — Objective-C / Objective-C++ exception support.
- `src/internal/` — private headers grouped by subsystem.
- `tests/` — upstream runtime tests plus CTRLZer0 contracts.
- `benchmarks/` — focused performance and regression benchmarks.
- `scripts/ci/` — reproducible local and CI build / packaging entry points.
- `docs/` — maintained documentation and archived historical material.

See `docs/ARCHITECTURE.md` for the build and integration model.

## Mosaic integration

Mosaic uses the dedicated CMake adapter in `msvc/mosaic/`. It builds the
complete modern runtime in an isolated LLVM configuration and exposes the same
stable integration target to consumers:

```cmake
target_link_libraries(your_target PRIVATE Mosaic::ObjCRuntime)
```

The runtime can also be initialized explicitly when it is embedded outside the
normal compiler-emitted module loading path:

```c
#include <objc/mosaic.h>

mosaic_objc_runtime_initialize();
```

This entry point delegates to the normal GNUstep runtime initialization path;
it does not maintain a second Mosaic-specific Objective-C runtime.

## Windows build and tests

The supported Windows entry point installs / selects the requested LLVM toolchain,
configures the Mosaic adapter, builds the runtime, and runs the contracts:

```powershell
./scripts/ci/build-windows.ps1 `
  -ClangCl C:\Tools\LLVM-23.1.1\LLVM\bin\clang-cl.exe `
  -Configuration Release `
  -BuildDir out/local-release `
  -WarningsAsErrors `
  -BuildBenchmarks
```

The GNUstep test suite can additionally be enabled from the repository root
with standard CMake options. Runtime changes should preserve both the upstream
suite and the CTRLZer0 contracts rather than adapting one at the expense of the
other.

## Upstream relationship

GNUstep libobjc2 is the semantic upstream for the core runtime. CTRLZer0 keeps
its own integration and validation layers and may carry portability or runtime
fixes while they are being evaluated for upstreaming.

Updates should be integrated from the current GNUstep branch and adapted into
the existing subsystem layout. Do not re-import an old Microsoft snapshot or
flatten the repository merely to match upstream paths.

The migration preserves both histories: current development descends from the
GNUstep history and is connected to the earlier CTRLZer0 / Microsoft-derived
line through an explicit history merge. Historical release announcements and
legacy documentation are retained under `docs/archive/upstream/` for provenance.

## Automation and releases

Every push and pull request validates Windows Debug and Release builds with
LLVM 23.1.1. Release validation includes the runtime contracts and performance
benchmark build. Scheduled nightly builds and `v*` tags publish packaged Windows
artifacts through GitHub Actions.

See `docs/CI.md` for the current automation policy and `CHANGELOG.md` for
CTRLZer0-specific development history.

## Licensing and provenance

The repository is distributed under the MIT License in `COPYING`, except for
third-party material that explicitly states different terms. GNUstep, Microsoft,
and other upstream copyright and attribution remain preserved.

CTRLZer0-authored files and modifications use the same MIT terms. See
`NOTICE.md` for provenance details and `CONTRIBUTING.md` for contribution rules.
