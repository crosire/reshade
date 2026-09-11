$path = "$PSScriptRoot\.."

# Find suitable Visual Studio 2017-2022 installation
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    $devenv = & $vswhere -latest -products * -version "[15.0,18.0)" -find "Common7\IDE\devenv.com"
}
if (-not $devenv) {
    $devenv = "devenv.com"
}

# Get product version
& "$PSScriptRoot\update_version.ps1" "$path\res\version.h"
$version = "$($global:ReShadeVersion[0]).$($global:ReShadeVersion[1]).$($global:ReShadeVersion[2])"
$official = Test-Path "$path\res\sign.pfx"

# Rebuild solution for all configurations
"Building ReShade $version release ..."

& $devenv "$path\ReShade.sln" /Rebuild "Release|32-bit" /Project "ReShade" /ProjectConfig "Release|Win32"
& $devenv "$path\ReShade.sln" /Rebuild "Release|64-bit" /Project "ReShade" /ProjectConfig "Release|x64"
& $devenv "$path\ReShade.sln" /Rebuild "Release Setup|32-bit" /Project "ReShade Setup" /ProjectConfig "Release|Any CPU"
if ($official) {
    & $devenv "$path\ReShade.sln" /Rebuild "Release|32-bit" /Project "ReShade" /ProjectConfig "Release Signed|Win32"
    & $devenv "$path\ReShade.sln" /Rebuild "Release|64-bit" /Project "ReShade" /ProjectConfig "Release Signed|x64"
    & $devenv "$path\ReShade.sln" /Rebuild "Release Setup|32-bit" /Project "ReShade Setup" /ProjectConfig "Release Signed|Any CPU"
}

# Rename output files using the product version
Copy-Item "$path\bin\AnyCPU\Release\ReShade Setup.exe" "ReShade_Setup_$($version)_Addon.exe"
if ($official) {
    Copy-Item "$path\bin\AnyCPU\Release Signed\ReShade Setup.exe" "ReShade_Setup_$($version).exe"
}
