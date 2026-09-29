using System;
using System.IO;
using System.Runtime.InteropServices;
using Dbce.Wheel.Ffb;
partial class Program
{
 static void Size<T>(int bytes){Check(Marshal.SizeOf(typeof(T))==bytes,"external size "+typeof(T).Name);}
 static void Offset<T>(string field,int offset){Check(Marshal.OffsetOf(typeof(T),field).ToInt32()==offset,"external offset "+typeof(T).Name+"."+field);}
 static int TestExternalBinding(string mode,string dir,string path)
 {
  Size<ExternalProcessIdentity>(24);Size<ExternalFfbConfig>(112);Size<ExternalFfbArmTicket>(56);
  Size<ExternalFfbClientFrame>(40);Size<ExternalFfbForceFrame>(64);Size<ExternalFfbStatus>(104);
  Offset<ExternalProcessIdentity>("Pid",16);Offset<ExternalFfbConfig>("Target",8);Offset<ExternalFfbConfig>("Client",32);
  Offset<ExternalFfbConfig>("Producer",56);Offset<ExternalFfbConfig>("WheelGuid",80);Offset<ExternalFfbConfig>("MaxMagnitude",96);
  Offset<ExternalFfbArmTicket>("IssuedQpc",32);Offset<ExternalFfbClientFrame>("CapturedQpc",24);
  Offset<ExternalFfbForceFrame>("CapturedQpc",40);Offset<ExternalFfbForceFrame>("SignedMagnitude",56);
  Offset<ExternalFfbStatus>("State",24);Offset<ExternalFfbStatus>("SourceSequence",40);Offset<ExternalFfbStatus>("DriverHResult",88);Offset<ExternalFfbStatus>("QpcFrequency",96);
  Check(WheelFfbNative.Version==1000 && WheelFfbNative.SupportsPovRead,"external module identity/input compatibility");
  var cfg=new ExternalFfbConfig {Target=new ExternalProcessIdentity{Handle=0x123456789ABCDEF0UL,CreationFileTime=0x1122334455667788UL,Pid=42},
   Client=new ExternalProcessIdentity{Pid=43},Producer=new ExternalProcessIdentity{Pid=44},WheelGuid=new Guid("11223344-5566-7788-99aa-bbccddeeffff"),
   MaxMagnitude=3000,SourceMaxAgeMs=150,ClientMaxAgeMs=250,PulseMaxMs=20};
  ulong session;ExternalFfbArmTicket ticket;ExternalFfbStatus status;
  if(mode=="externalpartial") {
   Check(!WheelFfbNative.SupportsExternalSessions,"seven exports incorrectly advertised full external support");
   Check(WheelFfbNative.EnterExternalSession(new IntPtr(123))==ExternalFfbResult.Unavailable,"partial enter");
   Check(WheelFfbNative.OpenExternalSession(cfg,"C:\\Game.exe",out session)==ExternalFfbResult.Unavailable && session==0,"partial open");
   Check(WheelFfbNative.ArmExternalSession(1,out ticket)==ExternalFfbResult.Unavailable,"partial arm");
   Check(WheelFfbNative.RenewExternalClient(1,new ExternalFfbClientFrame())==ExternalFfbResult.Unavailable,"partial client");
   Check(WheelFfbNative.SubmitExternalConstant(1,new ExternalFfbForceFrame())==ExternalFfbResult.Unavailable,"partial force");
   Check(WheelFfbNative.DisarmExternalSession(1)==ExternalFfbResult.Unavailable && WheelFfbNative.CloseExternalSession(1)==ExternalFfbResult.Unavailable,"partial cancellation");
   Check(WheelFfbNative.GetExternalStatus(1,out status)==ExternalFfbResult.Unavailable,"partial status");
  } else {
   Check(WheelFfbNative.SupportsExternalSessions,"external capability absent");
   Check(WheelFfbNative.EnterExternalSession(new IntPtr(123))==ExternalFfbResult.Ok,"HWND marshalling");
   Check(WheelFfbNative.OpenExternalSession(cfg,null,out session)==ExternalFfbResult.Invalid,"null path");
   Check(WheelFfbNative.OpenExternalSession(cfg,"C:\0bad",out session)==ExternalFfbResult.Invalid,"embedded path NUL");
   Check(WheelFfbNative.OpenExternalSession(cfg,"C:\\Game.exe",out session)==ExternalFfbResult.Ok && session==0xABCD123456789012UL,"config/path/session marshalling");
   Check(WheelFfbNative.ArmExternalSession(session,out ticket)==ExternalFfbResult.Waiting && ticket.Generation==0x123456789ABCUL && ticket.NonceLow==111 && ticket.NonceHigh==222 && ticket.IssuedQpc==0x123456789ABC && ticket.ExpiresQpc==ticket.IssuedQpc+1500000 && ticket.QpcFrequency==10000000,"ticket marshalling");
   var client=new ExternalFfbClientFrame{Flags=ExternalFfbClientFlags.All,Generation=ticket.Generation,Sequence=0xABC12345678UL,CapturedQpc=ticket.IssuedQpc+1,QpcFrequency=ticket.QpcFrequency};
   Check(WheelFfbNative.RenewExternalClient(session,client)==ExternalFfbResult.Ok,"client marshalling");
   var force=new ExternalFfbForceFrame{Attestations=ExternalFfbAttestations.All,Generation=ticket.Generation,NonceLow=ticket.NonceLow,NonceHigh=ticket.NonceHigh,Sequence=client.Sequence,CapturedQpc=client.CapturedQpc,QpcFrequency=client.QpcFrequency,SignedMagnitude=-2345};
   Check(WheelFfbNative.SubmitExternalConstant(session,force)==ExternalFfbResult.Applied,"force marshalling");
   force.Reserved=1;Check(WheelFfbNative.SubmitExternalConstant(session,force)==ExternalFfbResult.Invalid,"force reserved not silently removed");
   Check(WheelFfbNative.DisarmExternalSession(session)==ExternalFfbResult.Pending,"pending cancellation lost");
   Check(WheelFfbNative.CloseExternalSession(session)==ExternalFfbResult.Fault,"cleanup failure hidden");
   Check(WheelFfbNative.GetExternalStatus(session,out status)==ExternalFfbResult.Ok && status.Session==session && status.Generation==ticket.Generation && status.State==ExternalFfbState.SessionClosed && status.Reason==ExternalFfbReason.Driver && status.Cancellation==ExternalFfbCancellation.ApiFailed && status.Flags==(ExternalFfbStatusFlags.ModeEntered|ExternalFfbStatusFlags.PhysicalUnverified) && status.SourceSequence==9 && status.ClientSequence==10 && status.SourceDeadlineQpc==11 && status.ClientDeadlineQpc==12 && status.WheelReadQpc==13 && status.LastStartQpc==14 && status.DriverHResult==unchecked((int)0x80004005) && status.Reserved==0 && status.QpcFrequency==10000000,"status marshalling/cancellation/physical distinction");
   Check(!WheelFfbNative.Load(Path.GetFullPath(Path.Combine(dir,"..")),"UnityForceFeedback.dll") && WheelFfbNative.SupportsExternalSessions && WheelFfbNative.LoadedFrom==path,"older replacement changed optional external binding");
  }
  Console.WriteLine(mode+" optional binding/layout checks passed: "+assertions);return 0;
 }
}
