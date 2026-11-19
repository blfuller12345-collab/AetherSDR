<#
.SYNOPSIS
    Download ONNX Runtime prebuilt binaries for Windows x64.

.DESCRIPTION
    Downloads the official ONNX Runtime 1.18.x prebuilt release from GitHub
    and places headers / import library / DLL in third_party/onnxruntime/
    ready for CMake (find_library / find_path).

    Required only for the S_History_v2-AI CNN signal classifier.
    AetherSDR compiles and runs without it — ONNX is an optional enhancement.

.EXAMPLE
    .\setup-onnxruntime.ps1
#>

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\_verify_sha256.ps1"

# Pinned to the ONNX Runtime that sherpa-onnx bundles so a build with both shares
# one runtime (no symbol-version clash). Keep in sync with setup-onnxruntime.sh.
$OrtVersion = "1.27.0"
$OrtUrl     = "https://github.com/microsoft/onnxruntime/releases/download/v${OrtVersion}/onnxruntime-win-x64-${OrtVersion}.zip"
# SHA256 of the release zip. Bump alongside the version.
$OrtSha256  = "c5c81710938e68079ff1a192b04897faabe4b43830d48f39f27ecd4e16138bfc"
$OutDir     = "third_party\onnxruntime"
$ZipFile    = "third_party\onnxruntime-win-x64-${OrtVersion}.zip"

# ── Check if already set up ──────────────────────────────────────────────
if (Test-Path "$OutDir\lib\onnxruntime.lib") {
    Write-Host "ONNX Runtime already set up in $OutDir" -ForegroundColor Green
    exit 0
}

# ── Create directories ───────────────────────────────────────────────────
New-Item -ItemType Directory -Force -Path "third_party" | Out-Null
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
New-Item -ItemType Directory -Force -Path "$OutDir\include" | Out-Null
New-Item -ItemType Directory -Force -Path "$OutDir\lib" | Out-Null
New-Item -ItemType Directory -Force -Path "$OutDir\bin" | Out-Null

# ── Download ─────────────────────────────────────────────────────────────
# w5blf-custom: if the GitHub release can't be fetched (it returned 404 on
# 2026-11-19), fall back to the same version from Microsoft's NuGet package.
$UseNuGet = $false
if (-not (Test-Path $ZipFile)) {
    Write-Host "Downloading ONNX Runtime ${OrtVersion} prebuilt package..." -ForegroundColor Cyan
    try {
        Invoke-WebRequest -Uri $OrtUrl -OutFile $ZipFile
        Confirm-Sha256 -Path $ZipFile -Expected $OrtSha256
    } catch {
        Write-Warning "GitHub download failed ($($_.Exception.Message)); using NuGet Microsoft.ML.OnnxRuntime ${OrtVersion}."
        if (Test-Path $ZipFile) { Remove-Item -Force $ZipFile }
        $UseNuGet = $true
    }
}

if ($UseNuGet) {
    $NupkgUrl  = "https://api.nuget.org/v3-flatcontainer/microsoft.ml.onnxruntime/${OrtVersion}/microsoft.ml.onnxruntime.${OrtVersion}.nupkg"
    $NupkgFile = "third_party\microsoft.ml.onnxruntime.${OrtVersion}.zip"
    Invoke-WebRequest -Uri $NupkgUrl -OutFile $NupkgFile
    $tempDir = "third_party\onnxruntime-temp"
    if (Test-Path $tempDir) { Remove-Item -Recurse -Force $tempDir }
    Expand-Archive -Path $NupkgFile -DestinationPath $tempDir -Force
    $native = "$tempDir\runtimes\win-x64\native"
    if (-not (Test-Path "$native\onnxruntime.lib") -or -not (Test-Path "$native\onnxruntime.dll")) {
        Write-Error "NuGet package layout unexpected: $native has no onnxruntime.lib/.dll"
        exit 1
    }
    # No pinned hash for the .nupkg, so require Microsoft's Authenticode
    # signature on the DLL instead.
    $sig = Get-AuthenticodeSignature "$native\onnxruntime.dll"
    if ($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Subject -notmatch 'O=Microsoft Corporation') {
        Write-Error "onnxruntime.dll from NuGet is not validly signed by Microsoft (status: $($sig.Status))"
        exit 1
    }
    Write-Host "NuGet onnxruntime.dll signature: $($sig.Status), $($sig.SignerCertificate.Subject)"
    Copy-Item "$tempDir\build\native\include\*" "$OutDir\include\" -Recurse -Force
    Copy-Item "$native\onnxruntime.lib" "$OutDir\lib\onnxruntime.lib"
    Get-ChildItem $native -Filter "*.dll" | ForEach-Object { Copy-Item $_.FullName "$OutDir\bin\$($_.Name)" }
    Remove-Item -Recurse -Force $tempDir
    Remove-Item -Force $NupkgFile
    Write-Host "ONNX Runtime ${OrtVersion} (NuGet) ready in $OutDir" -ForegroundColor Green
    exit 0
}

# ── Extract ──────────────────────────────────────────────────────────────
Write-Host "Extracting..." -ForegroundColor Cyan
$tempDir = "third_party\onnxruntime-temp"
if (Test-Path $tempDir) { Remove-Item -Recurse -Force $tempDir }
Expand-Archive -Path $ZipFile -DestinationPath $tempDir -Force

$srcDir = Get-ChildItem "$tempDir\onnxruntime-win-x64-*" -Directory | Select-Object -First 1
if (-not $srcDir) {
    Write-Error "Could not find extracted onnxruntime directory in $tempDir"
    exit 1
}

# ── Copy headers ─────────────────────────────────────────────────────────
# ONNX Runtime packages keep headers directly in include/
Copy-Item "$($srcDir.FullName)\include\*" "$OutDir\include\" -Recurse -Force

# ── Copy lib / DLL ───────────────────────────────────────────────────────
# The prebuilt package puts .lib and .dll together in lib/
$libFile = Get-ChildItem "$($srcDir.FullName)\lib" -Filter "onnxruntime.lib" | Select-Object -First 1
$dllFiles = Get-ChildItem "$($srcDir.FullName)\lib" -Filter "*.dll"

if (-not $libFile) {
    Write-Error "onnxruntime.lib not found in extracted package"
    exit 1
}

Copy-Item $libFile.FullName "$OutDir\lib\onnxruntime.lib"
foreach ($dll in $dllFiles) {
    Copy-Item $dll.FullName "$OutDir\bin\$($dll.Name)"
}

# ── Cleanup ──────────────────────────────────────────────────────────────
Remove-Item -Recurse -Force $tempDir
Remove-Item -Force $ZipFile

Write-Host "ONNX Runtime ${OrtVersion} ready in $OutDir" -ForegroundColor Green
Write-Host "  Headers: $OutDir\include\"
Write-Host "  Lib:     $OutDir\lib\onnxruntime.lib"
Write-Host "  DLLs:    $OutDir\bin\"
Write-Host ""
Write-Host "Re-run CMake to pick up ONNX Runtime:" -ForegroundColor Yellow
Write-Host "  cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo"
