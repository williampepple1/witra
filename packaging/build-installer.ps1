# =============================================================================
#  build-installer.ps1
#
#  Windows packaging pipeline for Witra. Does:
#    1. Configure + build in Release (MinGW/Ninja).
#    2. Run windeployqt into dist\ so the app can run without Qt installed.
#    3. Invoke Inno Setup to produce build\installer\Witra-Setup-<ver>.exe.
#    4. Create portable zip archive.
#
#  Usage (from the repo root):
#      pwsh -File packaging\build-installer.ps1
#
#  Environment overrides:
#      $env:QT_DIR / $env:QT_ROOT_DIR / $env:Qt6_DIR
#                     - Qt Mingw prefix (or a path under it)
#      $env:MINGW_BIN / $env:IQTA_TOOLS
#                     - MinGW bin dir, or Qt Tools root that contains mingw*_64
#      $env:ISCC       - full path to ISCC.exe
#      $env:WITRA_VERSION - installer version (also accepted as /DMyAppVersion)
# =============================================================================

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
Set-Location $repo

function Step($msg) { Write-Host "`n=== $msg ===" -ForegroundColor Cyan }

# ------------------------------- Locate Qt -----------------------------------
function Resolve-QtMingwPrefix([string]$hint) {
    if (-not $hint) { return $null }
    $p = $hint.TrimEnd('\', '/')
    while ($p -and -not (Test-Path (Join-Path $p 'bin\windeployqt.exe'))) {
        $parent = Split-Path -Parent $p
        if (-not $parent -or $parent -eq $p) { return $null }
        $p = $parent
    }
    if (Test-Path (Join-Path $p 'bin\windeployqt.exe')) { return $p }
    return $null
}

if (-not $env:QT_DIR -or -not (Test-Path (Join-Path $env:QT_DIR 'bin\windeployqt.exe'))) {
    $fromEnv = @(
        (Resolve-QtMingwPrefix $env:QT_DIR),
        (Resolve-QtMingwPrefix $env:QT_ROOT_DIR),
        (Resolve-QtMingwPrefix $env:Qt6_DIR)
    ) | Where-Object { $_ } | Select-Object -First 1

    $candidates = @()
    if ($fromEnv) { $candidates += $fromEnv }
    $candidates += @(
        'C:\Qt\6.10.1\mingw_64',
        'C:\Qt\6.8.0\mingw_64',
        'C:\Qt\6.7.3\mingw_64',
        'C:\Qt\6.7.0\mingw_64',
        'C:\Qt\6.6.3\mingw_64',
        'C:\Qt\6.5.3\mingw_64'
    ) | Where-Object { Test-Path $_ }
    if (-not $candidates) {
        $candidates = Get-ChildItem 'C:\Qt' -Directory -ErrorAction SilentlyContinue |
            ForEach-Object { Join-Path $_.FullName 'mingw_64' } |
            Where-Object { Test-Path $_ }
    }
    if (-not $candidates) {
        throw "Could not locate a Qt mingw_64 install. Set `$env:QT_DIR or `$env:QT_ROOT_DIR first."
    }
    $env:QT_DIR = $candidates | Select-Object -First 1
}
Write-Host "Using Qt at: $env:QT_DIR"

# ------------------------------- Locate MinGW --------------------------------
if (-not $env:MINGW_BIN) {
    $toolRoots = @('C:\Qt\Tools', $env:IQTA_TOOLS) | Where-Object { $_ -and (Test-Path $_) }
    $mingw = Get-ChildItem $toolRoots -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -like 'mingw*_64' } |
        Sort-Object Name -Descending |
        Select-Object -First 1
    if ($mingw) { $env:MINGW_BIN = Join-Path $mingw.FullName 'bin' }
}
if ($env:MINGW_BIN -and (Test-Path $env:MINGW_BIN)) {
    $env:PATH = "$env:MINGW_BIN;$env:PATH"
}

# ------------------------------- Locate ISCC ---------------------------------
if (-not $env:ISCC) {
    $iscc = @(
        'C:\Program Files (x86)\Inno Setup 6\ISCC.exe',
        'C:\Program Files\Inno Setup 6\ISCC.exe'
    ) | Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $iscc) {
        throw "Could not find ISCC.exe. Install Inno Setup 6 or set `$env:ISCC."
    }
    $env:ISCC = $iscc
}
Write-Host "Using ISCC: $env:ISCC"

# ------------------------------- Get Version ---------------------------------
$version = $env:WITRA_VERSION
if (-not $version) {
    # Try to extract from CMakeLists.txt
    $cmakeContent = Get-Content (Join-Path $repo 'CMakeLists.txt') -Raw
    if ($cmakeContent -match 'project\s*\(\s*witra\s+VERSION\s+(\d+\.\d+\.\d+)') {
        $version = $Matches[1]
    } else {
        $version = '1.0.0'
    }
}
Write-Host "Version: $version"

# --------------------------- 1. Configure + build ----------------------------
Step '1/4  Configuring + building (Release)'
$build = Join-Path $repo 'build'
if (-not (Test-Path (Join-Path $build 'CMakeCache.txt'))) {
    & cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release `
        -DCMAKE_PREFIX_PATH="$env:QT_DIR/lib/cmake" `
        "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
}
& cmake --build build --config Release -j

# --------------------------- 2. windeployqt ----------------------------------
Step '2/4  Deploying Qt runtime into dist\'
$dist = Join-Path $repo 'dist'

if (Test-Path $dist) {
    Remove-Item -Recurse -Force $dist
}
New-Item -ItemType Directory -Force -Path $dist | Out-Null

# Find built executable
$exePath = Join-Path $build 'witra.exe'
if (-not (Test-Path $exePath)) {
    $exePath = Get-ChildItem $build -Recurse -Filter 'witra.exe' -ErrorAction SilentlyContinue |
        Select-Object -First 1 -ExpandProperty FullName
}
if (-not $exePath -or -not (Test-Path $exePath)) {
    throw "Could not find witra.exe under $build"
}

Copy-Item $exePath $dist -Force

$windeployqt = Join-Path $env:QT_DIR 'bin\windeployqt.exe'
if (-not (Test-Path $windeployqt)) {
    throw "windeployqt not found at $windeployqt"
}
& $windeployqt --release --compiler-runtime (Join-Path $dist 'witra.exe')

# --------------------------- 3. Compile installer ----------------------------
Step '3/4  Compiling Inno Setup installer'
$installerDir = Join-Path $build 'installer'
New-Item -ItemType Directory -Force -Path $installerDir | Out-Null

# Use the installer script - prefer packaging folder, fall back to root
$iss = Join-Path $repo 'packaging\witra-installer.iss'
if (-not (Test-Path $iss)) {
    $iss = Join-Path $repo 'installer.iss'
}
if (-not (Test-Path $iss)) {
    throw "Could not find installer.iss"
}

$isccArgs = @($iss)
if ($version) {
    $isccArgs = @("/DMyAppVersion=$version") + $isccArgs
}
& $env:ISCC @isccArgs

# --------------------------- 4. Create portable zip --------------------------
Step '4/4  Creating portable zip'
$zipPath = Join-Path $installerDir "Witra-$version-portable.zip"
if (Test-Path $zipPath) {
    Remove-Item $zipPath -Force
}
Compress-Archive -Path (Join-Path $dist '*') -DestinationPath $zipPath

# Summary
$installer = Get-ChildItem (Join-Path $installerDir 'Witra-Setup-*.exe') | Select-Object -First 1
$portable = Get-ChildItem (Join-Path $installerDir 'Witra-*-portable.zip') | Select-Object -First 1

Write-Host "`n=== Packaging Complete ===" -ForegroundColor Green
if ($installer) {
    Write-Host "Installer: $($installer.FullName)"
    Write-Host ("  Size: {0:N2} MB" -f ($installer.Length / 1MB))
}
if ($portable) {
    Write-Host "Portable:  $($portable.FullName)"
    Write-Host ("  Size: {0:N2} MB" -f ($portable.Length / 1MB))
}
