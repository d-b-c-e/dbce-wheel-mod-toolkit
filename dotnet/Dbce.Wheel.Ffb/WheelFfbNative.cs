using System;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;

namespace Dbce.Wheel.Ffb
{
    /// <summary>
    /// Managed binding to <c>WheelFfb.dll</c>, the toolkit's native DirectInput
    /// force-feedback and device layer.
    /// </summary>
    /// <remarks>
    /// <para>
    /// The DLL is loaded by <b>absolute path</b> before any <c>DllImport</c>
    /// resolves: Unity Mod Manager mods and BepInEx plugins live in folders that
    /// are not on the loader's search path. On Mono a <c>LoadLibraryW</c> preload
    /// is enough (a later import by base name finds the loaded module); on CoreCLR
    /// a <c>NativeLibrary.SetDllImportResolver</c> is installed through reflection
    /// when the API exists, so the same assembly serves both runtimes.
    /// </para>
    /// <para>
    /// Every call is guarded: a missing DLL or export records <see cref="LastError"/>
    /// and leaves the game running without force feedback rather than throwing
    /// inside a physics step.
    /// </para>
    /// </remarks>
    public static class WheelFfbNative
    {
        private const string Dll = "WheelFfb";

        /// <summary>Full scale of <see cref="SetForce"/>: DirectInput's nominal maximum.</summary>
        public const int ForceMax = 10000;

        [DllImport("kernel32", SetLastError = true, CharSet = CharSet.Unicode)]
        private static extern IntPtr LoadLibraryW(string path);

        [DllImport(Dll)] private static extern int  InitDirectInput(int hwnd);
        [DllImport(Dll)] private static extern void Aquire();
        [DllImport(Dll)] private static extern int  SetDeviceForcesXY(int x, int y);
        [DllImport(Dll)] private static extern int  StartEffect();
        [DllImport(Dll)] private static extern int  StopEffect();
        [DllImport(Dll)] private static extern int  SetAutoCenter(int enable);
        [DllImport(Dll)] private static extern void FreeDirectInput();
        [DllImport(Dll, CharSet = CharSet.Ansi)] private static extern void SetPreferredDevice(string name);
        [DllImport(Dll)] private static extern void SetPreferredDeviceGuid(byte[] guid16);
        [DllImport(Dll)] private static extern void SetPreferredDeviceIndex(int index);
        [DllImport(Dll)] private static extern int  EnumerateDevices();
        [DllImport(Dll, CharSet = CharSet.Ansi)] private static extern int GetDeviceName(int index, StringBuilder buffer, int size);
        [DllImport(Dll)] private static extern int  GetDeviceInfo(int index, out int axes, out int buttons);
        [DllImport(Dll)] private static extern int  EnumerateAllDevices();
        [DllImport(Dll, CharSet = CharSet.Ansi)] private static extern int GetAnyDeviceName(int index, StringBuilder buffer, int size);
        [DllImport(Dll)] private static extern int  GetAnyDeviceInfo(int index, out int axes, out int buttons, out int ffb);
        [DllImport(Dll)] private static extern int  OpenAuxDevice(int index);
        [DllImport(Dll)] private static extern int  ReadAuxButtons(byte[] buffer, int length);
        [DllImport(Dll)] private static extern void CloseAuxDevice();
        [DllImport(Dll)] private static extern int  OpenReadDevice(int index);
        [DllImport(Dll)] private static extern int  ReadDeviceState(int slot, int[] axes, byte[] buttons, int buttonCount);
        [DllImport(Dll)] private static extern void CloseReadDevices();
        [DllImport(Dll)] private static extern void PanicStop();
        [DllImport(Dll)] private static extern void ZeroForces();
        [DllImport(Dll)] private static extern int  InstallExitGuards();
        [DllImport(Dll)] private static extern void SetHoldTimeoutMs(int ms);
        [DllImport(Dll, CharSet = CharSet.Ansi)] private static extern void SetLogPath(string path);
        [DllImport(Dll)] private static extern int  GetLastHResult();
        [DllImport(Dll)] private static extern int  GetWheelFfbVersion();
        [DllImport(Dll)] private static extern int  CreatePeriodicEffect(int freqHz);
        [DllImport(Dll)] private static extern int  UpdatePeriodicEffect(int slot, int magnitude, int freqMilliHz);
        [DllImport(Dll)] private static extern void ReleasePeriodicEffects();
        [DllImport(Dll)] private static extern int  GetDeviceGuid(int index, byte[] guid16);
        [DllImport(Dll)] private static extern int  GetAnyDeviceGuid(int index, byte[] guid16);
        [DllImport(Dll)] private static extern int  CreateConditionEffect(int type);
        [DllImport(Dll)] private static extern int  UpdateConditionEffect(int slot, int coefficient, int saturation, int deadband, int offset);
        [DllImport(Dll)] private static extern void ReleaseConditionEffects();

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
            if (_loaded) return true;
            try
            {
                string path = Path.Combine(directory ?? "", fileName);
                if (!File.Exists(path)) { LastError = "Native plugin not found: " + path; return false; }
                InstallResolver(path);
                IntPtr h = LoadLibraryW(path);
                if (h == IntPtr.Zero) { LastError = "LoadLibrary failed (" + Marshal.GetLastWin32Error() + "): " + path; return false; }
                LoadedFrom = path;
                _loaded = true;
                return true;
            }
            catch (Exception ex) { LastError = "Load failed: " + ex.Message; return false; }
        }

        // CoreCLR (BepInEx 6): map the import name to the absolute path. Done via
        // reflection so this netstandard2.0 assembly still compiles and loads on Mono.
        private static void InstallResolver(string path)
        {
            try
            {
                var t = Type.GetType("System.Runtime.InteropServices.NativeLibrary, System.Runtime.InteropServices", false)
                     ?? Type.GetType("System.Runtime.InteropServices.NativeLibrary", false);
                if (t == null) return;
                var setResolver = t.GetMethod("SetDllImportResolver", BindingFlags.Public | BindingFlags.Static);
                var load = t.GetMethod("Load", new[] { typeof(string) });
                if (setResolver == null || load == null) return;
                var resolverType = setResolver.GetParameters()[1].ParameterType;   // DllImportResolver
                var handler = new Func<string, Assembly, DllImportSearchPath?, IntPtr>((name, asm, sp) =>
                    string.Equals(name, Dll, StringComparison.OrdinalIgnoreCase) ? (IntPtr)load.Invoke(null, new object[] { path }) : IntPtr.Zero);
                var del = Delegate.CreateDelegate(resolverType, handler.Target, handler.Method);
                setResolver.Invoke(null, new object[] { typeof(WheelFfbNative).Assembly, del });
            }
            catch { /* Mono, or an older runtime: the preload is sufficient there */ }
        }

        /// <summary>Redirects the native log (call before <see cref="Initialise"/> to keep a consumer's own folder).</summary>
        public static void LogTo(string path) { try { if (_loaded) SetLogPath(path); } catch (Exception ex) { LastError = ex.Message; } }

        /// <summary>Opens the preferred FFB device (index wins over name) and creates the effect.</summary>
        public static bool Initialise(string preferredName = "", int preferredIndex = -1, int hwnd = 0)
        {
            if (!_loaded) { LastError = LastError ?? "Native plugin not loaded"; return false; }
            try
            {
                if (preferredIndex >= 0) SetPreferredDeviceIndex(preferredIndex);
                else if (!string.IsNullOrEmpty(preferredName)) SetPreferredDevice(preferredName);
                _initialised = InitDirectInput(hwnd) != 0;
                _failed = !_initialised;
                if (_failed) LastError = "InitDirectInput failed - no usable force-feedback device (0x" + GetLastHResult().ToString("X8") + ")";
                else StartEffect();
                return _initialised;
            }
            catch (Exception ex) { _failed = true; LastError = ex.Message; return false; }
        }

        /// <summary>Closes and reopens with a different preference, without restarting the game.</summary>
        public static bool Reinitialise(string preferredName, int preferredIndex, int hwnd = 0)
        {
            Shutdown();
            return Initialise(preferredName, preferredIndex, hwnd);
        }

        /// <summary>Signed force, -<see cref="ForceMax"/>..+<see cref="ForceMax"/>. Cheap; call every physics step.</summary>
        public static bool SetForce(int force)
        {
            if (!Ready) return false;
            try { return SetDeviceForcesXY(force, 0) != 0; } catch { _failed = true; return false; }
        }

        public static void Stop()            { if (Ready) try { StopEffect(); } catch { } }
        public static void Start()           { if (Ready) try { StartEffect(); } catch { } }
        public static void Zero()            { if (Ready) try { ZeroForces(); } catch { } }
        public static void Panic()           { if (_initialised) try { PanicStop(); } catch { } }
        public static bool ExitGuards()      { if (!_initialised) return false; try { return InstallExitGuards() != 0; } catch { return false; } }
        public static void HoldTimeout(int ms) { if (_loaded) try { SetHoldTimeoutMs(ms); } catch { } }

        /// <summary>
        /// Pins device selection to a DirectInput instance GUID, which beats both
        /// index and name. Use it when your own input layer has already chosen a
        /// device: two Fanatec devices can carry the same name, so matching by
        /// name is guesswork exactly where it hurts. Null clears it. Toolkit 0.2.0+.
        /// </summary>
        public static void PreferGuid(Guid? instanceGuid)
        {
            if (!_loaded) return;
            try { SetPreferredDeviceGuid(instanceGuid?.ToByteArray()); } catch { }
        }
        public static void AutoCentre(bool on) { if (_initialised) try { SetAutoCenter(on ? 1 : 0); } catch { } }
        public static int  LastHResult       { get { try { return _loaded ? GetLastHResult() : 0; } catch { return 0; } } }
        public static int  Version           { get { try { return _loaded ? GetWheelFfbVersion() : 0; } catch { return 0; } } }

        /// <summary>
        /// Creates a hardware sine effect (road texture, tyre slip) on the held
        /// wheel, initially silent. Returns the slot for <see cref="UpdatePeriodic"/>,
        /// or -1 when the driver has no periodic support - then synthesise at
        /// 15 Hz or below through <see cref="SetForce"/> instead. Toolkit 0.2.0+.
        /// </summary>
        public static int  CreatePeriodic(int freqHz)  { if (!Ready) return -1; try { return CreatePeriodicEffect(freqHz); } catch { return -1; } }

        /// <summary>Envelope update; safe to call every frame (unchanged envelopes never reach the driver).</summary>
        public static bool UpdatePeriodic(int slot, float magnitude01, float freqHz)
        {
            if (!Ready || slot < 0) return false;
            try {
                int m = (int)Math.Round(Math.Max(0f, Math.Min(1f, magnitude01)) * 10000f);
                return UpdatePeriodicEffect(slot, m, (int)Math.Round(freqHz * 1000f)) != 0;
            } catch { return false; }
        }

        public static void ReleasePeriodics()  { if (_loaded) try { ReleasePeriodicEffects(); } catch { } }

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
            catch { return null; }
        }

        /// <summary>The same for the full controller list (shifters, pedal sets).</summary>
        public static Guid? AnyDeviceGuid(int index)
        {
            if (!_loaded) return null;
            try { var b = new byte[16]; return GetAnyDeviceGuid(index, b) != 0 ? new Guid(b) : (Guid?)null; }
            catch { return null; }
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
            try { return CreateConditionEffect((int)kind); } catch { return -1; }
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
                return UpdateConditionEffect(slot, C(coefficient, true), C(saturation, false), C(deadband, false), C(offset, true)) != 0;
            }
            catch { return false; }
        }

        public static void ReleaseConditions() { if (_loaded) try { ReleaseConditionEffects(); } catch { } }

        /// <summary>Releases the device. Safe to call repeatedly.</summary>
        public static void Shutdown()
        {
            if (!_initialised) return;
            try { FreeDirectInput(); } catch { }
            _initialised = false; _failed = false;
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

        public static bool OpenAux(int index)               { try { return _loaded && OpenAuxDevice(index) != 0; } catch { return false; } }
        public static int  ReadAux(byte[] buttons)          { try { return _loaded ? ReadAuxButtons(buttons, buttons.Length) : 0; } catch { return 0; } }
        public static void CloseAux()                       { try { if (_loaded) CloseAuxDevice(); } catch { } }
        public static int  OpenRead(int index)              { try { return _loaded ? OpenReadDevice(index) : -1; } catch { return -1; } }
        public static bool Read(int slot, int[] axes8, byte[] buttons) { try { return _loaded && ReadDeviceState(slot, axes8, buttons, buttons?.Length ?? 0) != 0; } catch { return false; } }
        public static void CloseRead()                      { try { if (_loaded) CloseReadDevices(); } catch { } }
    }
}
