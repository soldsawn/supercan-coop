param([int]$ProcId, [string]$Addr, [string]$Len, [string]$Out)
Add-Type -TypeDefinition @"
using System; using System.Runtime.InteropServices;
public static class RPM {
  [DllImport("kernel32.dll")] public static extern IntPtr OpenProcess(int a, bool b, int pid);
  [DllImport("kernel32.dll")] public static extern bool ReadProcessMemory(IntPtr h, IntPtr a, byte[] buf, int n, out int r);
}
"@
$h = [RPM]::OpenProcess(0x0410, $false, $ProcId)
$a = [Convert]::ToInt64($Addr, 16); $n = [Convert]::ToInt32($Len, 16)
$buf = New-Object byte[] $n; $r = 0
$ok = [RPM]::ReadProcessMemory($h, [IntPtr]$a, $buf, $n, [ref]$r)
[IO.File]::WriteAllBytes($Out, $buf)
"ok=$ok read=$r"
