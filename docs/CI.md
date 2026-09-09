<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders. -->
<!-- CTRLZer0 work is licensed under MIT; see COPYING and NOTICE.md. -->
# Continuous integration and releases

The repository uses the same PowerShell entry points locally and in GitHub
Actions. CI-specific behavior should remain in `.github/workflows`; compiler,
build, test and packaging behavior belongs in `scripts/ci`.

## Per-commit CI

`.github/workflows/ci.yml` runs on every push, pull request and manual dispatch.
The Windows runtime is built and tested in both Debug and Release using LLVM
23.1.1. A commit is considered runtime-green only when all CTest contracts pass.

Local equivalent:

```powershell
./scripts/ci/build-windows.ps1 `
  -ClangCl C:\path\to\clang-cl.exe `
  -Configuration Release
```
## Nightly

`.github/workflows/nightly.yml` runs once per day and can also be dispatched
manually. It builds and tests Release, packages the public headers and static
runtime library, calculates SHA-256, uploads a workflow artifact and refreshes
the rolling `nightly` prerelease.

The `nightly` tag is intentionally movable and always identifies the commit
used for the current nightly package.

## Versioned releases

Pushing a `v*` tag triggers `.github/workflows/release.yml`. A versioned release
is published only after the Release build and complete Windows contract suite
pass. The workflow uploads the Windows x64 ZIP and its SHA-256 checksum.

Release assets contain the public `objc/` API, `mosaic_objc_runtime.lib`, build
metadata, README/changelog and all applicable license / provenance files.
