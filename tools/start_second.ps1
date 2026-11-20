# Start the second test copy directly (no launcher: the launcher would terminate the first copy).
# Its DLL reads settings from Game\coop.ini and reuses the token shared by the launcher-started first copy.
param([string]$Copy = "SuperCan1b")
$repo = Split-Path -Parent $PSScriptRoot
$g = Join-Path $repo "local\testinstall\$Copy\Game"
Remove-Item Env:COOP_WINDOW, Env:COOP_LISTEN, Env:COOP_PEER, Env:COOP_OHC, Env:COOP_QUIET -ErrorAction SilentlyContinue
$a = '-nx 270 -pl oyuncu -d defGSQ_3 -d defGPQ_3 -d defGTQ_2 -d defGMQ_3 -d defGGQ_1 -d defGEQ_2 -d defGKT_0 -ip UNKNOWN -sn null -d defGRX=1920 -d defGRY=1080'
$p = Start-Process "$g\supercan.exe" -ArgumentList $a -WorkingDirectory $g -PassThru
Start-Sleep 8
$l = Get-ChildItem "$g\coop_probe_*_$($p.Id).log" -ErrorAction SilentlyContinue
"pid={0} alive={1}" -f $p.Id, (-not $p.HasExited)
if ($l) { (Get-Content $l.FullName | Select-String "\[ohc\]|\[coop\] listening|MessageBox\(").Line }
