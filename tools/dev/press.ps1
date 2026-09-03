param([string]$Seq = "57", [int]$DelayMs = 1200)
Add-Type -TypeDefinition @"
using System; using System.Runtime.InteropServices;
public static class KI {
  [StructLayout(LayoutKind.Sequential)] public struct KEYBDINPUT { public ushort wVk; public ushort wScan; public uint dwFlags; public uint time; public IntPtr dwExtraInfo; }
  [StructLayout(LayoutKind.Explicit, Size = 40)] public struct INPUT { [FieldOffset(0)] public uint type; [FieldOffset(8)] public KEYBDINPUT ki; [FieldOffset(32)] public long pad; }
  [DllImport("user32.dll", SetLastError=true)] public static extern uint SendInput(uint n, INPUT[] inputs, int size);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
  public static string Tap(ushort scan) {
    var d = new INPUT { type = 1 }; d.ki.wScan = scan; d.ki.dwFlags = 0x0008;
    var u = new INPUT { type = 1 }; u.ki.wScan = scan; u.ki.dwFlags = 0x0008 | 0x0002;
    uint a = SendInput(1, new[]{d}, Marshal.SizeOf(typeof(INPUT))); int e1 = Marshal.GetLastWin32Error();
    System.Threading.Thread.Sleep(70);
    uint b = SendInput(1, new[]{u}, Marshal.SizeOf(typeof(INPUT))); int e2 = Marshal.GetLastWin32Error();
    return "down=" + a + " up=" + b + " err=" + e1 + "/" + e2 + " size=" + Marshal.SizeOf(typeof(INPUT));
  }
}
"@
$p = Get-Process artofrally -ErrorAction SilentlyContinue
if (-not $p) { "game not running"; exit 1 }
$h = $p.MainWindowHandle
"main window: $h title='$($p.MainWindowTitle)'"
[KI]::ShowWindow($h, 9) | Out-Null
[KI]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 600
"foreground is game: $([KI]::GetForegroundWindow() -eq $h)"
$scans = $Seq.Split(',') | ForEach-Object { [int]$_.Trim() }
foreach ($s in $scans) { "tap $s : " + [KI]::Tap([uint16]$s); Start-Sleep -Milliseconds $DelayMs }
