# Run the Witra verification suite with Qt/MinGW on PATH so
# test_witra.exe can start and print results instead of exiting silently.
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot

function Resolve-QtBin([string]$hint) {
    if (-not $hint) { return $null }
    $p = $hint.TrimEnd('\', '/')
    while ($p -and -not (Test-Path (Join-Path $p 'Qt6Core.dll')) -and -not (Test-Path (Join-Path $p 'bin\Qt6Core.dll'))) {
        $parent = Split-Path -Parent $p
        if (-not $parent -or $parent -eq $p) { return $null }
        $p = $parent
    }
    if (Test-Path (Join-Path $p 'Qt6Core.dll')) { return $p }
    if (Test-Path (Join-Path $p 'bin\Qt6Core.dll')) { return (Join-Path $p 'bin') }
    return $null
}

$qtBin = @(
    (Resolve-QtBin $env:QT_DIR),
    (Resolve-QtBin $env:QT_ROOT_DIR),
    (Resolve-QtBin $env:Qt6_DIR),
    'C:\Qt\6.10.1\mingw_64\bin',
    'C:\Qt\6.8.0\mingw_64\bin',
    'C:\Qt\6.7.3\mingw_64\bin'
) | Where-Object { $_ -and (Test-Path $_) } | Select-Object -First 1

if (-not $qtBin) {
    Write-Error "Could not find Qt6Core.dll. Set QT_DIR or install Qt, then rerun."
}

$mingwBin = $env:MINGW_BIN
if (-not $mingwBin) {
    $gpp = Get-ChildItem 'C:\Qt\Tools' -Recurse -Filter 'g++.exe' -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match 'mingw' } |
        Select-Object -First 1
    if ($gpp) { $mingwBin = $gpp.DirectoryName }
}

$prefix = @($mingwBin, $qtBin) | Where-Object { $_ }
$env:PATH = ($prefix -join ';') + ';' + $env:PATH

$exeCandidates = @(
    (Join-Path $repo 'build\test_witra.exe'),
    (Join-Path $repo 'build\Release\test_witra.exe'),
    (Join-Path $repo 'build\test_witra'),
    (Join-Path (Get-Location) 'test_witra.exe')
)
$exe = $exeCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $exe) {
    Write-Error "test_witra was not found. Build it first: cmake --build build --target test_witra"
}

Write-Host "Using Qt bin: $qtBin"
if ($mingwBin) { Write-Host "Using MinGW: $mingwBin" }
Write-Host "Running $exe"
Write-Host ""

& $exe
$code = $LASTEXITCODE
Write-Host ""
Write-Host "test_witra exited with code $code"
exit $code
