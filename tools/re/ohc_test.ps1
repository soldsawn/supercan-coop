# Try candidate 'ohc' tokens: launch one copy quietly, report ACCEPT (reaches frame 300) or REJECT (Hata 22).
param([string[]]$Tokens)
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$g = Join-Path $repo "local\testinstall\SuperCan1b\Game"
$a = '-nx 270 -pl oyuncu -d defGSQ_1 -d defGPQ_0 -d defGTQ_0 -d defGMQ_3 -d defGGQ_1 -d defGEQ_2 -d defGKT_0 -ip UNKNOWN -sn null -d defGRX=1024 -d defGRY=768'
$env:COOP_QUIET = "1"; $env:COOP_WINDOW = "0,600,512,384"
foreach ($t in $Tokens) {
    $env:COOP_OHC = $t
    $p = Start-Process "$g\supercan.exe" -ArgumentList $a -WorkingDirectory $g -PassThru
    $deadline = (Get-Date).AddSeconds(25); $res = "TIMEOUT"
    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 500
        $l = Get-ChildItem "$g\coop_probe_*_$($p.Id).log" -ErrorAction SilentlyContinue
        if ($l) { $txt = Get-Content $l.FullName -Raw
            if ($txt -match "Hata 22") { $res = "REJECT"; break }
            if ($txt -match "Hata ") { $res = "OTHER: " + ([regex]::Match($txt, "Hata [^']*")).Value; break }
            if ($txt -match "first engine_frame returned 1") { $res = "ACCEPT"; break }
        }
        if ($p.HasExited) { $res = "EXITED"; break }
    }
    if ($res -eq "TIMEOUT" -and -not $p.HasExited) { $res = "ACCEPT" }
    if (-not $p.HasExited) { Stop-Process $p -Force }
    "{0} {1:HH:mm:ss} {2}" -f $t, (Get-Date), $res
    Start-Sleep 1
}
Remove-Item Env:COOP_QUIET, Env:COOP_WINDOW, Env:COOP_OHC -ErrorAction SilentlyContinue

