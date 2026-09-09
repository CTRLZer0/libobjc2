# Changelog

All notable changes to the CTRLZer0 libobjc2 runtime are documented here.

The project does not reconstruct historical release notes into this changelog.
Original upstream announcements remain preserved under
`docs/archive/upstream/releases/`.

## Unreleased

### Added

- Standalone Windows x86-64 runtime path for Mosaic.
- Explicit `mosaic_objc_runtime_initialize()` host initialization API.
- Centralized runtime contract suite under `tests/windows/`.
- Dedicated `benchmarks/` area for performance regression coverage.
- Windows runtime hot-path benchmark for class access, class lookup and selector lookup.
- Central source manifest shared by supported build entry points.
- Reproducible PowerShell entry points for LLVM installation, Windows build,
  tests and release packaging.
- Per-push / pull-request CI plus automated nightly and versioned releases.
### Changed

- Repository detached from its historical fork network and maintained directly
  by CTRLZer0.
- Runtime implementations reorganized into subsystem directories under `src/`.
- Private runtime headers isolated under `src/internal/`.
- Test sources centralized under `tests/`.
- Historical upstream documentation moved into `docs/archive/upstream/`.
- LLVM 23.1.1 selected as the target compiler baseline.
- Runtime typing updated for clean compilation across Clang 20 and LLVM 23.1.1.
- Windows CRT portability and constant-expression cleanup removes legacy LLVM 23 diagnostics without suppressing warnings.
- Raw Objective-C class access now uses alias-safe internal accessors; block class symbols use consistent typing and legacy protocol root classes are explicit.
- Protocol2 static-library registration, dynamic protocol list growth and copy APIs are hardened and covered by Windows contract tests.
- LLVM 23 Windows CI treats runtime compiler warnings as errors, preventing diagnostic debt from being reintroduced.
- Project documentation now describes current CI / release behavior rather than
  relying on inherited release announcements.
