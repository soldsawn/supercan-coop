# Automated split-screen test: (re)start both copies, get both into the level, then run a scenario.
# Needs no user input: the launcher is clicked by window messages (tools/winctl.ps1), the games are driven through
# Game\coop_cmd.txt (probe DLL remote control) and observed via Game\coop_status.txt, logs and in-game screenshots.
#   autotest.ps1 [-Out <dir for screenshots>] [-NoRestart] [-Scenario robots] [-MenuOnly: stop at the menu, user enters the level]
param([string]$Out = "$env:TEMP\supercan_autotest", [switch]$NoRestart, [string]$Scenario = "", [switch]$MenuOnly)
$ErrorActionPreference = "Continue"
$Repo = Split-Path -Parent $PSScriptRoot
$L = Join-Path $Repo "local"; $T = Join-Path $L "testinstall"
New-Item -ItemType Directory -Force $Out | Out-Null
function Cmd($copy, [string[]]$lines) { $f = "$T\$copy\Game\coop_cmd.txt"; for ($i = 0; $i -lt 40 -and (Test-Path $f); $i++) { Start-Sleep -Milliseconds 100 }; Set-Content $f $lines -Encoding ascii }
function Status($copy) { $h = @{}; Get-Content "$T\$copy\Game\coop_status.txt" -ErrorAction SilentlyContinue | ForEach-Object { $k, $v = $_ -split '=', 2; $h[$k] = $v }; $h }
function Shot($copy, $name) { $p = "$Out\$name.jpg"; Remove-Item $p -ErrorAction SilentlyContinue; Cmd $copy "shot $p"; for ($i = 0; $i -lt 30 -and -not (Test-Path $p); $i++) { Start-Sleep -Milliseconds 200 }; $p }
function Click($copy, $x, $y) { Cmd $copy "move $x $y"; Start-Sleep -Milliseconds 700; Cmd $copy "click $x $y"; Start-Sleep -Milliseconds 500 }
function WaitLevel($copy, $lvl, $sec) { for ($i = 0; $i -lt $sec; $i++) { if ((Status $copy).level -eq $lvl) { return $true }; Start-Sleep 1 }; $false }
function TokenFresh { $f = "$env:TEMP\supercan_coop_token.txt"; (Test-Path $f) -and ((Get-Date) - (Get-Item $f).LastWriteTime).TotalMinutes -lt 9 }

if (-not $NoRestart) {
    Get-Process supercan, SuperCan_Oyna -ErrorAction SilentlyContinue | Stop-Process -Force; Start-Sleep 2
    Copy-Item "$L\build\dinput8.dll" "$T\SuperCan1\Game\"; Copy-Item "$L\build\dinput8.dll" "$T\SuperCan1b\Game\"
    Remove-Item "$T\SuperCan1\Game\coop_cmd.txt", "$T\SuperCan1b\Game\coop_cmd.txt", "$T\SuperCan1\Game\coop_status.txt", "$T\SuperCan1b\Game\coop_status.txt" -ErrorAction SilentlyContinue
    if (TokenFresh) { "token fresh: starting both directly"; & "$PSScriptRoot\start_second.ps1" -Copy SuperCan1 | Out-Null }
    else {
        "starting launcher"
        Start-Process "$T\SuperCan1\SuperCan_Oyna.exe" -WorkingDirectory "$T\SuperCan1"
        for ($i = 0; $i -lt 40 -and -not (Get-Process supercan -ErrorAction SilentlyContinue); $i++) {
            Start-Sleep 1
            $w = & "$PSScriptRoot\winctl.ps1" -Proc SuperCan_Oyna -List 2>$null | Out-String
            if ($w -match "'Tamam'") { & "$PSScriptRoot\winctl.ps1" -Proc SuperCan_Oyna -Button Tamam | Out-Null }
            elseif ($w -match "ptal") { & "$PSScriptRoot\winctl.ps1" -Proc SuperCan_Oyna -Button "ptal" | Out-Null }
            elseif ($w -match "S.{1,2}perCan_Oyna") { Start-Sleep 1; & "$PSScriptRoot\winctl.ps1" -Proc SuperCan_Oyna -ClickX 440 -ClickY 310 | Out-Null; Start-Sleep 3 }
        }
        if (-not (Get-Process supercan -ErrorAction SilentlyContinue)) { "launcher did not start the game"; exit 1 }
    }
    Start-Sleep 6
    & "$PSScriptRoot\start_second.ps1" -Copy SuperCan1b | Out-Null
    foreach ($c in "SuperCan1", "SuperCan1b") { if (-not (WaitLevel $c "menu.wmb" 60)) { "$c never reached the menu" } }
    Start-Sleep 12   # menu camera fly-in
    if ($MenuOnly) { "menu ready (both copies); enter the level yourself"; exit 0 }
    foreach ($c in "SuperCan1", "SuperCan1b") {
        for ($try = 0; $try -lt 4 -and (Status $c).level -ne "orman.wmb"; $try++) {
            Cmd $c "moveent menu_oyuna_basla.mdl"; Start-Sleep 0.7; Cmd $c "clickent menu_oyuna_basla.mdl"; Start-Sleep 3   # OYUNA BASLA
            Cmd $c "moveent menu_devam.mdl"; Start-Sleep 0.7; Cmd $c "clickent menu_devam.mdl"                              # OYUNA DEVAM
            if (WaitLevel $c "orman.wmb" 15) { break }
            Shot $c "$c-menu-try$try" | Out-Null
        }
        "{0}: level={1}" -f $c, (Status $c).level
    }
    Start-Sleep 6
    foreach ($c in "SuperCan1", "SuperCan1b") { Shot $c "$c-cutscene" | Out-Null; Cmd $c "key 0x01 4" }   # Esc skips the comic cutscene
    Start-Sleep 5
}
foreach ($c in "SuperCan1", "SuperCan1b") { Shot $c "$c-now" | Out-Null; $s = Status $c; "{0}: level={1} player={2} role={3} rx={4} puppets={5} world_puppets={6} blocked={7}" -f $c, $s.level, $s.player, $s.role, $s.rx, $s.puppets, $s.world_puppets, $s.blocked }
"screenshots: $Out"
