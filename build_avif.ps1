#!/usr/bin/env pwsh
# Build script for libavif with SVT-AV1 codec
# This script configures and builds libavif with the correct settings for ReShade

param(
    [switch]$Clean,
    [switch]$Help
)

if ($Help) {
    Write-Host "Usage: .\build_avif.ps1 [-Clean] [-Help]"
    Write-Host ""
    Write-Host "Options:"
    Write-Host "  -Clean    Clean build directory before building"
    Write-Host "  -Help     Show this help message"
    Write-Host ""
    Write-Host "This script builds libavif with SVT-AV1 codec for ReShade integration."
    exit 0
}

# Set error action preference
$ErrorActionPreference = "Stop"

# Get the script directory
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$LibAvifDir = Join-Path $ScriptDir "deps\libavif"
$BuildDir = Join-Path $LibAvifDir "build"

Write-Host "Building libavif for ReShade..." -ForegroundColor Green
Write-Host "Script directory: $ScriptDir"
Write-Host "Libavif directory: $LibAvifDir"
Write-Host "Build directory: $BuildDir"

# Check if libavif directory exists
if (-not (Test-Path $LibAvifDir)) {
    Write-Error "libavif directory not found at: $LibAvifDir"
    Write-Host "Please ensure the libavif submodule is initialized:"
    Write-Host "  git submodule update --init --recursive"
    exit 1
}

# Clean build directory if requested
if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir
}

# Create build directory
if (-not (Test-Path $BuildDir)) {
    Write-Host "Creating build directory..." -ForegroundColor Yellow
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

# Change to build directory
Push-Location $BuildDir

try {
    Write-Host "Configuring CMake..." -ForegroundColor Yellow

    # CMake configuration with SVT-AV1 codec and static runtime
    $cmakeArgs = @(
        "-G", "Visual Studio 17 2022",
        "-A", "x64",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DBUILD_SHARED_LIBS=ON",
        "-DAVIF_CODEC_SVT=LOCAL",
        "-DAVIF_CODEC_AOM=OFF",
        "-DAVIF_CODEC_DAV1D=OFF",
        "-DAVIF_CODEC_LIBGAV1=OFF",
        "-DAVIF_CODEC_RAV1E=OFF",
        "-DAVIF_LIBYUV=OFF",
        "-DAVIF_BUILD_APPS=OFF",
        "-DAVIF_BUILD_TESTS=OFF",
        "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded",
        ".."
    )

    Write-Host "CMake command: cmake $($cmakeArgs -join ' ')"
    & cmake @cmakeArgs

    if ($LASTEXITCODE -ne 0) {
        throw "CMake configuration failed with exit code $LASTEXITCODE"
    }

    Write-Host "Building libavif..." -ForegroundColor Yellow

    # Build the project
    & cmake --build . --config Release --parallel

    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE"
    }

    Write-Host "Build completed successfully!" -ForegroundColor Green

    # Show build results
    $avifDll = Join-Path $BuildDir "Release\avif.dll"
    $avifLib = Join-Path $BuildDir "Release\avif.lib"

    if (Test-Path $avifDll) {
        $dllSize = [math]::Round((Get-Item $avifDll).Length / 1MB, 2)
        Write-Host "Generated files:" -ForegroundColor Cyan
        Write-Host "  avif.dll: $dllSize MB" -ForegroundColor White
    }

    if (Test-Path $avifLib) {
        Write-Host "  avif.lib: $(Get-Item $avifLib | Select-Object -ExpandProperty Length) bytes" -ForegroundColor White
    }

    Write-Host ""
    Write-Host "Next steps:" -ForegroundColor Cyan
    Write-Host "1. Copy avif.dll to ReShade output directory:" -ForegroundColor White
    Write-Host "   Copy-Item `"$avifDll`" `"bin\x64\Release\`"" -ForegroundColor Gray
    Write-Host "2. Rebuild ReShade to link against the new avif.lib" -ForegroundColor White

} catch {
    Write-Error "Build failed: $($_.Exception.Message)"
    exit 1
} finally {
    Pop-Location
}

Write-Host "libavif build script completed." -ForegroundColor Green
