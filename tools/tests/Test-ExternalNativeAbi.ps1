param([Parameter(Mandatory=$true)][string]$Dll,[Parameter(Mandatory=$true)][string]$Definition)
$ErrorActionPreference='Stop'
$env:DBCE_FFB_LOG='0'
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class ExternalAbiCheck {
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr LoadLibraryW(string name);
 [DllImport("kernel32.dll",CharSet=CharSet.Ansi,ExactSpelling=true)] static extern IntPtr GetProcAddress(IntPtr module,string name);
 [DllImport("kernel32.dll")] static extern bool FreeLibrary(IntPtr module);
 [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int VersionCall();
 [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int ReadCall(int slot,[Out]int[] axes,int axisCount,[Out]byte[] buttons,int buttonCount,[Out]int[] pov,int povCount);
 [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int EnterCall(IntPtr h);
 [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int OpenCall(IntPtr config,IntPtr path,uint chars,IntPtr session);
 [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int FrameCall(ulong session,IntPtr frame);
 [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int DisarmCall(ulong session,uint reason);
 [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int CloseCall(ulong session);
 static T Function<T>(IntPtr module,string name) {return (T)(object)Marshal.GetDelegateForFunctionPointer(GetProcAddress(module,name),typeof(T));}
 public static int Verify(string path,string[] names) {
  var h=LoadLibraryW(path);if(h==IntPtr.Zero)throw new Exception("Load failed "+Marshal.GetLastWin32Error());
  try {
   foreach(var name in names)if(GetProcAddress(h,name)==IntPtr.Zero)throw new Exception("Missing "+name);
   var version=(VersionCall)Marshal.GetDelegateForFunctionPointer(GetProcAddress(h,"GetWheelFfbVersion"),typeof(VersionCall));
   int result=version();if(result!=1000)throw new Exception("Wrong version "+result);
   var read=(ReadCall)Marshal.GetDelegateForFunctionPointer(GetProcAddress(h,"ReadDeviceStateWithPov"),typeof(ReadCall));
   int[] axes={1,2,3,4,5,6,7,8},pov={0,9000,18000,27000};var buttons=new byte[128];
   for(int i=0;i<buttons.Length;++i)buttons[i]=1;
   if(read(-1,axes,8,buttons,128,pov,4)!=0)throw new Exception("Invalid slot accepted");
   foreach(int a in axes)if(a!=0)throw new Exception("Stale axes");
   foreach(byte b in buttons)if(b!=0)throw new Exception("Stale buttons");
   foreach(int p in pov)if(p!=-1)throw new Exception("Stale POV");
   if(Function<EnterCall>(h,"ExternalFfbEnter")(IntPtr.Zero)!=-1)throw new Exception("Invalid external HWND accepted");
   if(Function<OpenCall>(h,"ExternalFfbOpen")(IntPtr.Zero,IntPtr.Zero,0,IntPtr.Zero)!=-2)throw new Exception("Open bypassed opt-in");
   foreach(string name in new[]{"ExternalFfbArm","ExternalFfbRenewClient","ExternalFfbSubmitConstant","ExternalFfbGetStatus"})
    if(Function<FrameCall>(h,name)(0,IntPtr.Zero)!=-2)throw new Exception(name+" bypassed opt-in");
   if(Function<DisarmCall>(h,"ExternalFfbDisarm")(0,1)!=-2 || Function<CloseCall>(h,"ExternalFfbClose")(0)!=-2)throw new Exception("Lifecycle bypassed opt-in");
   return result;
  } finally {FreeLibrary(h);}
 }
}
'@
$names=@(Get-Content -LiteralPath $Definition | Where-Object {$_ -match '^\s+[A-Za-z_][A-Za-z0-9_]*\s*$'} | ForEach-Object {$_.Trim()})
$version=[ExternalAbiCheck]::Verify([IO.Path]::GetFullPath($Dll),[string[]]$names)
[ordered]@{bits=[IntPtr]::Size*8;version=$version;exports=$names.Count;sha256=(Get-FileHash -LiteralPath $Dll -Algorithm SHA256).Hash;invalidSlotClears=$true;externalOptInGate=$true;deviceEnumerated=$false;deviceAcquired=$false;hardwareOutput=$false;scope='Exact DLL load, resolve, version and invalid-slot and external opt-in rejection calls only; not hardware smoke'} | ConvertTo-Json
