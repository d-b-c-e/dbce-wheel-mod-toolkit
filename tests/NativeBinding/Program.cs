using System;
using System.IO;
using System.Runtime.InteropServices;
using Dbce.Wheel.Ffb;
class Program {
 static int assertions;
 static void Check(bool ok,string message) { assertions++; if(!ok) throw new Exception(message); }
 [DllImport("kernel32",CharSet=CharSet.Unicode)] static extern IntPtr GetModuleHandleW(string path);
 [DllImport("kernel32",CharSet=CharSet.Ansi,ExactSpelling=true)] static extern IntPtr GetProcAddress(IntPtr h,string name);
 [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int Metric(int key);
 [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate void Fault(int value);
 static int Main(string[] args) { try {
  string path=Path.GetFullPath(args[0]); string dir=Path.GetDirectoryName(path);
  Check(!WheelFfbNative.Load(dir,"missing.dll"),"missing DLL accepted");
  Check(!WheelFfbNative.Load(Environment.SystemDirectory,"kernel32.dll"),"incomplete exports accepted");
  Check(WheelFfbNative.Load(dir,Path.GetFileName(path)),WheelFfbNative.LastError);
  Check(WheelFfbNative.Load(dir,Path.GetFileName(path)),"repeat load");
  Check(!WheelFfbNative.Load(dir,"WheelFfb.dll"),"different module silently accepted");
  var h=GetModuleHandleW(path);
  var metric=Marshal.GetDelegateForFunctionPointer<Metric>(GetProcAddress(h,"TestMetric"));
  var fault=Marshal.GetDelegateForFunctionPointer<Fault>(GetProcAddress(h,"TestFault"));
  Check(metric(0)==0 && !WheelFfbNative.Ready,"Load acquired device");
  var all=WheelFfbNative.ListAllDevices();
  Check(all.Length==2 && all[1].Axes==8 && all[1].InstanceGuid.HasValue,"device output marshalling");
  Check(WheelFfbNative.OpenRead(0)==0,"input without FFB");
  var axes=new int[8];var buttons=new byte[128];
  Check(WheelFfbNative.Read(0,axes,buttons) && axes[7]==107 && buttons[0]==1,"read marshalling");
  int reads=metric(7);
  Check(!WheelFfbNative.Read(0,new int[7],buttons) && metric(7)==reads,"short axes crossed native boundary");
  Check(WheelFfbNative.OpenAux(0) && WheelFfbNative.ReadAux(buttons)==128,"aux without FFB");
  WheelFfbNative.ShutdownAll();Check(metric(1)>0,"input-only final cleanup missed DI instance");
  Check(WheelFfbNative.Initialise("same",-1,0),"name init");
  Check(metric(3)==1 && metric(4)==-1 && metric(5)==0,"name selectors");
  Check(WheelFfbNative.Reinitialise("",1,0,null),"index init");
  Check(metric(3)==0 && metric(4)==1 && metric(5)==0 && metric(6)==0,"index retained old selector");
  Check(WheelFfbNative.Reinitialise("",-1,0,all[1].InstanceGuid),"GUID init");
  Check(metric(3)==0 && metric(4)==-1 && metric(5)==1 && metric(6)==1,"strict GUID selectors");
  Check(WheelFfbNative.SetForce(20000) && metric(2)==10000,"force not clamped");
  WheelFfbNative.Panic();Check(!WheelFfbNative.Ready && !WheelFfbNative.SetForce(1),"Panic stayed ready");
  Check(WheelFfbNative.Initialise("",-1,0,null),"Panic reinit");
  Check(metric(3)==0 && metric(4)==-1 && metric(5)==0 && metric(6)==0,"automatic retained selector");
  WheelFfbNative.Shutdown();int count=metric(0);
  Check(!WheelFfbNative.Initialise("",-1,0,Guid.NewGuid()) && metric(0)==count,"missing GUID acquired another device");
  int freed=metric(1);fault(1);
  Check(!WheelFfbNative.Initialise() && !WheelFfbNative.Ready && metric(1)>freed,"init failure leaked cleanup");
  fault(2);freed=metric(1);
  Check(!WheelFfbNative.Initialise() && !WheelFfbNative.Ready && metric(1)>freed,"start failure reported ready");
  fault(0);Check(WheelFfbNative.Initialise(),"recovery");
  fault(3);Check(!WheelFfbNative.SetForce(2) && WheelFfbNative.LastError.Contains("SetDeviceForcesXY"),"force failure lost error");
  fault(0);Check(WheelFfbNative.SetForce(-20000) && metric(2)==-10000,"transient force recovery");
  WheelFfbNative.ShutdownAll();freed=metric(1);WheelFfbNative.Shutdown();Check(metric(1)==freed,"duplicate device shutdown");
  Console.WriteLine("{\"status\":\"passed\",\"assertions\":"+assertions+"}");return 0;
 } catch(Exception e) {Console.Error.WriteLine(e);return 1;} }
}