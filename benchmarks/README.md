# Benchmarks

Performance changes to runtime hot paths should be accompanied by a benchmark
that measures the affected operation in isolation and records enough context to
compare compiler and build configurations.

Initial benchmark targets:

- selector registration and repeated selector lookup;
- class registration and class lookup;
- dispatch-table lookup on monomorphic and polymorphic selectors;
- retain / release and weak-reference operations;
- associated-object get / set;
- protocol and ivar lookup.

Benchmarks should use the same low-level C / Objective-C style as the runtime
and tests. A benchmark is not a correctness test; every optimized behavior must
also remain covered by `tests/`.

## Windows hot paths

Configure the Mosaic runtime with `MOSAIC_LIBOBJC2_BUILD_BENCHMARKS=ON` or
pass `-BuildBenchmarks` to `scripts/ci/build-windows.ps1`. The resulting
`mosaic_objc_runtime_hotpaths` executable measures `object_getClass`,
`object_setClass`, `objc_getClass`, and repeated selector registration.

The optional first argument controls the iteration count (default: 5,000,000).
Treat the output as a relative regression signal on the same machine and build
configuration, not as a cross-machine performance guarantee.
