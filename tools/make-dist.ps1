# Build the release artifacts for the current version:
#   dist/poly-<ver>-setup.exe    (Inno Setup installer, needs ISCC.exe)
#   dist/poly-<ver>-win64.zip    (portable)
#   dist/SHA256SUMS.txt
# Usage:  powershell -NoProfile -ExecutionPolicy Bypass -File tools/make-dist.ps1
# NOTE: keep this file pure ASCII - PowerShell 5.1 reads .ps1 with the ANSI code page.
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$verLine = Select-String -Path (Join-Path $root 'src\version.h') -Pattern 'POLY_VERSION\s+"([^"]+)"'
if (-not $verLine) { throw 'POLY_VERSION not found in src/version.h' }
$ver = $verLine.Matches[0].Groups[1].Value
Write-Host "version = $ver"

$dist = Join-Path $root 'dist'
New-Item -ItemType Directory -Force $dist | Out-Null

# --- portable zip -----------------------------------------------------------
$zipPath = Join-Path $dist "poly-$ver-win64.zip"
$prefix  = "poly-$ver-win64/"
$files = @(
    @{ src = 'bin\poly.exe';        name = 'poly.exe' },
    @{ src = 'bin\poly-gui.exe';    name = 'poly-gui.exe' },
    @{ src = 'README.md';           name = 'README.md' },
    @{ src = 'CHANGELOG.md';        name = 'CHANGELOG.md' },
    @{ src = 'testlib\testlib.h';   name = 'testlib/testlib.h' }
)
foreach ($f in $files) {
    if (-not (Test-Path (Join-Path $root $f.src))) { throw "missing file: $($f.src)" }
}
if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::Open($zipPath, [System.IO.Compression.ZipArchiveMode]::Create)
try {
    foreach ($f in $files) {
        # forward slashes in entry names so the archive extracts the same on any OS
        $entry = $zip.CreateEntry($prefix + $f.name, [System.IO.Compression.CompressionLevel]::Optimal)
        $s = [System.IO.File]::OpenRead((Join-Path $root $f.src))
        $out = $entry.Open()
        try { $s.CopyTo($out) } finally { $out.Dispose(); $s.Dispose() }
    }
} finally { $zip.Dispose() }
Write-Host ("zip  : {0} ({1:N0} bytes)" -f $zipPath, (Get-Item $zipPath).Length)

# --- installer --------------------------------------------------------------
$iscc = Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe'
if (-not (Test-Path $iscc)) {
    $cmd = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    if ($cmd) { $iscc = $cmd.Source }
}
if (Test-Path $iscc) {
    & $iscc '/Qp' (Join-Path $root 'installer\poly.iss') | Out-Null
    $setup = Join-Path $dist "poly-$ver-setup.exe"
    if (-not (Test-Path $setup)) { throw "installer not produced: $setup" }
    Write-Host ("setup: {0} ({1:N0} bytes)" -f $setup, (Get-Item $setup).Length)
} else {
    Write-Warning 'ISCC.exe not found - skipped the installer'
}

# --- checksums --------------------------------------------------------------
$sums = Join-Path $dist 'SHA256SUMS.txt'
$lines = @()
foreach ($name in @("poly-$ver-setup.exe", "poly-$ver-win64.zip")) {
    $p = Join-Path $dist $name
    if (Test-Path $p) {
        $h = (Get-FileHash $p -Algorithm SHA256).Hash.ToLower()
        $lines += "$h  $name"
    }
}
[System.IO.File]::WriteAllLines($sums, $lines, (New-Object System.Text.UTF8Encoding($false)))
Write-Host "sums : $sums"
$lines | ForEach-Object { Write-Host $_ }
