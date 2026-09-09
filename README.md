# CTRLZer0 libobjc2

CTRLZer0 libobjc2 is a modern Objective-C runtime focused on portable host
execution, with first-class Windows support for the Mosaic compatibility
runtime.

The project descends from the historical GNUstep / Microsoft libobjc2 codebase
but is now maintained as an independent CTRLZer0 repository. Upstream history,
copyright notices and MIT licensing are preserved for inherited code.

## Current status

- Windows x86-64 host runtime builds with `clang-cl`.
- Mosaic uses libobjc2 as a host-side Objective-C runtime model while guest
  ARM64 message dispatch remains owned by Mosaic / Dynarmic.
- Runtime initialization is available through
  `mosaic_objc_runtime_initialize()` without a compiler-emitted GNUstep module.
- Windows runtime contracts currently cover selectors, classes, ivars, weak
  references, associated objects and native method lookup.
- LLVM 23.1.1 is the target compiler baseline.

## Repository layout

- `objc/` — public Objective-C runtime headers.
- `src/` — implementation sources grouped by subsystem.
- `tests/` — centralized runtime and compatibility tests.
- `benchmarks/` — performance and regression benchmarks.
- `docs/` — maintained documentation and archived upstream material.
Implementation modules are documented in `docs/ARCHITECTURE.md`. Historical
release announcements and legacy API / installation notes are preserved under
`docs/archive/upstream/` and are not current project guidance.

## Build: Mosaic Windows runtime

The Mosaic entry point produces `mosaic_objc_runtime` and the
`Mosaic::ObjCRuntime` alias.

```powershell
cmake -S msvc/mosaic -B build/mosaic -DBUILD_TESTING=ON
cmake --build build/mosaic --config Release
ctest --test-dir build/mosaic -C Release --output-on-failure
```

`CMake/RuntimeSources.cmake` is the canonical source inventory shared by build
entry points. New sources should be registered there rather than copied into
separate build-specific file lists.

## Testing policy

Runtime changes should add or update tests before ABI or performance work is
merged. Tests intentionally follow the existing project style: small C /
Objective-C executables, direct runtime API calls and `assert()`-based checks
instead of an external unit-test framework.

Hot-path tuning must include a benchmark or measurable regression test.

Initial benchmark priorities are selector lookup, class lookup, dispatch-table
lookup, ARC / weak operations and associated-object access.
## Automation

Every push and pull request builds and tests the Windows runtime in Debug and
Release with LLVM 23.1.1. A daily workflow publishes a rolling `nightly`
prerelease, while pushing a `v*` tag creates a versioned GitHub release after
the Release test suite passes.

The PowerShell commands under `scripts/ci/` are shared between local builds and
GitHub Actions so CI failures can be reproduced without workflow-only logic.
See `docs/CI.md` for the complete automation and release policy.

## Changelog

Current CTRLZer0 development history is maintained in `CHANGELOG.md`. The
historical `ANNOUNCE*` files are archived unchanged for provenance and are not
used as the project's changelog.

## Licensing and provenance

See `COPYING`, `LICENSE-CTRLZERO` and `NOTICE.md`.

Inherited libobjc2 code retains its original MIT terms and copyright notices.
New CTRLZer0-owned files are AGPL-3.0-only when explicitly marked. In inherited
files substantially modified by CTRLZer0, SPDX headers may identify the file as
`MIT AND AGPL-3.0-only`: original portions remain MIT while CTRLZer0 additions
are licensed under AGPL-3.0-only.
