# Launch SuperCan 1 test copies directly (no AutoPlay launcher), side by side in windows.
# The probe DLL (Game\dinput8.dll) answers the launcher handshake (registry 'ohc') and forces windowed D3D9.
param([string[]]$Copies = @("SuperCan1"), [int]$Wait = 15, [int]$ResX = 960, [int]$ResY = 1080, [int]$GameX = 1920, [int]$GameY = 1080)
$root = Join-Path (Split-Path -Parent $PSScriptRoot) "local\testinstall"
$i = 0
$procs = foreach ($c in $Copies) {
    $g = Join-Path $root "$c\Game"
    $env:COOP_WINDOW = "{0},0,{1},{2}" -f ($i * $ResX), $ResX, $ResY
    $env:COOP_LISTEN = 27015 + $i
    $env:COOP_PEER = '127.0.0.1:' + (27015 + (1 - $i))
    $a = "-nx 270 -pl oyuncu -d defGSQ_3 -d defGPQ_3 -d defGTQ_2 -d defGMQ_3 -d defGGQ_1 -d defGEQ_2 -d defGKT_0 -ip UNKNOWN -sn null -d defGRX=$GameX -d defGRY=$GameY"
    Start-Process "$g\supercan.exe" -ArgumentList $a -WorkingDirectory $g -PassThru
    $i++
    Start-Sleep 3
}
Remove-Item Env:COOP_WINDOW,Env:COOP_LISTEN,Env:COOP_PEER -ErrorAction SilentlyContinue
Start-Sleep $Wait
foreach ($p in $procs) { "{0} pid={1} alive={2}" -f $p.Path, $p.Id, (-not $p.HasExited) }
