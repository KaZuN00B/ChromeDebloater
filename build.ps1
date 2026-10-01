# ChromeDebloater Pro Native C++ Build Script
param(
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot

Write-Host "`n==================================================" -ForegroundColor Cyan
Write-Host "   ChromeDebloater Pro Native C++ Build Script    " -ForegroundColor Cyan
Write-Host "==================================================`n" -ForegroundColor Cyan

# Locate vcvars64.bat
$vcvars = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars)) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -property installationPath
        $vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
    }
}

if (-not (Test-Path $vcvars)) {
    Write-Host "[X] Visual Studio C++ build environment not found!" -ForegroundColor Red
    exit 1
}

$buildDir = Join-Path $root "build"
$distDir = Join-Path $root "dist"

if ($Clean) {
    Write-Host "[*] Cleaning build directories..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $buildDir, $distDir -ErrorAction SilentlyContinue
}

if (-not (Test-Path $buildDir)) { New-Item -ItemType Directory -Path $buildDir | Out-Null }
if (-not (Test-Path $distDir)) { New-Item -ItemType Directory -Path $distDir | Out-Null }

$srcDir = Join-Path $root "src"
$appRc = Join-Path $srcDir "app.rc"
$appRes = Join-Path $buildDir "app.res"
$outExe = Join-Path $distDir "ChromeDebloater.exe"

# Compile resources
Write-Host "[*] Compiling application manifest and resources..." -ForegroundColor Green
$rcCmd = "call `"$vcvars`" >nul && cd /d `"$srcDir`" && rc /nologo /fo `"$appRes`" `"$appRc`""
cmd.exe /c $rcCmd
if ($LASTEXITCODE -ne 0) {
    Write-Host "[X] Resource compilation failed." -ForegroundColor Red
    exit 1
}

# Compile and link C++
Write-Host "[*] Compiling native C++ executable (O2 Optimized)..." -ForegroundColor Green
$sources = "`"$srcDir\main.cpp`" `"$srcDir\ui\window.cpp`" `"$srcDir\ui\render_utils.cpp`" `"$srcDir\engine\audit_engine.cpp`" `"$srcDir\engine\tweak_engine.cpp`" `"$srcDir\engine\backup_engine.cpp`""
$libs = "advapi32.lib shell32.lib user32.lib gdi32.lib gdiplus.lib comctl32.lib uxtheme.lib dwmapi.lib version.lib dnsapi.lib ole32.lib"
$clCmd = "call `"$vcvars`" >nul && cl /nologo /O2 /std:c++20 /EHsc /utf-8 /DUNICODE /D_UNICODE /MD /I`"$srcDir`" /Fo`"$buildDir\\`" $sources `"$appRes`" /link /SUBSYSTEM:WINDOWS /MANIFESTUAC:`"level='requireAdministrator' uiAccess='false'`" /OUT:`"$outExe`" $libs"

cmd.exe /c $clCmd
if ($LASTEXITCODE -ne 0) {
    Write-Host "[X] C++ Compilation failed." -ForegroundColor Red
    exit 1
}

if (Test-Path $outExe) {
    $item = Get-Item $outExe
    $sizeKb = [math]::Round($item.Length / 1KB, 1)
    Write-Host "`n[OK] SUCCESS: $outExe ($sizeKb KB)" -ForegroundColor Green
    Write-Host "     Pure native C++ executable. No Python, no Qt, no runtime dependencies." -ForegroundColor Gray
    Write-Host "     Double-click to run with administrator rights.`n" -ForegroundColor Gray
} else {
    Write-Host "[X] Output executable was not generated." -ForegroundColor Red
    exit 1
}
