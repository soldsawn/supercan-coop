# Window helper for automated tests (no real mouse): list a process's windows/controls, click a button by text,
# or click at a client position inside a top-level window.
#   winctl.ps1 -Proc SuperCan_Oyna -List
#   winctl.ps1 -Proc SuperCan_Oyna -Button Tamam
#   winctl.ps1 -Proc SuperCan_Oyna -ClickX 400 -ClickY 300 [-Title text]
param([string]$Proc, [switch]$List, [string]$Button, [int]$ClickX = -1, [int]$ClickY = -1, [string]$Title, [string]$Shot)
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing @"
using System; using System.Text; using System.Collections.Generic; using System.Runtime.InteropServices;
public class W {
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc f, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr p, EnumProc f, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  public static string Text(IntPtr h) { var s = new StringBuilder(256); GetWindowText(h, s, 256); return s.ToString(); }
  public static string Cls(IntPtr h) { var s = new StringBuilder(256); GetClassName(h, s, 256); return s.ToString(); }
  public static List<IntPtr> Tops(uint pid) { var r = new List<IntPtr>(); EnumWindows((h, l) => { uint p; GetWindowThreadProcessId(h, out p); if (p == pid && IsWindowVisible(h)) r.Add(h); return true; }, IntPtr.Zero); return r; }
  public static List<IntPtr> Kids(IntPtr t) { var r = new List<IntPtr>(); EnumChildWindows(t, (h, l) => { r.Add(h); return true; }, IntPtr.Zero); return r; }
}
"@
$ps = Get-Process -Name $Proc -ErrorAction SilentlyContinue
if (-not $ps) { "no process $Proc"; exit 1 }
$done = $false
foreach ($p in $ps) {
  foreach ($t in [W]::Tops([uint32]$p.Id)) {
    $r = New-Object W+RECT; [void][W]::GetWindowRect($t, [ref]$r)
    if ($List) { "TOP {0} '{1}' [{2}] {3},{4} {5}x{6}" -f $t, [W]::Text($t), [W]::Cls($t), $r.L, $r.T, ($r.R - $r.L), ($r.B - $r.T)
      foreach ($k in [W]::Kids($t)) { $kr = New-Object W+RECT; [void][W]::GetWindowRect($k, [ref]$kr); "   kid {0} '{1}' [{2}] {3},{4} {5}x{6}" -f $k, [W]::Text($k), [W]::Cls($k), $kr.L, $kr.T, ($kr.R - $kr.L), ($kr.B - $kr.T) } }
    if ($Shot -and (-not $Title -or [W]::Text($t) -like "*$Title*")) {
      $bmp = New-Object System.Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T); $g = [System.Drawing.Graphics]::FromImage($bmp); $dc = $g.GetHdc()
      [void][W]::PrintWindow($t, $dc, 2); $g.ReleaseHdc($dc); $bmp.Save($Shot, [System.Drawing.Imaging.ImageFormat]::Png); $g.Dispose(); $bmp.Dispose()
      "shot '{0}' -> {1}" -f [W]::Text($t), $Shot; $done = $true }
    if ($Button) { foreach ($k in [W]::Kids($t)) { if ([W]::Text($k) -like "*$Button*") { [void][W]::SendMessage($k, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero); "clicked button '{0}'" -f [W]::Text($k); $done = $true; break } } }
    if ($ClickX -ge 0 -and -not $done -and (-not $Title -or [W]::Text($t) -like "*$Title*")) {
      $sx = $r.L + $ClickX; $sy = $r.T + $ClickY; $tg = $t; $ox = $r.L; $oy = $r.T   # deepest visible child under the point gets it
      foreach ($k in [W]::Kids($t)) { $kr = New-Object W+RECT; [void][W]::GetWindowRect($k, [ref]$kr)
        if ([W]::IsWindowVisible($k) -and $sx -ge $kr.L -and $sx -lt $kr.R -and $sy -ge $kr.T -and $sy -lt $kr.B -and ($kr.R - $kr.L) -gt 0 -and ($kr.B - $kr.T) -gt 0) { $tg = $k; $ox = $kr.L; $oy = $kr.T } }
      $lp = [IntPtr]((($sy - $oy) -shl 16) -bor (($sx - $ox) -band 0xffff))
      [void][W]::PostMessage($tg, 0x0200, [IntPtr]::Zero, $lp); [void][W]::PostMessage($tg, 0x0201, [IntPtr]1, $lp); Start-Sleep -Milliseconds 80; [void][W]::PostMessage($tg, 0x0202, [IntPtr]::Zero, $lp)
      "clicked {0},{1} in '{2}'" -f $ClickX, $ClickY, [W]::Text($t); $done = $true }
    if ($done) { break }
  }
  if ($done) { break }
}
