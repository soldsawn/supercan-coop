# SuperCan 1 Co-op Release uretici
# Kullanim: powershell -ExecutionPolicy Bypass -File tools\build_release.ps1 -Version 1.0.0
param(
    [string]$Version = "1.0.0",
    [switch]$SkipBuild
)
$ErrorActionPreference = "Stop"

$repo = Split-Path -Parent $PSScriptRoot
$srcDir = Join-Path $repo "src"
$pkgDir = Join-Path $repo "package"
$buildDll = Join-Path $repo "local\build\dinput8.dll"
$vendorSteam = Join-Path $repo "local\vendor\steam_api.dll"
$outDir = Join-Path $repo "local\release"
$expectedSteamHash = "FDAFB7C65EC7D4182B42BF92CC8671C68E4DC547CC6E613899D2618A47EB65DA"

Write-Host "=== SuperCan Co-op Release v$Version ===" -ForegroundColor Cyan

# 1) Derleme
if (-not $SkipBuild) {
    Write-Host "[1/5] dinput8.dll derleniyor (src\build.bat)..."
    & cmd /c (Join-Path $srcDir "build.bat")
    if ($LASTEXITCODE -ne 0) { throw "src\build.bat basarisiz oldu (exit=$LASTEXITCODE)" }
}
if (-not (Test-Path $buildDll)) { throw "dinput8.dll bulunamadi: $buildDll" }

# 2) steam_api.dll dogrulama
Write-Host "[2/5] steam_api.dll kontrol ediliyor..."
if (-not (Test-Path $vendorSteam)) {
    throw "steam_api.dll bulunamadi: $vendorSteam (Steamworks.NET 2025.164.1 x86 gereklidir)"
}
$steamHash = (Get-FileHash $vendorSteam -Algorithm SHA256).Hash
if ($steamHash -ne $expectedSteamHash) {
    throw "steam_api.dll SHA-256 uyusmuyor! Beklenen: $expectedSteamHash, Bulunan: $steamHash"
}

# 3) Staging klasorunu hazirla
Write-Host "[3/5] Release icerigi hazirlaniyor..."
$stage = Join-Path $outDir ("stage_v" + $Version)
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force $stage | Out-Null

$pkgFiles = @(
    "KUR.bat",
    "KALDIR.bat",
    "kur.ps1",
    "LOG_TOPLA.bat",
    "log_topla.ps1",
    "SuperCan_Coop_Oyna.bat",
    "SuperCan_Coop_Baslatici.ps1",
    "coop.ini",
    "guvenlik_duvari.bat",
    "KURULUM.txt"
)
foreach ($f in $pkgFiles) {
    $p = Join-Path $pkgDir $f
    if (-not (Test-Path $p)) { throw "Eksik paket dosyasi: $p" }
    Copy-Item $p $stage -Force
}
Copy-Item $buildDll $stage -Force
Copy-Item $vendorSteam $stage -Force
foreach ($doc in "LICENSE", "THIRD_PARTY_LICENSES.md", "CHANGELOG.md") {
    $p = Join-Path $repo $doc
    if (-not (Test-Path $p)) { throw "Eksik belge: $p" }
    Copy-Item $p $stage -Force
}

# 4) Otomatik yasakli dosya ve PII taramasi
Write-Host "[4/5] Yasakli dosya ve PII taramasi yapiliyor..."
$forbiddenExt = @(".exe", ".gen", ".wmb", ".wrs", ".orig", ".log", ".pdb", ".lib", ".exp", ".obj")
$piiRegex = '7656119\d{10}|C:\\Users\\[A-Za-z0-9._-]+\\'

Get-ChildItem $stage -Recurse -File | ForEach-Object {
    if ($forbiddenExt -contains $_.Extension.ToLowerInvariant()) {
        throw "YASAKLI DOSYA BULUNDU: $($_.Name)"
    }
    if ($_.Extension.ToLowerInvariant() -notin @(".dll")) {
        $txt = Get-Content $_.FullName -Raw -ErrorAction SilentlyContinue
        if ($txt -and ($txt -match $piiRegex)) {
            throw "PII ESLEMESI BULUNDU ($($_.Name)): $($Matches[0])"
        }
    }
}
Write-Host "  Tarama temiz: yasakli dosya veya PII yok." -ForegroundColor Green

# 5) ZIP ve SHA256SUMS.txt uretimi
Write-Host "[5/5] ZIP ve SHA256SUMS.txt uretiliyor..."
$zipName = "SuperCan-Coop-v$Version.zip"
$zipPath = Join-Path $outDir $zipName
if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zipPath -Force

$zipHash = (Get-FileHash $zipPath -Algorithm SHA256).Hash
$dllHash = (Get-FileHash $buildDll -Algorithm SHA256).Hash

$sumsPath = Join-Path $outDir "SHA256SUMS.txt"
@(
    "$zipHash  $zipName",
    "$dllHash  dinput8.dll",
    "$steamHash  steam_api.dll"
) | Set-Content $sumsPath -Encoding ascii

Remove-Item $stage -Recurse -Force

Write-Host ""
Write-Host "Release hazir!" -ForegroundColor Green
Write-Host "  ZIP:  $zipPath ($((Get-Item $zipPath).Length) B)"
Write-Host "  SUMS: $sumsPath"
Get-Content $sumsPath
