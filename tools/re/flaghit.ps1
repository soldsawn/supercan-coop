param([int]$ProcId, [string]$Box)
Add-Type -TypeDefinition @"
using System; using System.Runtime.InteropServices;
public static class WPM {
  [DllImport("kernel32.dll")] public static extern IntPtr OpenProcess(int a, bool b, int pid);
  [DllImport("kernel32.dll")] public static extern bool ReadProcessMemory(IntPtr h, IntPtr a, byte[] buf, int n, out int r);
  [DllImport("kernel32.dll")] public static extern bool WriteProcessMemory(IntPtr h, IntPtr a, byte[] buf, int n, out int r);
}
"@
$h = [WPM]::OpenProcess(0x0438, $false, $ProcId)
$b = [Convert]::ToInt64($Box, 16); $r = 0
$buf = New-Object byte[] 4
[void][WPM]::ReadProcessMemory($h, [IntPtr]($b + 0x11C), $buf, 4, [ref]$r)
$s35 = [BitConverter]::ToInt32($buf, 0)
"s35 before = $($s35 / 1024.0)"
[void][WPM]::WriteProcessMemory($h, [IntPtr]($b + 0x124), [BitConverter]::GetBytes([int]0), 4, [ref]$r)
[void][WPM]::WriteProcessMemory($h, [IntPtr]($b + 0x11C), [BitConverter]::GetBytes([int]($s35 - 102)), 4, [ref]$r)
"written s35 = $(($s35 - 102) / 1024.0)"
