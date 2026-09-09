# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
# Original CTRLZer0 work; see LICENSE-CTRLZERO and NOTICE.md for licensing
# and provenance details.
param(
    [string]$Version = "23.1.1",
    [string]$InstallRoot = "$env:RUNNER_TEMP\llvm"
)

$ErrorActionPreference = "Stop"
$target = Join-Path $InstallRoot "LLVM-$Version"
if (Test-Path $target) {
    $cached = Get-ChildItem -Path $target -Filter clang-cl.exe -Recurse -ErrorAction SilentlyContinue |
        Select-Object -First 1 -ExpandProperty FullName
    if ($cached) {
        $cachedVersion = & $cached --version | Select-Object -First 1
        if ($cachedVersion -match [regex]::Escape($Version)) {
            Write-Host "Using cached $cachedVersion"
            if ($env:GITHUB_OUTPUT) {
                "clang_cl=$cached" | Out-File -FilePath $env:GITHUB_OUTPUT -Append -Encoding utf8
            }
            $cached
            exit 0
        }
    }
}

$tag = "llvmorg-$Version"
$api = "https://api.github.com/repos/llvm/llvm-project/releases/tags/$tag"
$headers = @{ "User-Agent" = "CTRLZer0-libobjc2-CI" }
if ($env:GITHUB_TOKEN) {
    $headers.Authorization = "Bearer $env:GITHUB_TOKEN"
}

Write-Host "Resolving LLVM $Version from the official llvm-project release..."
$release = Invoke-RestMethod -Uri $api -Headers $headers
$asset = $release.assets | Where-Object {
    $_.name -match "^LLVM-$([regex]::Escape($Version))-win64\.msi$"
} | Select-Object -First 1
if (-not $asset) {
    throw "Official LLVM $Version Windows x64 MSI was not found in release $tag."
}

New-Item -ItemType Directory -Force -Path $InstallRoot | Out-Null
$msi = Join-Path $InstallRoot $asset.name
Invoke-WebRequest -Uri $asset.browser_download_url -Headers $headers -OutFile $msi

New-Item -ItemType Directory -Force -Path $target | Out-Null
$arguments = @('/a', "`"$msi`"", '/qn', "TARGETDIR=`"$target`"")
$process = Start-Process msiexec.exe -ArgumentList $arguments -Wait -PassThru
if ($process.ExitCode -ne 0) {
    throw "LLVM administrative install failed with exit code $($process.ExitCode)."
}
$clang = Get-ChildItem -Path $target -Filter clang-cl.exe -Recurse |
    Select-Object -First 1 -ExpandProperty FullName
if (-not $clang) {
    throw "clang-cl.exe was not found under $target."
}

$actual = & $clang --version | Select-Object -First 1
if ($actual -notmatch [regex]::Escape($Version)) {
    throw "Expected LLVM $Version but got: $actual"
}

Write-Host $actual
Write-Host "LLVM_CLANG_CL=$clang"
if ($env:GITHUB_OUTPUT) {
    "clang_cl=$clang" | Out-File -FilePath $env:GITHUB_OUTPUT -Append -Encoding utf8
}
$clang
