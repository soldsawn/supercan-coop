# SuperCan Co-op Ozel Baslatici (Windows Forms)
# SuperCan_Oyna.exe olmadan (sertifika/guncelleme uyarisiz) dogrudan supercan.exe'yi baslatir.
param(
    [string]$GameDir = ""
)

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[System.Windows.Forms.Application]::EnableVisualStyles()

function Find-GameDir {
    param([string]$Hint)
    $candidates = @()
    if ($Hint) { $candidates += $Hint; $candidates += (Join-Path $Hint "Game") }
    $candidates += $PSScriptRoot
    $candidates += (Join-Path $PSScriptRoot "Game")
    $candidates += (Split-Path $PSScriptRoot -Parent)
    foreach ($c in $candidates) {
        if ($c -and (Test-Path (Join-Path $c "supercan.exe"))) { return (Resolve-Path $c).Path }
    }
    $reg = Get-ItemProperty -Path "HKCU:\Software\Sobee\SuperCan" -ErrorAction SilentlyContinue
    if ($reg -and $reg.path) {
        $p = Join-Path $reg.path "Game"
        if (Test-Path (Join-Path $p "supercan.exe")) { return $p }
    }
    return ""
}

function Compute-OhcToken {
    $now = Get-Date
    $s1 = 0; foreach ($ch in [char[]]"UNKNOWN") { $s1 += [int]$ch }
    $s2 = 0; foreach ($ch in [char[]]"null")    { $s2 += [int]$ch }
    $yr = $now.Year % 100
    $mo = $now.Month
    $dy = $now.Day
    $hr = $now.Hour
    $m10 = [math]::Floor($now.Minute / 10)
    return ($s1 * 11 + $s2 * 7 + $yr * 223 + $mo * 91 + $dy * 53 + $hr * 37 + $m10 * 48 + 684)
}

function Read-CoopIni {
    param([string]$Path)
    $map = @{ net="steam"; listen="27015"; peer=""; window="full"; skip_intro="1"; keep_ga="1"; steam_lobby="" }
    if (Test-Path $Path) {
        foreach ($line in (Get-Content $Path -ErrorAction SilentlyContinue)) {
            $t = $line.Trim()
            if (-not $t -or $t.StartsWith(";") -or $t.StartsWith("#") -or $t.StartsWith("[")) { continue }
            $idx = $t.IndexOf("=")
            if ($idx -gt 0) {
                $k = $t.Substring(0, $idx).Trim().ToLower()
                $v = $t.Substring($idx + 1).Trim()
                $map[$k] = $v
            }
        }
    }
    return $map
}

$resolvedDir = Find-GameDir -Hint $GameDir
$iniPath = if ($resolvedDir) { Join-Path $resolvedDir "coop.ini" } else { "" }
$cfg = Read-CoopIni -Path $iniPath

$form = New-Object System.Windows.Forms.Form
$form.Text = "SuperCan Co-op Baslatici"
$form.Size = New-Object System.Drawing.Size(520, 495)
$form.StartPosition = "CenterScreen"
$form.FormBorderStyle = "FixedDialog"
$form.MaximizeBox = $false
$form.BackColor = [System.Drawing.Color]::FromArgb(20, 24, 34)
$form.ForeColor = [System.Drawing.Color]::FromArgb(235, 240, 248)
$fontMain = New-Object System.Drawing.Font("Segoe UI", 9.5)
$fontBold = New-Object System.Drawing.Font("Segoe UI", 10, [System.Drawing.FontStyle]::Bold)
$fontTitle = New-Object System.Drawing.Font("Segoe UI", 14, [System.Drawing.FontStyle]::Bold)
$form.Font = $fontMain

$lblTitle = New-Object System.Windows.Forms.Label
$lblTitle.Text = "SUPERCAN 1  CO-OP BASLATICI"
$lblTitle.Font = $fontTitle
$lblTitle.ForeColor = [System.Drawing.Color]::FromArgb(72, 202, 228)
$lblTitle.Location = New-Object System.Drawing.Point(20, 14)
$lblTitle.AutoSize = $true
$form.Controls.Add($lblTitle)

$lblSub = New-Object System.Windows.Forms.Label
$lblSub.Text = "Steam Lobi (F8) + Yerel Ag UDP  |  Sertifika uyarisiz dogrudan baslatma"
$lblSub.ForeColor = [System.Drawing.Color]::FromArgb(150, 165, 190)
$lblSub.Location = New-Object System.Drawing.Point(22, 44)
$lblSub.AutoSize = $true
$form.Controls.Add($lblSub)

function Add-Label($text, $x, $y) {
    $l = New-Object System.Windows.Forms.Label
    $l.Text = $text
    $l.Location = New-Object System.Drawing.Point($x, $y)
    $l.AutoSize = $true
    $form.Controls.Add($l)
    return $l
}

# Oyun klasoru
Add-Label "Oyun Game Klasoru:" 22 76 | Out-Null
$txtGame = New-Object System.Windows.Forms.TextBox
$txtGame.Location = New-Object System.Drawing.Point(22, 98)
$txtGame.Size = New-Object System.Drawing.Size(380, 25)
$txtGame.BackColor = [System.Drawing.Color]::FromArgb(30, 36, 50)
$txtGame.ForeColor = [System.Drawing.Color]::White
$txtGame.Text = $resolvedDir
$form.Controls.Add($txtGame)

$btnBrowse = New-Object System.Windows.Forms.Button
$btnBrowse.Text = "Gozat..."
$btnBrowse.Location = New-Object System.Drawing.Point(410, 97)
$btnBrowse.Size = New-Object System.Drawing.Size(78, 27)
$btnBrowse.FlatStyle = "Flat"
$btnBrowse.BackColor = [System.Drawing.Color]::FromArgb(42, 52, 72)
$btnBrowse.Add_Click({
    $fbd = New-Object System.Windows.Forms.FolderBrowserDialog
    $fbd.Description = "SuperCan Game klasorunu secin (supercan.exe iceren)"
    if ($fbd.ShowDialog() -eq "OK") {
        $found = Find-GameDir -Hint $fbd.SelectedPath
        if ($found) { $txtGame.Text = $found } else { $txtGame.Text = $fbd.SelectedPath }
    }
})
$form.Controls.Add($btnBrowse)

# Ag modu
Add-Label "Baglanti Modu:" 22 138 | Out-Null
$cmbNet = New-Object System.Windows.Forms.ComboBox
$cmbNet.Location = New-Object System.Drawing.Point(175, 134)
$cmbNet.Size = New-Object System.Drawing.Size(313, 25)
$cmbNet.DropDownStyle = "DropDownList"
$cmbNet.BackColor = [System.Drawing.Color]::FromArgb(30, 36, 50)
$cmbNet.ForeColor = [System.Drawing.Color]::White
[void]$cmbNet.Items.Add("Steam (Internet / Arkadas Daveti - F8)")
[void]$cmbNet.Items.Add("Yerel Ag (UDP / LAN)")
$cmbNet.SelectedIndex = if ($cfg["net"] -eq "udp") { 1 } else { 0 }
$form.Controls.Add($cmbNet)

# Steam Lobi ID veya UDP Peer
Add-Label "Steam Lobi ID / UDP IP:" 22 174 | Out-Null
$txtPeer = New-Object System.Windows.Forms.TextBox
$txtPeer.Location = New-Object System.Drawing.Point(175, 170)
$txtPeer.Size = New-Object System.Drawing.Size(313, 25)
$txtPeer.BackColor = [System.Drawing.Color]::FromArgb(30, 36, 50)
$txtPeer.ForeColor = [System.Drawing.Color]::White
$txtPeer.Text = if ($cfg["net"] -eq "udp") { $cfg["peer"] } else { $cfg["steam_lobby"] }
$form.Controls.Add($txtPeer)

# Pencere modu
Add-Label "Ekran Modu:" 22 210 | Out-Null
$cmbWin = New-Object System.Windows.Forms.ComboBox
$cmbWin.Location = New-Object System.Drawing.Point(175, 206)
$cmbWin.Size = New-Object System.Drawing.Size(313, 25)
$cmbWin.DropDownStyle = "DropDownList"
$cmbWin.BackColor = [System.Drawing.Color]::FromArgb(30, 36, 50)
$cmbWin.ForeColor = [System.Drawing.Color]::White
[void]$cmbWin.Items.Add("Cercevesiz Tam Ekran (Onerilen - window=full)")
[void]$cmbWin.Items.Add("Gercek Tam Ekran (Oyunun Kendi Modu - window=off)")
[void]$cmbWin.Items.Add("Pencere 1280x720 (Sol Ust)")
$cmbWin.SelectedIndex = if ($cfg["window"] -eq "off") { 1 } elseif ($cfg["window"] -match ",") { 2 } else { 0 }
$form.Controls.Add($cmbWin)

# Cozunurluk
Add-Label "Cozunurluk:" 22 246 | Out-Null
$cmbRes = New-Object System.Windows.Forms.ComboBox
$cmbRes.Location = New-Object System.Drawing.Point(175, 242)
$cmbRes.Size = New-Object System.Drawing.Size(313, 25)
$cmbRes.DropDownStyle = "DropDownList"
$cmbRes.BackColor = [System.Drawing.Color]::FromArgb(30, 36, 50)
$cmbRes.ForeColor = [System.Drawing.Color]::White
[void]$cmbRes.Items.Add("1920x1080")
[void]$cmbRes.Items.Add("1600x900")
[void]$cmbRes.Items.Add("1366x768")
[void]$cmbRes.Items.Add("1280x720")
$cmbRes.SelectedIndex = 0
$form.Controls.Add($cmbRes)

# Grafik kalitesi
Add-Label "Grafik Kalitesi:" 22 282 | Out-Null
$cmbGfx = New-Object System.Windows.Forms.ComboBox
$cmbGfx.Location = New-Object System.Drawing.Point(175, 278)
$cmbGfx.Size = New-Object System.Drawing.Size(313, 25)
$cmbGfx.DropDownStyle = "DropDownList"
$cmbGfx.BackColor = [System.Drawing.Color]::FromArgb(30, 36, 50)
$cmbGfx.ForeColor = [System.Drawing.Color]::White
[void]$cmbGfx.Items.Add("Yuksek (Onerilen)")
[void]$cmbGfx.Items.Add("Orta")
[void]$cmbGfx.Items.Add("Dusuk")
$cmbGfx.SelectedIndex = 0
$form.Controls.Add($cmbGfx)

# Secenekler
$chkIntro = New-Object System.Windows.Forms.CheckBox
$chkIntro.Text = "Acilis videolarini atla (skip_intro=1)"
$chkIntro.Location = New-Object System.Drawing.Point(22, 318)
$chkIntro.AutoSize = $true
$chkIntro.Checked = ($cfg["skip_intro"] -ne "0")
$form.Controls.Add($chkIntro)

$chkKeepGa = New-Object System.Windows.Forms.CheckBox
$chkKeepGa.Text = "Grafik dosyasini koru (keep_ga=1)"
$chkKeepGa.Location = New-Object System.Drawing.Point(280, 318)
$chkKeepGa.AutoSize = $true
$chkKeepGa.Checked = ($cfg["keep_ga"] -ne "0")
$form.Controls.Add($chkKeepGa)

$lblInfo = New-Object System.Windows.Forms.Label
$lblInfo.Text = "Oyun ici kisayollar:  F5 = Arkadasa Isinlan  |  F8 = Steam Lobi & Davet  |  F9 = HUD Gizle"
$lblInfo.ForeColor = [System.Drawing.Color]::FromArgb(144, 224, 239)
$lblInfo.Location = New-Object System.Drawing.Point(22, 355)
$lblInfo.Size = New-Object System.Drawing.Size(470, 30)
$form.Controls.Add($lblInfo)

$btnPlay = New-Object System.Windows.Forms.Button
$btnPlay.Text = "OYUNU BASLAT (CO-OP)"
$btnPlay.Font = $fontBold
$btnPlay.Location = New-Object System.Drawing.Point(22, 395)
$btnPlay.Size = New-Object System.Drawing.Size(466, 44)
$btnPlay.FlatStyle = "Flat"
$btnPlay.BackColor = [System.Drawing.Color]::FromArgb(0, 150, 199)
$btnPlay.ForeColor = [System.Drawing.Color]::White
$btnPlay.Add_Click({
    $gdir = $txtGame.Text.Trim()
    $exe = Join-Path $gdir "supercan.exe"
    if (-not (Test-Path $exe)) {
        [System.Windows.Forms.MessageBox]::Show("supercan.exe bulunamadi:`n$exe`nLutfen dogru Game klasorunu secin.", "Hata", "OK", "Error") | Out-Null
        return
    }
    $netVal = if ($cmbNet.SelectedIndex -eq 1) { "udp" } else { "steam" }
    $winVal = if ($cmbWin.SelectedIndex -eq 1) { "off" } elseif ($cmbWin.SelectedIndex -eq 2) { "0,0,1280,720" } else { "full" }
    $peerVal = if ($netVal -eq "udp") { $txtPeer.Text.Trim() } else { "" }
    $lobbyVal = if ($netVal -eq "steam") { $txtPeer.Text.Trim() } else { "" }

    $iniLines = @(
        "[coop]",
        "net=$netVal",
        "listen=27015",
        "peer=$peerVal",
        "window=$winVal",
        "skip_intro=$(if ($chkIntro.Checked) { 1 } else { 0 })",
        "keep_ga=$(if ($chkKeepGa.Checked) { 1 } else { 0 })"
    )
    if ($lobbyVal) { $iniLines += "steam_lobby=$lobbyVal" }
    Set-Content -Path (Join-Path $gdir "coop.ini") -Value $iniLines -Encoding ASCII

    # Hata 22 (ohc) jetonunu hesaplayip kayit defterine yaz (SuperCan_Oyna.exe gerektirmez!)
    $tok = Compute-OhcToken
    $regPath = "HKCU:\Software\Sobee\SuperCan"
    if (-not (Test-Path $regPath)) { New-Item -Path $regPath -Force | Out-Null }
    Set-ItemProperty -Path $regPath -Name "ohc" -Value "$tok" -Type String

    $resParts = $cmbRes.SelectedItem.ToString().Split("x")
    $rx = $resParts[0]; $ry = $resParts[1]
    $gfxArgs = switch ($cmbGfx.SelectedIndex) {
        1 { "-d defGSQ_2 -d defGPQ_2 -d defGTQ_1 -d defGMQ_2 -d defGGQ_1 -d defGEQ_1 -d defGKT_0" }
        2 { "-d defGSQ_1 -d defGPQ_1 -d defGTQ_0 -d defGMQ_1 -d defGGQ_0 -d defGEQ_0 -d defGKT_0" }
        default { "-d defGSQ_3 -d defGPQ_3 -d defGTQ_2 -d defGMQ_3 -d defGGQ_1 -d defGEQ_2 -d defGKT_0" }
    }
    $args = "-nx 270 -pl oyuncu $gfxArgs -ip UNKNOWN -sn null -d defGRX=$rx -d defGRY=$ry"
    if ($lobbyVal -and $netVal -eq "steam") { $args += " +connect_lobby $lobbyVal" }

    Start-Process -FilePath $exe -ArgumentList $args -WorkingDirectory $gdir
    $form.Close()
})
$form.Controls.Add($btnPlay)

[void]$form.ShowDialog()
