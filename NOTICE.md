# Provenance and licensing

This repository is maintained independently by CTRLZer0 and contains code
derived from the GNUstep Objective-C runtime and Microsoft's libobjc2 port.

Historical runtime code remains MIT licensed. Original copyright and license
terms are preserved in `COPYING`, source headers and repository history.

CTRLZer0 adds Windows host-runtime integration for Mosaic, loader-independent
initialization, portable host dispatch support, build integration, tests,
benchmarks and ongoing runtime modernization.

New files that are original CTRLZer0 work use
`SPDX-License-Identifier: AGPL-3.0-only` and are governed by
`LICENSE-CTRLZERO`.

Inherited files substantially modified by CTRLZer0 may use
`SPDX-License-Identifier: MIT AND AGPL-3.0-only`. In those files, inherited
portions remain under MIT while CTRLZer0 additions are licensed under
AGPL-3.0-only. Original attribution must not be removed during refactors.
