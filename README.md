# CTRLZer0 libobjc2

CTRLZer0 libobjc2 is a modern Objective-C runtime focused on portable host
execution, with first-class Windows support for the Mosaic compatibility
runtime.

The project is derived from the historical GNUstep / Microsoft libobjc2 work,
but is now maintained as an independent CTRLZer0 repository. The original
runtime remains MIT licensed. New CTRLZer0-owned files are licensed separately
where explicitly marked.

## Current status

- Windows x86-64 host runtime builds with `clang-cl`.
- The Mosaic path intentionally excludes the native Objective-C messenger;
  guest ARM64 dispatch is handled by Mosaic / Dynarmic.
- Class allocation, selector registration, method installation, instance
  creation and native IMP lookup are covered by a host contract test.
- The current Windows contract returns `objc-runtime-value=42`.
- LLVM 23.1.1 is the target compiler baseline.

## Repository layout

- `objc/` — public Objective-C runtime headers.
- `tests/` — centralized runtime and compatibility test suite.
- `tests/windows/` — Windows host contracts used by Mosaic.
- `benchmarks/` — performance benchmarks and regression baselines.
- `msvc/mosaic/` — embeddable Windows build entry point for Mosaic.
- `docs/UPSTREAM_RUNTIME_NOTES.md` — preserved historical runtime notes.

The implementation sources are being migrated from the historical flat root
into cohesive runtime modules. Refactors are required to remain behavior
preserving and test-green before performance work is merged.

## Build: Mosaic Windows runtime

The Mosaic build produces a static `mosaic_objc_runtime` target and the
`Mosaic::ObjCRuntime` alias. It uses `clang-cl` for the Objective-C sources and
is designed to be embedded into a Visual Studio / MSVC parent project.

```powershell
cmake -S msvc/mosaic -B build/mosaic -DBUILD_TESTING=ON
cmake --build build/mosaic --config Release
ctest --test-dir build/mosaic -C Release --output-on-failure
```

## Testing policy

Runtime changes must add or update tests before performance tuning or ABI
changes are accepted. The suite intentionally follows the project's existing
style: small C / Objective-C executables, direct runtime API calls, and
`assert()`-based validation rather than an external unit-test framework.

Coverage is being organized around:

- class and metaclass lifecycle;
- selectors and method dispatch tables;
- ARC, weak references and autorelease behavior;
- associated objects and properties;
- protocols, ivars and metadata loading;
- blocks and IMP bridging;
- forwarding and exceptions;
- Windows / Mosaic host integration.

Performance work must include a benchmark or measurable regression test for the
hot path being changed. Selector lookup, class lookup, dispatch-table lookup,
ARC / weak operations and associated-object access are initial priorities.

## Licensing and provenance

See `COPYING`, `LICENSE-CTRLZERO`, and `NOTICE.md`. Historical runtime code and
modifications of that code retain the MIT terms. New CTRLZer0-owned files may
be AGPL-3.0-only when they carry that SPDX identifier.
