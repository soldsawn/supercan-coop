# SuperCan 1 Co-op anonim log toplayici
# Son 3 oyun logunu, coop.ini ve DLL/EXE hash ozetini Masaustu'nde tek .zip olarak toplar.
# SteamID, IP adresleri ve Windows kullanici adi otomatik maskelenir.
param([Parameter(Mandatory)][string]$Game)
$ErrorActionPreference = "Stop"

$exe = Join-Path $Game "supercan.exe"
if (-not (Test-Path $exe)) { throw "supercan.exe bulunamadi: $exe" }

$ts = Get-Date -Format "yyyyMMdd_HHmmss"
$tmp = Join-Path $env:TEMP ("supercan_log_" + $ts)
New-Item -ItemType Directory -Force $tmp | Out-Null

$escapedUser = [Regex]::Escape($env:USERNAME)
function Anon([string]$s) {
    if (-not $s) { return "" }
    $s = [Regex]::Replace($s, '7656119\d{10}', '<STEAMID_MASKED>')
    $s = [Regex]::Replace($s, '\b(?!127\.0\.0\.1\b)(?:\d{1,3}\.){3}\d{1,3}\b', '<IP_MASKED>')
    $s = [Regex]::Replace($s, '([A-Za-z]:\\Users\\)[^\\\r\n"'']+', '$1<USER>')
    if ($escapedUser.Length -ge 3) {
        $s = [Regex]::Replace($s, $escapedUser, '<USER>', 'IgnoreCase')
    }
    return $s
}

$logs = @(Get-ChildItem $Game -Filter "coop_probe_*.log" -ErrorAction SilentlyContinue |
          Sort-Object LastWriteTime -Descending |
          Select-Object -First 3)
foreach ($l in $logs) {
    $raw = Get-Content $l.FullName -Raw -ErrorAction SilentlyContinue
    Set-Content (Join-Path $tmp $l.Name) (Anon $raw) -Encoding UTF8
}

$ini = Join-Path $Game "coop.ini"
if (Test-Path $ini) {
    Set-Content (Join-Path $tmp "coop.ini") (Anon (Get-Content $ini -Raw)) -Encoding UTF8
}

$info = @(
    "SuperCan Co-op Teshis Ozeti",
    ("Tarih: " + (Get-Date -Format "yyyy-MM-dd HH:mm:ss")),
    ("OS: " + [Environment]::OSVersion.VersionString),
    ("Toplanan log sayisi: " + $logs.Count),
    ""
)
foreach ($f in "dinput8.dll", "steam_api.dll", "supercan.exe", "supercan.exe.orig") {
    $p = Join-Path $Game $f
    if (Test-Path $p) {
        $it = Get-Item $p
        $h = (Get-FileHash $p -Algorithm SHA256).Hash
        $info += ("{0,-20} {1,10} B  SHA256={2}" -f $f, $it.Length, $h)
    } else {
        $info += ("{0,-20} YOK" -f $f)
    }
}
$info | Set-Content (Join-Path $tmp "system_info.txt") -Encoding UTF8

$desk = [Environment]::GetFolderPath("Desktop")
if (-not $desk) { $desk = $Game }
$zip = Join-Path $desk ("SuperCan_Coop_Log_" + $ts + ".zip")
Compress-Archive -Path (Join-Path $tmp "*") -DestinationPath $zip -Force
Remove-Item $tmp -Recurse -Force

Write-Host ("Anonim log paketi olusturuldu: " + $zip) -ForegroundColor Green
Write-Host "SteamID, IP adresleri ve kullanici adiniz otomatik olarak maskelendi." -ForegroundColor Cyan
