using System;
using System.Runtime.InteropServices;

namespace Dbce.Wheel.Ffb
{
    /// <summary>Experimental native 0.10 result codes. Pending is not completed cleanup.</summary>
    public enum ExternalFfbResult { Ok=1, Waiting=2, Applied=3, Canceled=4, Closed=5, Pending=6,
        Invalid=-1, WrongMode=-2, WrongSession=-3, Stale=-4, Busy=-5, Fault=-6, Ineligible=-7, Unavailable=-8 }
    public enum ExternalFfbState : uint { OwnerOnly, Prepared, Arming, Armed, Revoked, SessionClosed, Faulted }
    public enum ExternalFfbReason : uint { None, Stop, Focus, TargetExit, ClientExit, ProducerExit, SourceStale,
        ClientStale, Input, Window, Driver, SourceInvalid, ClientOff, Close, LegacyCancel, Clock }
    public enum ExternalFfbCancellation : uint { None, Requested, ApiDone, ApiFailed }
    [Flags] public enum ExternalFfbStatusFlags : uint { ModeEntered=1, OwnerValid=2, DriverBusy=4, PhysicalUnverified=8, ClosePending=16 }
    [Flags] public enum ExternalFfbAttestations : uint { LocalPlayer=1, Driving=2, KnownMode=4, RequiredInputs=8, All=15 }
    [Flags] public enum ExternalFfbClientFlags : uint { Enabled=1, InputsValid=2, All=3 }

    /// <summary>Handle valid in the owner process, plus exact PID/creation FILETIME. Native duplicates it.</summary>
    [StructLayout(LayoutKind.Sequential, Pack=8)]
    public struct ExternalProcessIdentity
    {
        public ulong Handle, CreationFileTime;
        public uint Pid, Reserved;
    }
    /// <summary>Immutable session limits; Guid uses the Windows instance-GUID byte layout.</summary>
    [StructLayout(LayoutKind.Sequential, Pack=8)]
    public struct ExternalFfbConfig
    {
        public uint CbSize, Version;
        public ExternalProcessIdentity Target, Client, Producer;
        public Guid WheelGuid;
        public uint MaxMagnitude, SourceMaxAgeMs, ClientMaxAgeMs, PulseMaxMs;
    }
    /// <summary>Challenge must reach the real producer before it captures state. Expiry bounds initial echo only.</summary>
    [StructLayout(LayoutKind.Sequential, Pack=8)]
    public struct ExternalFfbArmTicket
    {
        public uint CbSize, Version;
        public ulong Generation, NonceLow, NonceHigh;
        public long IssuedQpc, ExpiresQpc, QpcFrequency;
    }
    [StructLayout(LayoutKind.Sequential, Pack=8)]
    public struct ExternalFfbClientFrame
    {
        public uint CbSize;
        public ExternalFfbClientFlags Flags;
        public ulong Generation, Sequence;
        public long CapturedQpc, QpcFrequency;
    }
    /// <summary>One genuine source capture allows at most one finite constant pulse; no replay or retry.</summary>
    [StructLayout(LayoutKind.Sequential, Pack=8)]
    public struct ExternalFfbForceFrame
    {
        public uint CbSize;
        public ExternalFfbAttestations Attestations;
        public ulong Generation, NonceLow, NonceHigh, Sequence;
        public long CapturedQpc, QpcFrequency;
        public int SignedMagnitude;
        public uint Reserved;
    }
    /// <summary>API cancellation is not proof of physical zero; OEM autocentre can resume on orderly Close.</summary>
    [StructLayout(LayoutKind.Sequential, Pack=8)]
    public struct ExternalFfbStatus
    {
        public uint CbSize, Version;
        public ulong Session, Generation;
        public ExternalFfbState State;
        public ExternalFfbReason Reason;
        public ExternalFfbCancellation Cancellation;
        public ExternalFfbStatusFlags Flags;
        public ulong SourceSequence, ClientSequence;
        public long SourceDeadlineQpc, ClientDeadlineQpc, WheelReadQpc, LastStartQpc;
        public int DriverHResult;
        public uint Reserved;
        public long QpcFrequency;
    }

    public static partial class WheelFfbNative
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate int ExternalEnterCall(IntPtr window);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet=CharSet.Unicode)]
        private delegate int ExternalOpenCall(ref ExternalFfbConfig config, [MarshalAs(UnmanagedType.LPWStr)] string path, uint pathChars, out ulong session);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate int ExternalArmCall(ulong session, ref ExternalFfbArmTicket ticket);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate int ExternalClientCall(ulong session, ref ExternalFfbClientFrame frame);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate int ExternalForceCall(ulong session, ref ExternalFfbForceFrame frame);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate int ExternalDisarmCall(ulong session, uint reason);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate int ExternalCloseCall(ulong session);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate int ExternalStatusCall(ulong session, ref ExternalFfbStatus status);
        private static ExternalEnterCall _externalEnter;
        private static ExternalOpenCall _externalOpen;
        private static ExternalArmCall _externalArm;
        private static ExternalClientCall _externalClient;
        private static ExternalForceCall _externalForce;
        private static ExternalDisarmCall _externalDisarm;
        private static ExternalCloseCall _externalClose;
        private static ExternalStatusCall _externalStatus;
        /// <summary>All eight optional exports on the exact bound module. Older DLLs remain loadable.</summary>
        public static bool SupportsExternalSessions => _loaded && _externalEnter!=null && _externalOpen!=null &&
            _externalArm!=null && _externalClient!=null && _externalForce!=null && _externalDisarm!=null && _externalClose!=null && _externalStatus!=null;
        private static void BindExternalSessions(IntPtr module)
        {
            _externalEnter=Optional<ExternalEnterCall>(module,"ExternalFfbEnter");
            _externalOpen=Optional<ExternalOpenCall>(module,"ExternalFfbOpen");
            _externalArm=Optional<ExternalArmCall>(module,"ExternalFfbArm");
            _externalClient=Optional<ExternalClientCall>(module,"ExternalFfbRenewClient");
            _externalForce=Optional<ExternalForceCall>(module,"ExternalFfbSubmitConstant");
            _externalDisarm=Optional<ExternalDisarmCall>(module,"ExternalFfbDisarm");
            _externalClose=Optional<ExternalCloseCall>(module,"ExternalFfbClose");
            _externalStatus=Optional<ExternalStatusCall>(module,"ExternalFfbGetStatus");
        }
        private static ExternalFfbResult ExternalError(Exception error)
        {LastError="External session: "+error.Message;return ExternalFfbResult.Unavailable;}
        /// <summary>Sticky process-lifetime opt-in. Call on an owned top-level HWND thread before concurrent toolkit use.</summary>
        public static ExternalFfbResult EnterExternalSession(IntPtr ownerWindow)
        {
            if(!SupportsExternalSessions)return ExternalFfbResult.Unavailable;
            try{return (ExternalFfbResult)_externalEnter(ownerWindow);}catch(Exception ex){return ExternalError(ex);}
        }
        /// <summary>Opens strict GUID idle finite output. Close all readers first; reopen by saved GUID after success.</summary>
        public static ExternalFfbResult OpenExternalSession(ExternalFfbConfig config,string targetImagePath,out ulong session)
        {
            session=0;if(!SupportsExternalSessions)return ExternalFfbResult.Unavailable;
            if(string.IsNullOrEmpty(targetImagePath) || targetImagePath.Length>32767 || targetImagePath.IndexOf('\0')>=0)return ExternalFfbResult.Invalid;
            config.CbSize=112;config.Version=1;
            try{var result=(ExternalFfbResult)_externalOpen(ref config,targetImagePath,(uint)targetImagePath.Length,out session);
                if(session!=0)_needsCleanup=true;return result;}catch(Exception ex){return ExternalError(ex);}
        }
        /// <summary>Explicit rearm handshake, not user permission. No cached input/game frame may be restamped.</summary>
        public static ExternalFfbResult ArmExternalSession(ulong session,out ExternalFfbArmTicket ticket)
        {
            ticket=new ExternalFfbArmTicket{CbSize=56,Version=1};if(!SupportsExternalSessions)return ExternalFfbResult.Unavailable;
            try{return (ExternalFfbResult)_externalArm(session,ref ticket);}catch(Exception ex){return ExternalError(ex);}
        }
        public static ExternalFfbResult RenewExternalClient(ulong session,ExternalFfbClientFrame frame)
        {
            if(!SupportsExternalSessions)return ExternalFfbResult.Unavailable;frame.CbSize=40;
            try{return (ExternalFfbResult)_externalClient(session,ref frame);}catch(Exception ex){return ExternalError(ex);}
        }
        public static ExternalFfbResult SubmitExternalConstant(ulong session,ExternalFfbForceFrame frame)
        {
            if(!SupportsExternalSessions)return ExternalFfbResult.Unavailable;frame.CbSize=64;
            try{return (ExternalFfbResult)_externalForce(session,ref frame);}catch(Exception ex){return ExternalError(ex);}
        }
        public static ExternalFfbResult DisarmExternalSession(ulong session,ExternalFfbReason reason=ExternalFfbReason.Stop)
        {
            if(!SupportsExternalSessions)return ExternalFfbResult.Unavailable;
            try{return (ExternalFfbResult)_externalDisarm(session,(uint)reason);}catch(Exception ex){return ExternalError(ex);}
        }
        /// <summary>Pending requires status polling while keeping HWND/process alive. Fault retains cleanup failure.</summary>
        public static ExternalFfbResult CloseExternalSession(ulong session)
        {
            if(!SupportsExternalSessions)return ExternalFfbResult.Unavailable;
            try{return (ExternalFfbResult)_externalClose(session);}catch(Exception ex){return ExternalError(ex);}
        }
        /// <summary>Does not enter the device gate. PhysicalUnverified is always set.</summary>
        public static ExternalFfbResult GetExternalStatus(ulong session,out ExternalFfbStatus status)
        {
            status=new ExternalFfbStatus{CbSize=104,Version=1};if(!SupportsExternalSessions)return ExternalFfbResult.Unavailable;
            try{return (ExternalFfbResult)_externalStatus(session,ref status);}catch(Exception ex){return ExternalError(ex);}
        }
    }
}
