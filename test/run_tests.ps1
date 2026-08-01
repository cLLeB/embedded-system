# Builds and runs the core test suite, with MSVC if it is installed and MinGW g++
# if it is not. Either compiler proves the same thing; MSVC is preferred only
# because /W4 /WX is the stricter of the two default warning sets.
#
# Compiles ONLY SmartSocket/src/core - never src/hal - which is the check that the
# core really is free of Arduino.h. If someone adds an Arduino include to the core,
# this script stops compiling and the mistake is caught here rather than at
# migration time.
#
# Usage:  powershell -ExecutionPolicy Bypass -File test\run_tests.ps1

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$outDir = Join-Path $root 'build\test'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

# --- locate a compiler ---------------------------------------------------
# MSVC first, then MinGW g++. Nothing here is fatal until BOTH are missing:
# requiring MSVC specifically meant this script could not run at all on a machine
# that had a perfectly good g++ sitting in Program Files.
$vcvars = $null
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    $vsPath = & $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath
    if ($vsPath) {
        $candidate = Join-Path $vsPath 'VC\Auxiliary\Build\vcvars64.bat'
        if (Test-Path $candidate) { $vcvars = $candidate }
    }
}

$gxx = $null
if (-not $vcvars) {
    $gxxCandidates = @('C:\Program Files\CodeBlocks\MinGW\bin\g++.exe')
    $onPath = Get-Command g++ -ErrorAction SilentlyContinue
    if ($onPath) { $gxxCandidates += $onPath.Source }
    $gxx = $gxxCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}

if (-not $vcvars -and -not $gxx) {
    Write-Error ("No C++ compiler found. Install either Visual Studio Build Tools " +
                 "with the C++ workload, or MinGW-w64 (g++ on PATH).")
}

# --- sources -------------------------------------------------------------
$coreDir = Join-Path $root 'SmartSocket\src\core'
$testDir = Join-Path $root 'test'

$sources = @()
$sources += (Get-ChildItem $coreDir -Filter *.cpp | ForEach-Object { $_.FullName })
$sources += (Get-ChildItem $testDir -Filter *.cpp | ForEach-Object { $_.FullName })

$quoted = ($sources | ForEach-Object { '"' + $_ + '"' }) -join ' '
$exe = Join-Path $outDir 'run_tests.exe'

if ($vcvars) {
    Write-Host "Building test suite with MSVC ($($sources.Count) translation units)..." -ForegroundColor Cyan

    # /W4 /WX: warnings are errors. On an 8-bit target an unnoticed narrowing or
    # signed/unsigned slip is a field bug, so they are not allowed to accumulate.
    $clArgs = "/nologo /EHsc /W4 /WX /std:c++14 /D_CRT_SECURE_NO_WARNINGS " +
              "/Fo:`"$outDir\\`" /Fe:`"$exe`" $quoted"

    # vcvars must run in the same shell as cl, so both go through one cmd invocation.
    $cmd = "`"$vcvars`" >nul 2>&1 && cl $clArgs"
    $output = & cmd.exe /c $cmd 2>&1
    $buildOk = $LASTEXITCODE -eq 0
} else {
    Write-Host "Building test suite with g++ ($($sources.Count) translation units)..." -ForegroundColor Cyan
    Write-Host "  $gxx" -ForegroundColor DarkGray

    $gxxArgs = @('-std=c++14', '-Wall', '-Wextra', '-Werror',
                 '-I', (Join-Path $root 'SmartSocket\src\core')) +
               $sources + @('-o', $exe)
    $output = & $gxx $gxxArgs 2>&1
    $buildOk = $LASTEXITCODE -eq 0

    # The MinGW runtime DLLs live next to g++, not on the system PATH, so a binary
    # it links runs fine from a shell that can see them and exits with 0xC0000135
    # (DLL not found) from one that cannot. Put them on PATH for the run below.
    $env:PATH = (Split-Path -Parent $gxx) + ';' + $env:PATH
}

if (-not $buildOk) {
    Write-Host "BUILD FAILED" -ForegroundColor Red
    $output | ForEach-Object { Write-Host $_ }
    exit 1
}

# Surface warnings even on success; /WX means these should be rare.
$output | Where-Object { $_ -match 'warning' } | ForEach-Object {
    Write-Host $_ -ForegroundColor Yellow
}

Write-Host "Build OK. Running tests...`n" -ForegroundColor Green

& $exe
$testExit = $LASTEXITCODE

if ($testExit -eq 0) {
    Write-Host "`nALL TESTS PASSED" -ForegroundColor Green
} else {
    Write-Host "`nTESTS FAILED" -ForegroundColor Red
}
exit $testExit
