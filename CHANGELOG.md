# Changelog

Notable CTRLZer0-specific changes are documented here. Historical GNUstep
release announcements remain under `docs/archive/upstream/releases/`.

## Unreleased

### Added

- Current GNUstep libobjc2 history as the semantic runtime base.
- Explicit `mosaic_objc_runtime_initialize()` embedding API.
- `Mosaic::ObjCRuntime` CMake integration backed by the complete GNUstep runtime.
- Dedicated Windows contracts under `tests/windows/`.
- Runtime hot-path benchmarks under `benchmarks/windows/`.
- Reproducible LLVM 23 Windows build, test, package, nightly and release scripts.
- Static-runtime consumer visibility mode for Windows.
- Generated `objc-config.h` propagation in Mosaic builds and release packages.
- A history bridge preserving the earlier CTRLZer0 / Microsoft-derived lineage.

### Changed

- Embedded Blocks runtime now uses modern atomic refcounting and transactional concurrent `__block` forwarding while preserving the existing Blocks ABI.
- Legacy hopscotch hash-table allocation and resize paths now use overflow-safe sizing and integer load-factor checks.
- Class lookup now uses a generation-invalidated direct-mapped TLS cache that preserves content validation while accelerating common Mosaic working sets.
- Core runtime development now follows current GNUstep instead of the historical
  Microsoft snapshot.
- GNUstep implementation sources are reorganized into the CTRLZer0 subsystem
  layout under `src/`, with private headers under `src/internal/`.
- Windows shared and static runtimes can be built simultaneously without output
  collisions.
- The static target now includes native message-send and block-trampoline
  assembly objects just like the shared target.
- Windows CRT portability helpers remove LLVM 23 deprecation diagnostics without
  warning suppression.
- CI treats runtime and CTRLZer0 contract warnings as errors.
- Root documentation now describes the CTRLZer0 project while preserving the
  imported GNUstep README under `docs/archive/upstream/`.
- GNUstep compatibility CI is retained alongside the Mosaic-specific Windows
  gate so both upstream behavior and integration behavior remain covered.

### Fixed

- Hash-table removal of a missing key now releases the table lock, and inserting through `table_set` no longer dereferences a missing cell.
- `objc_disposeClassPair()` now handles dynamically-created root classes without dereferencing a nil superclass.
- `class_addIvar()` now initializes ivar size and ownership flags before applying
  alignment metadata, preventing uninitialized bits from turning dynamic ivars
  into accidental strong or weak references.
- Dynamic protocol adoption allocates the correct `objc_protocol_list` layout.
- Protocol copy APIs are null-safe and initialize output counts consistently.
- Protocol lookup accepts semantically equivalent typed and untyped selectors.
- Dynamic protocol allocation validates names and allocation failures.
- Associated-object and weak-reference contracts now validate the modern GNUstep
  ARC semantics instead of depending on the removed legacy implementation.
- Windows static and shared runtime outputs no longer overwrite the same import
  library name.
