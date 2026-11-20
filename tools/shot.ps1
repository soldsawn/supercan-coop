# Desktop screenshot (for driving the launcher / checking windows). usage: shot.ps1 -Out file.png [-X 0 -Y 0 -W 1920 -H 1080]
param([string]$Out, [int]$X = 0, [int]$Y = 0, [int]$W = 0, [int]$H = 0)
Add-Type -AssemblyName System.Windows.Forms, System.Drawing
$b = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
if (-not $W) { $W = $b.Width }; if (-not $H) { $H = $b.Height }
$bmp = New-Object System.Drawing.Bitmap $W, $H
$g = [System.Drawing.Graphics]::FromImage($bmp); $g.CopyFromScreen($X, $Y, 0, 0, $bmp.Size)
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png); $g.Dispose(); $bmp.Dispose()
"{0}x{1} -> {2}" -f $W, $H, $Out
