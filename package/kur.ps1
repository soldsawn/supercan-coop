# SuperCan 1 Co-op kurulum ve kaldirma betigi (orijinal kuruluma kurmayin, kopya klasorde kullanin).
# Kurulum:   powershell -ExecutionPolicy Bypass -File kur.ps1 -Game "C:\SuperCanCoop\Game"
#   -Net steam   (varsayilan) internet uzerinden, Steam arkadas listesi / F8 Lobi ile
#   -Net udp     yalnizca ayni yerel ag; -Peer 192.168.1.100 (istege bagli)
#   -NoShortcut  Masaustu kisayolu olusturma
# Kaldirma:  powershell -ExecutionPolicy Bypass -File kur.ps1 -Game "C:\SuperCanCoop\Game" -Uninstall
param(
    [Parameter(Mandatory)][string]$Game,
    [ValidateSet("steam", "udp")][string]$Net = "steam",
    [string]$Peer = "",
    [int]$Port = 27015,
    [switch]$Uninstall,
    [switch]$NoShortcut
)
$ErrorActionPreference = "Stop"
$src = Split-Path -Parent $MyInvocation.MyCommand.Path
$exe = Join-Path $Game "supercan.exe"
if (-not (Test-Path $exe)) { throw "supercan.exe bulunamadi: $exe" }

$origHash = "AA91BD3B6D5EF81D82E4748C14E18A260E6090998B1AD3698DBD7A34EBB0C3B4"
$laaHash  = "0AB2D5FF30CC4293AAE6C5072C4F23E6384A8B531EC88026C668B92F07C2EB2A"

if ($Uninstall) {
    Write-Host "SuperCan Co-op kaldiriliyor: $Game" -ForegroundColor Yellow
    $origFile = "$exe.orig"
    if (Test-Path $origFile) {
        Copy-Item $origFile $exe -Force
        Remove-Item $origFile -Force
        $restoredHash = (Get-FileHash $exe -Algorithm SHA256).Hash
        if ($restoredHash -eq $origHash) {
            Write-Host "supercan.exe orijinal yedekten geri yuklendi ve SHA-256 dogrulandi." -ForegroundColor Green
        } else {
            Write-Warning "supercan.exe yedekten geri yuklendi ancak hash beklenen orijinalle eslesmedi ($restoredHash)."
        }
    } else {
        Write-Warning "supercan.exe.orig yedegi bulunamadi; supercan.exe'ye dokunulmadi."
    }

    $removeFiles = @(
        "dinput8.dll",
        "steam_api.dll",
        "steam_appid.txt",
        "SuperCan_Coop_Baslatici.ps1",
        "SuperCan_Coop_Oyna.bat",
        "LOG_TOPLA.bat",
        "log_topla.ps1",
        "coop.ini",
        "coop_status.txt",
        "coop_cmd.txt"
    )
    foreach ($f in $removeFiles) {
        $p = Join-Path $Game $f
        if (Test-Path $p) {
            Remove-Item $p -Force
            Write-Host "$f kaldirildi."
        }
    }

    $desktop = [Environment]::GetFolderPath("Desktop")
    if ($desktop) {
        $lnk = Join-Path $desktop "SuperCan Co-op.lnk"
        if (Test-Path $lnk) {
            Remove-Item $lnk -Force
            Write-Host "Masaustu kisayolu kaldirildi."
        }
    }

    Write-Host "Kaldirma tamamlandi. (Eski coop_probe_*.log dosyalari isterseniz elle silinebilir.)" -ForegroundColor Green
    return
}

# --- KURULUM ---
$h = (Get-FileHash $exe -Algorithm SHA256).Hash
if ($h -eq $origHash) {
    # Large-address-aware: PE header Characteristics |= 0x20 (2 GB -> 4 GB; E2005 hatasini giderir). Exe asla dagitilmaz.
    Copy-Item $exe "$exe.orig" -Force
    $b = [IO.File]::ReadAllBytes($exe)
    $off = [BitConverter]::ToInt32($b, 0x3C) + 22
    $b[$off] = $b[$off] -bor 0x20
    [IO.File]::WriteAllBytes($exe, $b)
    if ((Get-FileHash $exe -Algorithm SHA256).Hash -ne $laaHash) {
        Copy-Item "$exe.orig" $exe -Force
        throw "LAA yamasi beklenen sonucu vermedi; supercan.exe geri alindi."
    }
    Write-Host "supercan.exe yedeklendi (.orig) ve LAA yamasi yapildi." -ForegroundColor Green
}
elseif ($h -eq $laaHash) {
    Write-Host "supercan.exe zaten LAA surumu."
}
else {
    Write-Warning "supercan.exe beklenen surum degil ($h). Exe'ye dokunulmadi; DLL yine de kuruluyor."
}

function Same($x, $y) { (Test-Path $y) -and ((Resolve-Path $x).Path -ieq (Resolve-Path $y).Path) }
$copyList = @("dinput8.dll", "steam_api.dll", "SuperCan_Coop_Baslatici.ps1", "SuperCan_Coop_Oyna.bat", "LOG_TOPLA.bat", "log_topla.ps1")
foreach ($f in $copyList) {
    $from = Join-Path $src $f
    if (-not (Test-Path $from)) {
        if ($f -eq "dinput8.dll" -or $f -eq "steam_api.dll") {
            Write-Warning "$f paket klasorunde bulunamadi ($from). Release paketini kullandiginizdan emin olun."
        }
        continue
    }
    $to = Join-Path $Game $f
    if (Same $from $to) { Write-Host "$f zaten yerinde (paket Game klasorunun icinde)." }
    else { Copy-Item $from $to -Force; Write-Host "$f kopyalandi." }
}
Set-Content (Join-Path $Game "steam_appid.txt") "480" -Encoding ascii -NoNewline   # Steam 'Spacewar' (AppID 480)

$ini = Join-Path $Game "coop.ini"
if ($Peer -or -not (Test-Path $ini)) {
    $peerLine = if ($Peer) { "peer=${Peer}:$Port" } else { "peer=" }
    @("[coop]", "net=$Net", "listen=$Port", $peerLine, "window=full") | Set-Content $ini -Encoding ascii
    Write-Host "coop.ini yazildi:"
} else {
    $lines = @(Get-Content $ini | Where-Object { $_ -notmatch '^\s*net\s*=' })
    $i = [Array]::FindIndex([string[]]$lines, [Predicate[string]]{ param($l) $l -match '^\s*\[coop\]' })
    if ($i -lt 0) { $lines = @("[coop]") + $lines; $i = 0 }
    $lines = @($lines[0..$i]) + "net=$Net" + @(if ($i + 1 -lt $lines.Count) { $lines[($i + 1)..($lines.Count - 1)] })
    $lines | Set-Content $ini -Encoding ascii
    Write-Host "mevcut coop.ini korundu, net=$Net yapildi:"
}
Get-Content $ini

if (-not $NoShortcut) {
    $launcher = Join-Path $Game "SuperCan_Coop_Oyna.bat"
    $desktop = [Environment]::GetFolderPath("Desktop")
    if ((Test-Path $launcher) -and $desktop) {
        try {
            $ws = New-Object -ComObject WScript.Shell
            $sc = $ws.CreateShortcut((Join-Path $desktop "SuperCan Co-op.lnk"))
            $sc.TargetPath = $launcher
            $sc.WorkingDirectory = $Game
            $sc.Description = "SuperCan 1 Co-op Baslatici"
            $sc.IconLocation = "$exe,0"
            $sc.Save()
            Write-Host "Masaustu kisayolu olusturuldu: SuperCan Co-op" -ForegroundColor Green
        } catch {
            Write-Warning "Masaustu kisayolu olusturulamadi: $_"
        }
    }
}

if ($Net -eq "steam") {
    Write-Host "Steam modu hazir: oyundan ONCE Steam'i ac ve giris yap. Oyunu SuperCan_Coop_Oyna.bat ile baslat." -ForegroundColor Cyan
}
