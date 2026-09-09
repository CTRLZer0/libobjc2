# Test suite

The test suite is intentionally lightweight and follows the runtime's existing
coding style. Tests are small C / Objective-C executables that exercise public
or internal runtime contracts directly and use `assert()` for validation.

New tests should be grouped by runtime behavior rather than by implementation
file. The long-term layout is:

- `runtime/` — classes, metaclasses, ivars, protocols and metadata loading.
- `dispatch/` — selectors, lookup, forwarding and dispatch tables.
- `memory/` — ARC, weak references, autorelease and associated objects.
- `blocks/` — Blocks runtime and block-to-IMP behavior.
- `exceptions/` — Objective-C / C++ exception interoperability.
- `windows/` — host contracts required by Mosaic on Windows.

Existing tests are being migrated incrementally to these groups without
changing their coding conventions or behavior.
