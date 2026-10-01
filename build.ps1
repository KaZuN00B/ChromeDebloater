# build.ps1 — Build ChromeDebloater.exe from source
# Run this script from ChromeDebloater project root

param([switch]$Clean)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot

Write-Host "`n============================================" -ForegroundColor Cyan
Write-Host "  ChromeDebloater — Build Script" -ForegroundColor Cyan
Write-Host "============================================`n" -ForegroundColor Cyan

# Check pyinstaller
try {
    $pyi = pyinstaller --version 2>&1
    Write-Host "[+] PyInstaller version: $pyi" -ForegroundColor Green
} catch {
    Write-Host "[!] PyInstaller not found. Installing..." -ForegroundColor Yellow
    python -m pip install pyinstaller --quiet
}

# Clean previous build
if ($Clean -or (Test-Path "$root\dist\ChromeDebloater.exe")) {
    Write-Host "[*] Cleaning previous build..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force "$root\dist", "$root\build" -ErrorAction SilentlyContinue
}

# Run pyinstaller
Write-Host "[*] Running PyInstaller..." -ForegroundColor Green
Set-Location $root
pyinstaller ChromeDebloater.spec --clean --noconfirm

if (Test-Path "$root\dist\ChromeDebloater.exe") {
    $size = [math]::Round((Get-Item "$root\dist\ChromeDebloater.exe").Length / 1MB, 1)
    Write-Host "`n[+] Build SUCCESS: dist\ChromeDebloater.exe ($size MB)" -ForegroundColor Green
    Write-Host "    Double-click to run — UAC elevation fires automatically.`n" -ForegroundColor Gray
} else {
    Write-Host "`n[X] Build FAILED — check output above." -ForegroundColor Red
    exit 1
}
