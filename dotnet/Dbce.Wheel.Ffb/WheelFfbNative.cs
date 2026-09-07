using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

namespace Dbce.Wheel.Ffb
{
    /// <summary>
    /// Managed binding to <c>WheelFfb.dll</c>, the toolkit's native DirectInput
    /// force-feedback and device layer.
    /// </summary>
    /// <remarks>Exports are bound once from the exact loaded module. Renamed plugins work
    /// on Mono and CoreCLR without an import-name resolver. Successful modules stay pinned
    /// for the process lifetime; Shutdown releases devices, not the binding.</remarks>
    public static class WheelFfbNative
    {

        /// <summary>Full scale of <see cref="SetForce"/>: DirectInput's nominal maximum.</summary>
        public const int ForceMax = 10000;

        [DllImport("kernel32", SetLastError = true, CharSet = CharSet.Unicode)]
        private static extern IntPtr LoadLibraryW(string path);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private delegate int InitDirectInputCall(int hwnd);
        private static InitDirectInputCall InitDirectInput;
        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private delegate void AquireCall();
        private static AquireCall Aquire;
        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private delegate int SetDeviceForcesXYCall(int x, int y);
        private static SetDeviceForcesXYCall SetDeviceForcesXY;
        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private delegate int StartEffectCall();
        private static StartEffectCall StartEffect;
        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private delegate int StopEffectCall();
        private static StopEffectCall StopEffect;
        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private delegate int SetAutoCenterCall(int enable);
        private static SetAutoCenterCall SetAutoCenter;
        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private delegate void FreeDirectInputCall();
        private static FreeDirectInputCall FreeDirectInput;
        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private delegate void SetPreferredDeviceCall(string name);
        private static SetPreferredDeviceCall SetPreferredDevice;
        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private delegate void SetPreferredDeviceGuidCall(byte[] guid16);
        private static SetPreferredDeviceGuidCall SetPreferredDeviceGuid;
        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private delegate void SetPreferredDeviceIndexCall(int index);
        private static SetPreferredDeviceIndexCall SetPreferredDeviceIndex;
        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private delegate int EnumerateDevicesCall();
        private static EnumerateDevicesCall EnumerateDevices;
        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private delegate int GetDeviceNameCall(int index, StringBuilder buffer, int size);
        private static GetDeviceNameCall GetDeviceName;
        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private delegate int GetDeviceInfoCall(int index, out int axes, out int buttons);
        private static GetDeviceInfoCall GetDeviceInfo;
        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private delegate int EnumerateAllDevicesCall();
        private static EnumerateAllDevicesCall EnumerateAllDevices;
        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private delegate int GetAnyDeviceNameCall(int index, StringBuilder buffer, int size);
        private static GetAnyDeviceNameCall GetAnyDeviceName;
        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private delegate int GetAnyDeviceInfoCall(int index, out int axes, out int buttons, out int ffb);
        private static GetAnyDeviceInfoCall GetAnyDeviceInfo;
        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private delegate int OpenAuxDeviceCall(int index);
        private static OpenAuxDeviceCall OpenAuxDevice;
        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private delegate int ReadAuxButtonsCall([Out] byte[] buffer, int length);
        private static ReadAuxButtonsCall ReadAuxButtons;
        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private delegate void CloseAuxDeviceCall();
        private static CloseAuxDeviceCall CloseAuxDevice;
        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private delegate int OpenReadDeviceCall(int index);
        private static OpenReadDeviceCall OpenReadDevice;
        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private delegate int ReadDeviceStateCall(int slot, [Out] int[] axes, [Out] byte[] buttons, int buttonCount);
        private static ReadDeviceStateCall ReadDeviceState;
        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private delegate void CloseReadDevicesCall();
        private static CloseReadDevicesCall CloseReadDevices;
        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private delegate void PanicStopCall();
        private static PanicStopCall PanicStop;
        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private delegate void ZeroForcesCall();
        private static ZeroForcesCall ZeroForces;
        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private delegate int InstallExitGuardsCall();
        private static InstallExitGuardsCall InstallExitGuards;
        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private delegate void SetHoldTimeoutMsCall(int ms);
        private static SetHoldTimeoutMsCall SetHoldTimeoutMs;
        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private delegate void SetLogPathCall(string path);
        private static SetLogPathCall SetLogPath;
        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private delegate int GetLastHResultCall();
        private static GetLastHResultCall GetLastHResult;
        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private delegate int GetWheelFfbVersionCall();
        private static GetWheelFfbVersionCall GetWheelFfbVersion;
        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private delegate int CreatePeriodicEffectCall(int freqHz);
        private static CreatePeriodicEffectCall CreatePeriodicEffect;
        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private delegate int UpdatePeriodicEffectCall(int slot, int magnitude, int freqMilliHz);
        private static UpdatePeriodicEffectCall UpdatePeriodicEffect;
        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private delegate void ReleasePeriodicEffectsCall();
        private static ReleasePeriodicEffectsCall ReleasePeriodicEffects;
        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private delegate int GetDeviceGuidCall(int index, [Out] byte[] guid16);
        private static GetDeviceGuidCall GetDeviceGuid;
        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private delegate int GetAnyDeviceGuidCall(int index, [Out] byte[] guid16);
        private static GetAnyDeviceGuidCall GetAnyDeviceGuid;
        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private delegate int CreateConditionEffectCall(int type);
        private static CreateConditionEffectCall CreateConditionEffect;
        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private delegate int UpdateConditionEffectCall(int slot, int coefficient, int saturation, int deadband, int offset);
        private static UpdateConditionEffectCall UpdateConditionEffect;
        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private delegate void ReleaseConditionEffectsCall();
        private static ReleaseConditionEffectsCall ReleaseConditionEffects;
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate void StrictSelectionCall(int enabled);
        private static StrictSelectionCall SetStrictDeviceSelection;
        [DllImport("kernel32", CharSet = CharSet.Ansi, ExactSpelling = true)]
        private static extern IntPtr GetProcAddress(IntPtr module, string name);
        [DllImport("kernel32")] private static extern bool FreeLibrary(IntPtr module);
        private static readonly object LoadLock = new object();
        private static Guid? _pendingGuid;
        private static bool _needsCleanup;

        private static bool _loaded, _initialised, _failed;

        /// <summary>Absolute path the DLL was loaded from, or null.</summary>
        public static string LoadedFrom { get; private set; }
        /// <summary>Last failure, for panels and support bundles.</summary>
        public static string LastError { get; private set; }
        /// <summary>True once the device is open and the effect created.</summary>
        public static bool Ready => _initialised && !_failed;

        /// <summary>
        /// Loads <c>WheelFfb.dll</c> from <paramref name="directory"/>. Call once,
        /// before anything else; safe to call again.
        /// </summary>
        public static bool Load(string directory, string fileName = "WheelFfb.dll")
        {
            lock (LoadLock)
            {
                IntPtr h = IntPtr.Zero;
                try
                {
                    string path = Path.GetFullPath(Path.Combine(directory ?? "", fileName));
                    if (_loaded)
                    {
                        if (string.Equals(path, LoadedFrom, StringComparison.OrdinalIgnoreCase)) return true;
                        LastError = "A different native module is already bound: " + LoadedFrom;
                        return false;
                    }
                    if (!File.Exists(path)) { LastError = "Native plugin not found: " + path; return false; }
                    h = LoadLibraryW(path);
                    if (h == IntPtr.Zero) { LastError = "LoadLibrary failed (" + Marshal.GetLastWin32Error() + "): " + path; return false; }
                InitDirectInput = Bind<InitDirectInputCall>(h, "InitDirectInput");
                Aquire = Bind<AquireCall>(h, "Aquire");
                SetDeviceForcesXY = Bind<SetDeviceForcesXYCall>(h, "SetDeviceForcesXY");
                StartEffect = Bind<StartEffectCall>(h, "StartEffect");
                StopEffect = Bind<StopEffectCall>(h, "StopEffect");
                SetAutoCenter = Bind<SetAutoCenterCall>(h, "SetAutoCenter");
                FreeDirectInput = Bind<FreeDirectInputCall>(h, "FreeDirectInput");
                SetPreferredDevice = Bind<SetPreferredDeviceCall>(h, "SetPreferredDevice");
                SetPreferredDeviceGuid = Bind<SetPreferredDeviceGuidCall>(h, "SetPreferredDeviceGuid");
                SetPreferredDeviceIndex = Bind<SetPreferredDeviceIndexCall>(h, "SetPreferredDeviceIndex");
                EnumerateDevices = Bind<EnumerateDevicesCall>(h, "EnumerateDevices");
                GetDeviceName = Bind<GetDeviceNameCall>(h, "GetDeviceName");
                GetDeviceInfo = Bind<GetDeviceInfoCall>(h, "GetDeviceInfo");
                EnumerateAllDevices = Bind<EnumerateAllDevicesCall>(h, "EnumerateAllDevices");
                GetAnyDeviceName = Bind<GetAnyDeviceNameCall>(h, "GetAnyDeviceName");
                GetAnyDeviceInfo = Bind<GetAnyDeviceInfoCall>(h, "GetAnyDeviceInfo");
                OpenAuxDevice = Bind<OpenAuxDeviceCall>(h, "OpenAuxDevice");
                ReadAuxButtons = Bind<ReadAuxButtonsCall>(h, "ReadAuxButtons");
                CloseAuxDevice = Bind<CloseAuxDeviceCall>(h, "CloseAuxDevice");
                OpenReadDevice = Bind<OpenReadDeviceCall>(h, "OpenReadDevice");
                ReadDeviceState = Bind<ReadDeviceStateCall>(h, "ReadDeviceState");
                CloseReadDevices = Bind<CloseReadDevicesCall>(h, "CloseReadDevices");
                PanicStop = Bind<PanicStopCall>(h, "PanicStop");
                ZeroForces = Bind<ZeroForcesCall>(h, "ZeroForces");
                InstallExitGuards = Bind<InstallExitGuardsCall>(h, "InstallExitGuards");
                SetHoldTimeoutMs = Bind<SetHoldTimeoutMsCall>(h, "SetHoldTimeoutMs");
                SetLogPath = Bind<SetLogPathCall>(h, "SetLogPath");
                GetLastHResult = Bind<GetLastHResultCall>(h, "GetLastHResult");
                GetWheelFfbVersion = Bind<GetWheelFfbVersionCall>(h, "GetWheelFfbVersion");
                CreatePeriodicEffect = Bind<CreatePeriodicEffectCall>(h, "CreatePeriodicEffect");
                UpdatePeriodicEffect = Bind<UpdatePeriodicEffectCall>(h, "UpdatePeriodicEffect");
                ReleasePeriodicEffects = Bind<ReleasePeriodicEffectsCall>(h, "ReleasePeriodicEffects");
                GetDeviceGuid = Bind<GetDeviceGuidCall>(h, "GetDeviceGuid");
                GetAnyDeviceGuid = Bind<GetAnyDeviceGuidCall>(h, "GetAnyDeviceGuid");
                CreateConditionEffect = Bind<CreateConditionEffectCall>(h, "CreateConditionEffect");
                UpdateConditionEffect = Bind<UpdateConditionEffectCall>(h, "UpdateConditionEffect");
                ReleaseConditionEffects = Bind<ReleaseConditionEffectsCall>(h, "ReleaseConditionEffects");
                    // Additive native 0.5 export. Legacy selection works with 0.4;
                    // strict GUID selection requires this capability.
                    var strict = GetProcAddress(h, "SetStrictDeviceSelection");
                    SetStrictDeviceSelection = strict == IntPtr.Zero ? null :
                        Marshal.GetDelegateForFunctionPointer<StrictSelectionCall>(strict);
                    LoadedFrom = path; _loaded = true; LastError = null;
                    h = IntPtr.Zero; // Keep our successful module reference for the process lifetime.
                    return true;
                }
                catch (Exception ex) { LastError = "Load failed: " + ex.Message; return false; }
                finally { if (h != IntPtr.Zero) FreeLibrary(h); }
            }
        }

        private static T Bind<T>(IntPtr module, string name) where T : class
        {
            IntPtr address = GetProcAddress(module, name);
            if (address == IntPtr.Zero) throw new EntryPointNotFoundException(name);
            return (T)(object)Marshal.GetDelegateForFunctionPointer(address, typeof(T));
        }

        /// <summary>Redirects the native log before device initialisation.</summary>
        public static void LogTo(string path) { try { if (_loaded) SetLogPath(path); } catch (Exception ex) { LastError = ex.Message; } }

        /// <summary>Opens the preferred FFB device. Existing three-argument ABI is retained.</summary>
        public static bool Initialise(string preferredName = "", int preferredIndex = -1, int hwnd = 0)
            => Initialise(preferredName, preferredIndex, hwnd, _pendingGuid);

        /// <summary>An explicit GUID never falls back to a different wheel. Requires native 0.5.</summary>
        public static bool Initialise(string preferredName, int preferredIndex, int hwnd, Guid? preferredGuid)
        {
            if (!_loaded) { LastError = LastError ?? "Native plugin not loaded"; return false; }
            if (_initialised) { LastError = "Already initialised; use Reinitialise to change selection"; return Ready; }
            if (_needsCleanup) Shutdown();
            try
            {
                _failed = false;
                SetPreferredDevice(""); SetPreferredDeviceIndex(-1); SetPreferredDeviceGuid(null);
                SetStrictDeviceSelection?.Invoke(0);
                if (preferredGuid.HasValue)
                {
                    if (SetStrictDeviceSelection == null)
                    { LastError = "Strict GUID selection requires native 0.5 or later"; return false; }
                    var devices = ListFfbDevices();
                    if (new DevicePreference { InstanceGuid = preferredGuid }.Choose(devices) < 0)
                    { LastError = "Saved wheel is missing or ambiguous. Choose the device again."; return false; }
                    SetPreferredDeviceGuid(preferredGuid.Value.ToByteArray());
                    SetStrictDeviceSelection(1);
                }
                else if (preferredIndex >= 0) SetPreferredDeviceIndex(preferredIndex);
                else if (!string.IsNullOrEmpty(preferredName)) SetPreferredDevice(preferredName);
                _needsCleanup = true;
                _initialised = InitDirectInput(hwnd) != 0;
                if (!_initialised || StartEffect() == 0)
                {
                    string error = "FFB initialise/start failed (0x" + GetLastHResult().ToString("X8") + ")";
                    Shutdown(); LastError = error; return false;
                }
                LastError = null;
                return true;
            }
            catch (Exception ex) { Shutdown(); LastError = ex.Message; return false; }
        }

        public static bool Reinitialise(string preferredName, int preferredIndex, int hwnd = 0)
        { Shutdown(); return Initialise(preferredName, preferredIndex, hwnd); }
        public static bool Reinitialise(string preferredName, int preferredIndex, int hwnd, Guid? preferredGuid)
        { Shutdown(); return Initialise(preferredName, preferredIndex, hwnd, preferredGuid); }

        /// <summary>Signed force, -<see cref="ForceMax"/>..+<see cref="ForceMax"/>. Cheap; call every physics step.</summary>
        public static bool SetForce(int force)
        {
            if (!Ready) return false;
            try
            {
                bool ok = SetDeviceForcesXY(Math.Max(-ForceMax, Math.Min(ForceMax, force)), 0) != 0;
                if (!ok) LastError = "SetDeviceForcesXY failed (0x" + GetLastHResult().ToString("X8") + ")";
                return ok; // Native reacquisition may recover on the next frame.
            }
            catch (Exception ex) { _failed = true; LastError = ex.Message; return false; }
        }

        public static void Stop() { if (_needsCleanup) try { Result(StopEffect() != 0, "StopEffect"); } catch (Exception ex) { LastError = ex.Message; } }
        public static void Start() { if (Ready) try { Result(StartEffect() != 0, "StartEffect"); } catch (Exception ex) { LastError = ex.Message; } }
        public static void Zero()            { if (_needsCleanup) try { ZeroForces(); } catch (Exception ex) { LastError = ex.Message; } }
        public static void Panic()
        {
            if (!_needsCleanup) return;
            try { PanicStop(); } catch (Exception ex) { LastError = ex.Message; }
            _initialised = false; _failed = true;
            LastError = LastError ?? "Panic stopped force feedback; reinitialise to resume";
        }
        public static bool ExitGuards()      { if (!_initialised) return false; try { return InstallExitGuards() != 0; } catch (Exception ex) { LastError = ex.Message; return false; } }
        public static void HoldTimeout(int ms) { if (_loaded) try { SetHoldTimeoutMs(ms); } catch (Exception ex) { LastError = ex.Message; } }

        /// <summary>
        /// Pins device selection to a DirectInput instance GUID, which beats both
        /// index and name. Use it when your own input layer has already chosen a
        /// device: two Fanatec devices can carry the same name, so matching by
        /// name is guesswork exactly where it hurts. Null clears it. Toolkit 0.2.0+.
        /// </summary>
        public static void PreferGuid(Guid? instanceGuid)
        {
            if (!_loaded) return;
            _pendingGuid = instanceGuid;
        }
        public static void AutoCentre(bool on) { if (_initialised) try { SetAutoCenter(on ? 1 : 0); } catch (Exception ex) { LastError = ex.Message; } }
        public static int  LastHResult       { get { try { return _loaded ? GetLastHResult() : 0; } catch (Exception ex) { LastError = ex.Message; return 0; } } }
        public static int  Version           { get { try { return _loaded ? GetWheelFfbVersion() : 0; } catch (Exception ex) { LastError = ex.Message; return 0; } } }

        /// <summary>
        /// Creates a hardware sine effect (road texture, tyre slip) on the held
        /// wheel, initially silent. Returns the slot for <see cref="UpdatePeriodic"/>,
        /// or -1 when the driver has no periodic support - then synthesise at
        /// 15 Hz or below through <see cref="SetForce"/> instead. Toolkit 0.2.0+.
        /// </summary>
        public static int  CreatePeriodic(int freqHz)  { if (!Ready) return -1; try { return CreatePeriodicEffect(freqHz); } catch (Exception ex) { LastError = ex.Message; return -1; } }

        /// <summary>Envelope update; safe to call every frame (unchanged envelopes never reach the driver).</summary>
        public static bool UpdatePeriodic(int slot, float magnitude01, float freqHz)
        {
            if (!Ready || slot < 0) return false;
            try {
                int m = (int)Math.Round(Math.Max(0f, Math.Min(1f, magnitude01)) * 10000f);
                return Result(UpdatePeriodicEffect(slot, m, (int)Math.Round(freqHz * 1000f)) != 0, "UpdatePeriodicEffect");
            } catch (Exception ex) { LastError = ex.Message; return false; }
        }

        public static void ReleasePeriodics()  { if (_loaded) try { ReleasePeriodicEffects(); } catch (Exception ex) { LastError = ex.Message; } }

        /// <summary>
        /// Instance GUID of an enumerated force-feedback device, to hand back to
        /// <see cref="PreferGuid"/>. Without this a mod with no DirectInput layer
        /// of its own could not pin a device by identity, and matching by name
        /// picks the wrong one of a Fanatec base's two entries. Toolkit 0.4.0+.
        /// </summary>
        public static Guid? DeviceGuid(int index)
        {
            if (!_loaded) return null;
            try { var b = new byte[16]; return GetDeviceGuid(index, b) != 0 ? new Guid(b) : (Guid?)null; }
            catch (Exception ex) { LastError = ex.Message; return null; }
        }

        /// <summary>The same for the full controller list (shifters, pedal sets).</summary>
        public static Guid? AnyDeviceGuid(int index)
        {
            if (!_loaded) return null;
            try { var b = new byte[16]; return GetAnyDeviceGuid(index, b) != 0 ? new Guid(b) : (Guid?)null; }
            catch (Exception ex) { LastError = ex.Message; return null; }
        }

        /// <summary>Condition effect kind for <see cref="CreateCondition"/>.</summary>
        public enum ConditionKind { Spring = 0, Damper = 1, Inertia = 2, Friction = 3 }

        /// <summary>
        /// Creates a hardware condition effect - the base computes force from axis
        /// position or velocity continuously, rather than following our updates.
        /// A hardware damper is smoother and more stable than damping synthesised
        /// at 60 Hz, and is usually the right answer to a base that oscillates.
        /// Returns the slot, or -1 when the driver has no such effect (normal;
        /// fall back to damping in the model). Toolkit 0.4.0+.
        /// </summary>
        public static int CreateCondition(ConditionKind kind)
        {
            if (!Ready) return -1;
            try { return CreateConditionEffect((int)kind); } catch (Exception ex) { LastError = ex.Message; return -1; }
        }

        /// <summary>
        /// Updates a condition slot. All four are -1..1 (saturation and deadband
        /// 0..1); safe to call every frame, since an unchanged condition never
        /// reaches the driver.
        /// </summary>
        public static bool UpdateCondition(int slot, float coefficient, float saturation = 1f, float deadband = 0f, float offset = 0f)
        {
            if (!Ready || slot < 0) return false;
            try
            {
                int C(float v, bool signed) => (int)Math.Round(Math.Max(signed ? -1f : 0f, Math.Min(1f, v)) * 10000f);
                return Result(UpdateConditionEffect(slot, C(coefficient, true), C(saturation, false), C(deadband, false), C(offset, true)) != 0, "UpdateConditionEffect");
            }
            catch (Exception ex) { LastError = ex.Message; return false; }
        }

        public static void ReleaseConditions() { if (_loaded) try { ReleaseConditionEffects(); } catch (Exception ex) { LastError = ex.Message; } }

        /// <summary>Releases the device. Safe to call repeatedly.</summary>
        public static void Shutdown()
        {
            if (!_needsCleanup) return;
            try { ZeroForces(); } catch (Exception ex) { LastError = ex.Message; }
            try { StopEffect(); } catch (Exception ex) { LastError = ex.Message; }
            try { FreeDirectInput(); } catch (Exception ex) { LastError = ex.Message; }
            _initialised = false; _failed = false; _needsCleanup = false;
        }

        /// <summary>Final consumer shutdown, including all independently opened readers.
        /// Use Shutdown alone when switching FFB without closing input devices.</summary>
        public static void ShutdownAll()
        {
            if (!_loaded) return;
            Shutdown(); CloseAux(); CloseRead();
            try { FreeDirectInput(); } catch (Exception ex) { LastError = ex.Message; }
        }

        // --- device lists -----------------------------------------------------

        public struct DeviceInfo
        {
            public int Index; public string Name; public int Axes; public int Buttons; public bool ForceFeedback;
            public Guid? InstanceGuid;
            public int? VendorId, ProductId; // Unknown is not zero; supplied by a backend with product metadata.
            /// <summary>Name plus capabilities, so two same-named devices differ.</summary>
            public string Label => Name + "  (" + Axes + " axes, " + Buttons + " buttons" + (ForceFeedback ? ", force feedback" : "") + ")";
        }

        /// <summary>Force-feedback-capable devices, in DirectInput order.</summary>
        public static DeviceInfo[] ListFfbDevices()
        {
            if (!_loaded) return new DeviceInfo[0];
            try
            {
                int n = EnumerateDevices(); var list = new DeviceInfo[Math.Max(0, n)];
                var sb = new StringBuilder(260);
                for (int i = 0; i < n; i++)
                {
                    sb.Length = 0; GetDeviceName(i, sb, sb.Capacity);
                    GetDeviceInfo(i, out int a, out int b);
                    list[i] = new DeviceInfo { Index = i, Name = sb.ToString(), Axes = a, Buttons = b, ForceFeedback = true, InstanceGuid = DeviceGuid(i) };
                }
                return list;
            }
            catch (Exception ex) { LastError = ex.Message; return new DeviceInfo[0]; }
        }

        /// <summary>Every attached game controller, force feedback or not.</summary>
        public static DeviceInfo[] ListAllDevices()
        {
            if (!_loaded) return new DeviceInfo[0];
            try
            {
                int n = EnumerateAllDevices(); var list = new DeviceInfo[Math.Max(0, n)];
                var sb = new StringBuilder(260);
                for (int i = 0; i < n; i++)
                {
                    sb.Length = 0; GetAnyDeviceName(i, sb, sb.Capacity);
                    GetAnyDeviceInfo(i, out int a, out int b, out int f);
                    list[i] = new DeviceInfo { Index = i, Name = sb.ToString(), Axes = a, Buttons = b, ForceFeedback = f != 0, InstanceGuid = AnyDeviceGuid(i) };
                }
                return list;
            }
            catch (Exception ex) { LastError = ex.Message; return new DeviceInfo[0]; }
        }

        // --- reading other devices (shifters, pedals, a wheel the game cannot read) ---

        private static bool Result(bool ok, string operation)
        {
            if (!ok) LastError = operation + " failed (0x" + LastHResult.ToString("X8") + ")";
            return ok;
        }
        public static bool OpenAux(int index)
        {
            if (!_loaded || index < 0) { LastError = "OpenAux requires a loaded module and nonnegative index"; return false; }
            try { return Result(OpenAuxDevice(index) != 0, "OpenAux"); }
            catch (Exception ex) { LastError = ex.Message; return false; }
        }
        public static int ReadAux(byte[] buttons)
        {
            if (!_loaded || buttons == null || buttons.Length == 0) { LastError = "ReadAux requires a button buffer and loaded module"; return 0; }
            try { int n = ReadAuxButtons(buttons, buttons.Length); Result(n > 0, "ReadAux"); return n; }
            catch (Exception ex) { LastError = ex.Message; return 0; }
        }
        public static void CloseAux() { try { if (_loaded) CloseAuxDevice(); } catch (Exception ex) { LastError = ex.Message; } }
        public static int OpenRead(int index)
        {
            if (!_loaded || index < 0) { LastError = "OpenRead requires a loaded module and nonnegative index"; return -1; }
            try { int slot = OpenReadDevice(index); Result(slot >= 0, "OpenRead"); return slot; }
            catch (Exception ex) { LastError = ex.Message; return -1; }
        }
        public static bool Read(int slot, int[] axes8, byte[] buttons)
        {
            if (!_loaded || slot < 0 || axes8 == null || axes8.Length < 8) { LastError = "Read requires a valid slot and eight axes"; return false; }
            try { return Result(ReadDeviceState(slot, axes8, buttons, buttons?.Length ?? 0) != 0, "Read"); }
            catch (Exception ex) { LastError = ex.Message; return false; }
        }
        public static void CloseRead() { try { if (_loaded) CloseReadDevices(); } catch (Exception ex) { LastError = ex.Message; } }
    }
}
