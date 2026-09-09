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
- Central source manifest shared by supported build entry points.

### Changed

- Repository detached from its historical fork network and maintained directly
  by CTRLZer0.
- Runtime implementations reorganized into subsystem directories under `src/`.
- Test sources centralized under `tests/`.
- Historical upstream documentation moved into `docs/archive/upstream/`.
- LLVM 23.1.1 selected as the target compiler baseline.
- Runtime typing updated for clean compilation across Clang 20 and LLVM 23.1.1.
