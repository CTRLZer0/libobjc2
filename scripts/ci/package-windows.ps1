# SPDX-License-Identifier: MIT
# Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
# CTRLZer0 work is licensed under MIT; see COPYING and NOTICE.md for licensing
# and provenance details.
param(
    [Parameter(Mandatory = $true)][string]$BuildDir,
    [string]$Version = "dev",
    [string]$OutputDir = "out/packages"
)

$ErrorActionPreference = "Stop"
$repo = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
$build = (Resolve-Path $BuildDir).Path
$output = if ([IO.Path]::IsPathRooted($OutputDir)) {
    $OutputDir
} else {
    Join-Path $repo $OutputDir
}

$library = Join-Path $build "mosaic_objc_runtime.lib"
if (-not (Test-Path $library)) {
    throw "Runtime library not found: $library"
}

$name = "libobjc2-$Version-windows-x64"
$stage = Join-Path $output $name
Remove-Item -Recurse -Force $stage -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path (Join-Path $stage "lib") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $stage "include") | Out-Null
Copy-Item $library (Join-Path $stage "lib\mosaic_objc_runtime.lib")
Copy-Item -Recurse (Join-Path $repo "objc") (Join-Path $stage "include\objc")

foreach ($file in @("README.md", "CHANGELOG.md", "COPYING", "NOTICE.md")) {
    Copy-Item (Join-Path $repo $file) (Join-Path $stage $file)
}

$commit = (& git -C $repo rev-parse HEAD).Trim()
$metadata = @(
    "version=$Version",
    "commit=$commit",
    "platform=windows-x64",
    "library=mosaic_objc_runtime.lib"
)
$metadata | Set-Content -Path (Join-Path $stage "BUILD-INFO.txt") -Encoding utf8

New-Item -ItemType Directory -Force -Path $output | Out-Null
$zip = Join-Path $output "$name.zip"
Remove-Item -Force $zip -ErrorAction SilentlyContinue
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip -CompressionLevel Optimal
$hash = Get-FileHash -Algorithm SHA256 $zip
$checksum = Join-Path $output "$name.sha256"
"$($hash.Hash.ToLower())  $([IO.Path]::GetFileName($zip))" |
    Set-Content -Path $checksum -Encoding ascii

Write-Host "PACKAGE=$zip"
Write-Host "CHECKSUM=$checksum"
if ($env:GITHUB_OUTPUT) {
    "package=$zip" | Out-File -FilePath $env:GITHUB_OUTPUT -Append -Encoding utf8
    "checksum=$checksum" | Out-File -FilePath $env:GITHUB_OUTPUT -Append -Encoding utf8
}
