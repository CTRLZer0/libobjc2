# Provenance and licensing

This repository is maintained independently by CTRLZer0 and contains code
derived from the GNUstep Objective-C runtime and Microsoft's libobjc2 port.

The historical runtime sources are MIT licensed. Their original copyright and
license terms are preserved in `COPYING` and in the source history.

CTRLZer0 has added Windows host-runtime integration for Mosaic, including a
loader-independent initialization path, C-dispatch support for host-side ARC
operations, a Mosaic-specific build entry point, public host integration APIs,
and Windows runtime contract tests.

New files that are original CTRLZer0 work carry an explicit
`SPDX-License-Identifier: AGPL-3.0-only` marker and are governed by
`LICENSE-CTRLZERO`.

A change to an existing MIT-licensed source file does not by itself change that
file's license. Provenance and copyright notices must not be removed during
refactors or source relocation.
