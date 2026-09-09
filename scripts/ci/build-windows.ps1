# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
# Original CTRLZer0 work; see LICENSE-CTRLZERO and NOTICE.md for licensing
# and provenance details.
param(
    [Parameter(Mandatory = $true)][string]$ClangCl,
    [ValidateSet("Debug", "Release")][string]$Configuration = "Release",
    [string]$BuildDir = "out/ci-windows",
    [switch]$SkipTests,
    [switch]$WarningsAsErrors,
    [switch]$BuildBenchmarks
)

$ErrorActionPreference = "Stop"
$repo = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
$build = if ([IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir
} else {
    Join-Path $repo $BuildDir
}

$command = Get-Command vswhere.exe -ErrorAction SilentlyContinue
$candidates = @()
if ($command) { $candidates += $command.Source }
$candidates += "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
$candidates += "C:\Program Files\Microsoft Visual Studio\Installer\vswhere.exe"
$vswhere = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $vswhere) {
    throw "vswhere.exe was not found. Visual Studio Build Tools are required."
}
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) {
    throw "A Visual Studio installation with the C++ toolchain was not found."
}

$devcmd = Join-Path $vs "Common7\Tools\VsDevCmd.bat"
$environment = & cmd.exe /s /c "`"$devcmd`" -no_logo -arch=x64 -host_arch=x64 >nul && set"
foreach ($line in $environment) {
    $separator = $line.IndexOf('=')
    if ($separator -gt 0) {
        $name = $line.Substring(0, $separator)
        $value = $line.Substring($separator + 1)
        Set-Item -Path "Env:$name" -Value $value
    }
}

if (-not (Test-Path $ClangCl)) {
    throw "clang-cl not found: $ClangCl"
}
$clangPath = (Resolve-Path $ClangCl).Path
& $clangPath --version | Select-Object -First 1
New-Item -ItemType Directory -Force -Path $build | Out-Null

$cache = Join-Path $build "CMakeCache.txt"
if (Test-Path $cache) {
    $compilerEntry = Select-String -Path $cache -Pattern '^CMAKE_C_COMPILER:[^=]+=(.+)$' |
        Select-Object -First 1
    if ($compilerEntry) {
        $cachedCompiler = $compilerEntry.Matches[0].Groups[1].Value -replace '/', '\'
        $cachedCompiler = [IO.Path]::GetFullPath($cachedCompiler)
        if (-not [string]::Equals($cachedCompiler, $clangPath,
                [StringComparison]::OrdinalIgnoreCase)) {
            Write-Host "Compiler changed; resetting CMake cache: $cachedCompiler -> $clangPath"
            Remove-Item -Force $cache
            Remove-Item -Recurse -Force (Join-Path $build "CMakeFiles") -ErrorAction SilentlyContinue
        }
    }
}

$warnings = if ($WarningsAsErrors) { "ON" } else { "OFF" }
$benchmarks = if ($BuildBenchmarks) { "ON" } else { "OFF" }
& cmake -S (Join-Path $repo "msvc/mosaic") -B $build -G Ninja `
    "-DCMAKE_C_COMPILER=$clangPath" `
    "-DCMAKE_BUILD_TYPE=$Configuration" `
    "-DBUILD_TESTING=ON" `
    "-DMOSAIC_LIBOBJC2_WARNINGS_AS_ERRORS=$warnings" `
    "-DMOSAIC_LIBOBJC2_BUILD_BENCHMARKS=$benchmarks"
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }

& cmake --build $build --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw "Build failed." }

if (-not $SkipTests) {
    & ctest --test-dir $build -C $Configuration --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "CTest failed." }
}

Write-Host "BUILD_DIR=$build"
if ($env:GITHUB_OUTPUT) {
    "build_dir=$build" | Out-File -FilePath $env:GITHUB_OUTPUT -Append -Encoding utf8
}
