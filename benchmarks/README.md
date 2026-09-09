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
