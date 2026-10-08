// WheelFfb.dll - the shared native force-feedback and device layer of
// dbce-wheel-mod-toolkit. DirectInput 8 backend.
//
// Lineage: art-of-sim-rally's UnityForceFeedback.dll (2026-08-31 .. 2026-09-03),
// whose seven original exports are the ones art of rally's own dead
// ForceFeedback MonoBehaviour declares - kept verbatim so that game keeps
// working when this file is vendored under that name. Everything else was
// added by the mods that consume it. Lifecycle rules folded in from
// OutRun2006Tweaks-FFB (panic stop, exit guards, recreate-and-ramp) and
// cruisn-collection (hold watchdog). See knowledge/DIRECTINPUT-FFB.md.
//
// Contract (C ABI, x64, undecorated names; see WheelFfb.def):
//     int  InitDirectInput(int hwnd);        // opens the preferred FFB device, falls back to others
//     void Aquire(void);                     // sic - art of rally's spelling
//     int  SetDeviceForcesXY(int x, int y);  // signed magnitude, +/-10000
//     BOOL StartEffect(void);  BOOL StopEffect(void);  BOOL SetAutoCenter(BOOL);
//     void FreeDirectInput(void);
//     device selection: SetPreferredDevice, SetPreferredDeviceIndex, EnumerateDevices,
//                       GetDeviceName, GetDeviceInfo
//     any controller:   EnumerateAllDevices, GetAnyDeviceName, GetAnyDeviceInfo,
//                       OpenAuxDevice, ReadAuxButtons, CloseAuxDevice,
//                       OpenReadDevice, ReadDeviceState, CloseReadDevices
//     lifecycle:        PanicStop, ZeroForces, InstallExitGuards, SetHoldTimeoutMs
//     diagnostics:      SetLogPath, GetLastHResult, GetWheelFfbVersion
//
// Returns are 4-byte Win32 BOOL, NOT C++ bool: the default P/Invoke marshalling
// for `bool` is 4 bytes. Returning a 1-byte C++ bool leaves the upper 3 bytes
// undefined and the managed side reads garbage.
//
// Logging is ON by default to %LOCALAPPDATA%\DbceWheel\ffb.log; DBCE_FFB_LOG=0
// turns it off, DBCE_FFB_LOG_PATH=<file> or SetLogPath() redirects it. A
// consumer that wants its own folder calls SetLogPath before InitDirectInput.

#define DIRECTINPUT_VERSION 0x0800
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <dinput.h>
#include <shlobj.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cctype>
#include <cstdlib>
#include <atomic>
#include <mutex>
#include "burst_effect.h"

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "user32.lib")

// DirectInput's nominal full-scale force. The game hands us values it has
// already scaled and clamped (ForceFeedback.clampValue), but never trust it.
static const int  kForceMax = DI_FFNOMINALMAX;   // 10000
static const char kLogEnv[]     = "DBCE_FFB_LOG";
static const char kLogPathEnv[] = "DBCE_FFB_LOG_PATH";
static const int  kVersion      = 600;            // 0.6.0
static const ULONGLONG kRampMs  = 250;            // ramp-in after an effect is recreated

static std::atomic<HRESULT> g_lastHr{S_OK};
// Independent of API/effect locks. Never held across watchdog join.
static std::recursive_mutex g_logMutex;
static char      g_logPath[MAX_PATH] = {};       // resolved lazily, or set by SetLogPath
static ULONGLONG g_rampStart     = 0;
static ULONGLONG g_lastUpdateTick = 0;
static int       g_lastForce     = 0;
static std::atomic<int> g_holdMs{0};
static HANDLE    g_holdThread    = nullptr;
static std::atomic<bool> g_stopThreads{false};
// Public calls serialize across shutdown. The watchdog uses internal helpers only.
static std::recursive_mutex g_apiMutex;
static bool      g_panicked      = false;
static HWND      g_guardHwnd     = nullptr;
static WNDPROC   g_guardOrigProc = nullptr;

// Enough for any realistic rig; more FFB devices than this and the user has
// bigger problems than picking one.
static const int kMaxCandidates = 8;

struct Candidate { GUID guid; char name[MAX_PATH]; DWORD axes; DWORD buttons; };
static Candidate g_candidates[kMaxCandidates];
static int       g_candidateCount = 0;
static char      g_preferred[MAX_PATH] = {};
static int       g_preferredIndex = -1;
static GUID      g_preferredGuid = {};
static bool      g_preferredGuidSet = false;
static bool      g_strictSelection = false;

// Axis count the constant-force effect was actually created with. Every
// SetParameters call must agree with it.
static DWORD     g_effectAxes = 2;
static GUID      g_activeGuid = {};      // instance GUID of the wheel we hold
static char      g_activeName[260] = {};

// A second device we read directly - typically a sequential or H-pattern
// shifter. Kept entirely separate from the force feedback device: it is opened
// non-exclusively so it never competes with the game's own input, and it exists
// precisely because Rewired's Raw Input backend does not enumerate these at all.
static const int kMaxAuxDevices = 16;
struct AuxDevice { GUID guid; char name[MAX_PATH]; DWORD axes; DWORD buttons; int ffb; };
static DWORD g_activeAxes = 0, g_activeButtons = 0;   // caps of the wheel we hold
static AuxDevice g_aux[kMaxAuxDevices];
static int       g_auxCount = 0;
static LPDIRECTINPUTDEVICE8 g_auxDevice = nullptr;

static LPDIRECTINPUT8       g_di      = nullptr;
static LPDIRECTINPUTDEVICE8 g_device  = nullptr;
static LPDIRECTINPUTEFFECT  g_effect  = nullptr;
static HWND                 g_hwnd    = nullptr;
static bool                 g_logging = false;
static bool                 g_logChecked = false;
static CRITICAL_SECTION     g_lock;
// Published once for the module lifetime; FreeDirectInput never destroys this lock.
static INIT_ONCE g_lockOnce = INIT_ONCE_STATIC_INIT;
static std::atomic<bool> g_lockInit{false};
static BOOL CALLBACK InitializeEffectLock(PINIT_ONCE, PVOID, PVOID*)
{
    if (!InitializeCriticalSectionEx(&g_lock, 0, 0)) {
        g_lastHr.store(HRESULT_FROM_WIN32(GetLastError()));
        return FALSE;
    }
    g_lockInit.store(true, std::memory_order_release);
    return TRUE;
}
static bool EnsureEffectLock()
{
    return InitOnceExecuteOnce(&g_lockOnce, InitializeEffectLock, nullptr, nullptr) != FALSE;
}

// --------------------------------------------------------------------------
// logging
// --------------------------------------------------------------------------

// Logging is ON by default, and AOSR_FFB_LOG=0 turns it off.
//
// This is deliberately inverted from the obvious design. During phase 0 the
// question being asked is "does the game call this DLL at all", and an absent
// log file is the answer. If logging were opt-in, an absent log would be
// ambiguous between "the game never called us" and "the environment variable
// did not reach the game process" - two findings that point in opposite
// directions, and the second is easy to cause by accident (a Steam-launched
// game does not inherit a variable set in your shell). Defaulting to on
// collapses that ambiguity: no file means no call. The log is a few lines per
// session, so the cost is nil.
static bool LoggingEnabled()
{
    if (!g_logChecked) {
        char buf[8] = {};
        DWORD n = GetEnvironmentVariableA(kLogEnv, buf, sizeof(buf));
        g_logging = !(n > 0 && buf[0] == '0');
        g_logChecked = true;
    }
    return g_logging;
}

static void LogPath(char* out, size_t cch)
{
    out[0] = '\0';
    char env[MAX_PATH] = {};
    DWORD n = GetEnvironmentVariableA(kLogPathEnv, env, sizeof(env));
    if (n > 0 && n < sizeof(env)) { strncpy_s(out, cch, env, _TRUNCATE); return; }
    PWSTR wide = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &wide))) {
        char local[MAX_PATH] = {};
        WideCharToMultiByte(CP_UTF8, 0, wide, -1, local, MAX_PATH, nullptr, nullptr);
        CoTaskMemFree(wide);
        char dir[MAX_PATH] = {};
        _snprintf_s(dir, sizeof(dir), _TRUNCATE, "%s\\DbceWheel", local);
        CreateDirectoryA(dir, nullptr);
        _snprintf_s(out, cch, _TRUNCATE, "%s\\ffb.log", dir);
    }
}

static void Log(const char* fmt, ...)
{
    std::lock_guard<std::recursive_mutex> logGuard(g_logMutex);
    if (!LoggingEnabled()) return;

    if (g_logPath[0] == '\0') LogPath(g_logPath, sizeof(g_logPath));
    if (g_logPath[0] == '\0') return;

    FILE* f = nullptr;
    if (fopen_s(&f, g_logPath, "a") != 0 || !f) return;

    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(f, "%02d:%02d:%02d.%03d  ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);

    fputc('\n', f);
    fclose(f);
}

// --------------------------------------------------------------------------
// device discovery
// --------------------------------------------------------------------------

// Case-insensitive substring test, for matching a user-supplied device name
// against DirectInput's product string.
static bool ContainsNoCase(const char* haystack, const char* needle)
{
    if (!haystack || !needle || !*needle) return false;
    size_t hn = strlen(haystack), nn = strlen(needle);
    if (nn > hn) return false;
    for (size_t i = 0; i + nn <= hn; ++i) {
        size_t j = 0;
        while (j < nn && tolower((unsigned char)haystack[i + j]) ==
                         tolower((unsigned char)needle[j])) ++j;
        if (j == nn) return true;
    }
    return false;
}

// Enumerates every force-feedback-capable controller rather than stopping at the
// first. Two reasons: a rig can easily have more than one FFB device (a wheel
// plus an FFB joystick, say) and silently grabbing whichever DirectInput happened
// to list first is a coin flip; and logging all of them turns "force feedback
// didn't work" into a diagnosable report of what was actually present.
//
// Devices without FFB are skipped outright - acquiring one exclusively would take
// input away from Rewired for no benefit.
// One DirectInput instance for the life of the process. Releasing it after a
// "temporary" enumeration left the device table populated and g_di null, and
// the next OpenAuxDevice dereferenced it - a hard crash for anyone whose force
// feedback had failed to initialise (Fanatec support bundle, 2026-09-03).
// DirectInput8Create is resolved from the SYSTEM dinput8.dll by full path, not
// through this DLL's import table. In a game modded with a dinput8.dll proxy
// (OutRun 2006, the Milestone titles) an import named "dinput8.dll" binds to
// the proxy sitting beside the game exe, and every device this module opened
// would then pass through the proxy's own hooks - which is precisely the
// layer those mods use us to replace. The proxies load the real DLL this same
// way. A 32-bit build gets SysWOW64 through the redirector automatically.
typedef HRESULT (WINAPI *DirectInput8Create_t)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
static HRESULT CreateDirectInputFromSystem(LPDIRECTINPUT8* out)
{
    static DirectInput8Create_t create = nullptr;
    if (!create) {
        wchar_t path[MAX_PATH];
        UINT n = GetSystemDirectoryW(path, MAX_PATH);
        HMODULE m = nullptr;
        if (n && n < MAX_PATH - 13) { wcscat_s(path, MAX_PATH, L"\\dinput8.dll"); m = LoadLibraryW(path); }
        if (!m) { Log("system dinput8.dll not loadable (%lu); falling back to the search path", GetLastError()); m = LoadLibraryW(L"dinput8.dll"); }
        if (m) create = (DirectInput8Create_t)GetProcAddress(m, "DirectInput8Create");
        if (!create) return E_FAIL;
    }
    return create(GetModuleHandle(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8, (LPVOID*)out, nullptr);
}

static bool EnsureDirectInput()
{
    if (!EnsureEffectLock()) return false;
    if (g_di) return true;
    HRESULT hr = CreateDirectInputFromSystem(&g_di);
    if (FAILED(hr)) { Log("DirectInput8Create failed 0x%08lX", hr); g_di = nullptr; return false; }
    return true;
}

static BOOL CALLBACK EnumDeviceCallback(const DIDEVICEINSTANCE* inst, VOID* ctx)
{
    (void)ctx;
    if (g_candidateCount >= kMaxCandidates) return DIENUM_STOP;

    // The wheel we already hold exclusively is not re-opened to be inspected:
    // a second device object on a live FFB device is the one interaction in
    // the log immediately before every update started failing on a MOZA
    // R12. Its answer is known - it is an FFB device - so it is listed
    // without touching it.
    if (g_device && IsEqualGUID(inst->guidInstance, g_activeGuid)) {
        Candidate& held = g_candidates[g_candidateCount++];
        held.guid = inst->guidInstance;
        strncpy_s(held.name, sizeof(held.name), inst->tszProductName, _TRUNCATE);
        held.axes = g_activeAxes; held.buttons = g_activeButtons;
        Log("  found FFB device [%d]: %s (in use)", g_candidateCount - 1, held.name);
        return DIENUM_CONTINUE;
    }

    LPDIRECTINPUTDEVICE8 candidate = nullptr;
    if (FAILED(g_di->CreateDevice(inst->guidInstance, &candidate, nullptr)))
        return DIENUM_CONTINUE;

    DIDEVCAPS caps = {};
    caps.dwSize = sizeof(caps);
    if (FAILED(candidate->GetCapabilities(&caps)) || !(caps.dwFlags & DIDC_FORCEFEEDBACK)) {
        Log("  skip (no force feedback): %s", inst->tszProductName);
        candidate->Release();
        return DIENUM_CONTINUE;
    }

    Candidate& c = g_candidates[g_candidateCount++];
    c.guid = inst->guidInstance;
    strncpy_s(c.name, sizeof(c.name), inst->tszProductName, _TRUNCATE);
    c.axes = caps.dwAxes; c.buttons = caps.dwButtons;
    Log("  found FFB device [%d]: %s (%u axes, %u buttons)",
        g_candidateCount - 1, c.name, caps.dwAxes, caps.dwButtons);

    candidate->Release();
    return DIENUM_CONTINUE;
}

// Collects every attached game controller, force feedback or not. The FFB
// enumeration deliberately skips devices without actuators; this one must not,
// because a shifter is exactly such a device.
static BOOL CALLBACK EnumAuxCallback(const DIDEVICEINSTANCE* inst, VOID* ctx)
{
    (void)ctx;
    if (g_auxCount >= kMaxAuxDevices) return DIENUM_STOP;

    AuxDevice& d = g_aux[g_auxCount++];
    d.guid = inst->guidInstance;
    strncpy_s(d.name, sizeof(d.name), inst->tszProductName, _TRUNCATE);
    d.axes = 0; d.buttons = 0; d.ffb = 0;
    if (g_device && IsEqualGUID(inst->guidInstance, g_activeGuid)) {
        // Never re-open the wheel we hold; its answer is already known.
        d.axes = g_activeAxes; d.buttons = g_activeButtons; d.ffb = 1;
    } else if (g_di) {
        LPDIRECTINPUTDEVICE8 probe = nullptr;
        if (SUCCEEDED(g_di->CreateDevice(inst->guidInstance, &probe, nullptr)) && probe) {
            DIDEVCAPS caps = {}; caps.dwSize = sizeof(caps);
            if (SUCCEEDED(probe->GetCapabilities(&caps))) {
                d.axes = caps.dwAxes; d.buttons = caps.dwButtons; d.ffb = (caps.dwFlags & DIDC_FORCEFEEDBACK) ? 1 : 0;
            }
            probe->Release();
        }
    }
    Log("  device [%d]: %s (%lu axes, %lu buttons%s)", g_auxCount - 1, d.name, d.axes, d.buttons, d.ffb ? ", force feedback" : "");
    return DIENUM_CONTINUE;
}

// The game truncates GetForegroundWindow() into an int. HWND values fit in 32
// bits on Windows in practice, but sign-extend rather than zero-extend so a
// high-bit handle survives, and fall back to discovering the window ourselves.
static HWND ResolveHwnd(int hwnd)
{
    HWND h = (HWND)(INT_PTR)hwnd;
    if (h && IsWindow(h)) return h;

    h = GetForegroundWindow();
    if (h && IsWindow(h)) {
        Log("  hwnd %d invalid; using foreground window %p", hwnd, (void*)h);
        return h;
    }
    Log("  hwnd %d invalid and no foreground window", hwnd);
    return nullptr;
}

static void ReleaseEffect()
{
    if (g_effect) {
        g_effect->Stop();
        g_effect->Release();
        g_effect = nullptr;
    }
}

// --------------------------------------------------------------------------
// exports
// --------------------------------------------------------------------------

extern "C" {

// Forward-declared: InitDirectInput unwinds through it on every failure path.
__declspec(dllexport) void FreeDirectInput(void);
static void ReleasePeriodics();   // defined with the periodic exports below
static void ReleaseConditions();  // and the condition exports after them

// Returns 1 on success, 0 on failure. The game ignores the value, but a
// non-zero result is the honest signal and makes the log readable.

// The window the exclusive cooperative level is bound to must belong to this
// process. The caller passes GetForegroundWindow(), which at init time can be
// something else entirely (an overlay, a console). Find our own top-level
// window instead: visible, unowned, largest.
struct OwnWindowSearch { HWND best; long area; };
static BOOL CALLBACK OwnWindowCb(HWND w, LPARAM lp)
{
    DWORD pid = 0; GetWindowThreadProcessId(w, &pid);
    if (pid != GetCurrentProcessId() || !IsWindowVisible(w) || GetWindow(w, GW_OWNER)) return TRUE;
    RECT r; if (!GetWindowRect(w, &r)) return TRUE;
    long area = (long)(r.right - r.left) * (long)(r.bottom - r.top);
    OwnWindowSearch* s = (OwnWindowSearch*)lp;
    if (area > s->area) { s->area = area; s->best = w; }
    return TRUE;
}
static HWND ResolveGameWindow(HWND passed)
{
    DWORD pid = 0;
    if (passed && IsWindow(passed)) { GetWindowThreadProcessId(passed, &pid); if (pid == GetCurrentProcessId()) return passed; }
    OwnWindowSearch s = { nullptr, 0 };
    EnumWindows(OwnWindowCb, (LPARAM)&s);
    return s.best ? s.best : passed;
}

// Creates the constant-force effect on g_device. Separate from init so a
// dead effect can be rebuilt mid-session - see SetDeviceForcesXY.
static HRESULT CreateForceEffect()
{
    HRESULT hr;
    // A single constant force on X (+Y where the device has it). This mirrors
    // what the game asks for: SetDeviceForcesXY is the only force call it makes.
    DWORD axes[2]      = { DIJOFS_X, DIJOFS_Y };
    LONG  direction[2] = { 0, 0 };   // zero at rest; updates keep |direction| == |magnitude|

    DICONSTANTFORCE constant = {};
    constant.lMagnitude = 0;

    DIEFFECT effect      = {};
    effect.dwSize        = sizeof(DIEFFECT);
    effect.dwFlags       = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
    effect.dwDuration    = INFINITE;
    effect.dwGain        = DI_FFNOMINALMAX;
    effect.dwTriggerButton = DIEB_NOTRIGGER;
    effect.cAxes         = 2;
    effect.rgdwAxes      = axes;
    effect.rglDirection  = direction;
    effect.cbTypeSpecificParams = sizeof(DICONSTANTFORCE);
    effect.lpvTypeSpecificParams = &constant;

    g_effectAxes = 2;
    hr = g_device->CreateEffect(GUID_ConstantForce, &effect, &g_effect, nullptr);
    if (FAILED(hr)) {
        // Plenty of wheels expose a single FFB axis. Retry with X alone, and
        // remember that we did - SetParameters must describe the same number of
        // axes the effect was created with, or every update is rejected with
        // E_INVALIDARG and the wheel goes silently dead.
        Log("  2-axis CreateEffect failed 0x%08lX, retrying single axis", hr);
        effect.cAxes = 1;
        hr = g_device->CreateEffect(GUID_ConstantForce, &effect, &g_effect, nullptr);
        if (SUCCEEDED(hr)) g_effectAxes = 1;
    }
    return hr;
}

// Releases a device that turned out not to do force feedback, keeping the
// DirectInput instance for the next attempt (and for the shifter and reader).
static void CloseCandidate()
{
    ReleasePeriodics();
    ReleaseConditions();
    ReleaseEffect();
    if (g_device) { g_device->Unacquire(); g_device->Release(); g_device = nullptr; }
    g_activeGuid = GUID{}; g_activeName[0] = 0; g_activeAxes = g_activeButtons = 0;
}

// Stop/release the mismatched actuator under the watchdog's effect lock. Keep
// its worker, exit guards and shared DirectInput/readers for a later explicit
// initialization. Ported from the master candidate 5c3503f.
static void ReleaseRetainedDevice()
{
    if (g_lockInit) EnterCriticalSection(&g_lock);
    g_lastForce = 0;
    CloseCandidate();
    if (g_lockInit) LeaveCriticalSection(&g_lock);
}

// Opens one FFB candidate and creates the constant-force effect on it.
// Returns 1 on success; on failure everything but the instance is released.
static int OpenCandidate(int idx)
{
    HRESULT hr = S_OK;

    if (FAILED(g_di->CreateDevice(g_candidates[idx].guid, &g_device, nullptr)) || !g_device) {
        Log("  could not open the device");
        return 0;
    }
    g_activeGuid = g_candidates[idx].guid;
    g_activeAxes = g_candidates[idx].axes; g_activeButtons = g_candidates[idx].buttons;
    strncpy_s(g_activeName, sizeof(g_activeName), g_candidates[idx].name, _TRUNCATE);

    if (FAILED(hr = g_device->SetDataFormat(&c_dfDIJoystick2))) {
        Log("  SetDataFormat failed 0x%08lX", hr);
        CloseCandidate();
        return 0;
    }

    // Force feedback requires exclusive access. Background keeps effects alive
    // when the game loses focus, which matters on a multi-monitor sim rig.
    if (FAILED(hr = g_device->SetCooperativeLevel(g_hwnd, DISCL_EXCLUSIVE | DISCL_BACKGROUND))) {
        Log("  SetCooperativeLevel(EXCLUSIVE|BACKGROUND) failed 0x%08lX - "
            "another process may hold the wheel", hr);
        CloseCandidate();
        return 0;
    }

    // Autocentre fights every effect we apply; off by default, the game can
    // turn it back on through SetAutoCenter.
    DIPROPDWORD prop = {};
    prop.diph.dwSize       = sizeof(DIPROPDWORD);
    prop.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    prop.diph.dwObj        = 0;
    prop.diph.dwHow        = DIPH_DEVICE;
    prop.dwData            = DIPROPAUTOCENTER_OFF;
    g_device->SetProperty(DIPROP_AUTOCENTER, &prop.diph);
    // Axes in a known range, so the direct-input reader can calibrate against
    // 0..65535 on every device. Harmless for force feedback.
    {
        DIPROPRANGE range = {};
        range.diph.dwSize       = sizeof(DIPROPRANGE);
        range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        range.diph.dwObj        = 0;
        range.diph.dwHow        = DIPH_DEVICE;
        range.lMin = 0; range.lMax = 65535;
        g_device->SetProperty(DIPROP_RANGE, &range.diph);
    }

    g_device->Acquire();

    hr = CreateForceEffect();
    if (FAILED(hr)) {
        Log("  CreateEffect(GUID_ConstantForce) failed 0x%08lX", hr);
        g_lastHr = hr;
        CloseCandidate();
        return 0;
    }
    if (g_panicked && FAILED(hr = g_device->SendForceFeedbackCommand(DISFFC_SETACTUATORSON))) {
        g_lastHr = hr; CloseCandidate(); return 0;
    }
    g_panicked = false;
    Log("  initialised OK (constant force on %lu axis/axes)", g_effectAxes);
    return 1;
}

__declspec(dllexport) int InitDirectInput(int hwnd)
{
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex);
    Log("InitDirectInput(hwnd=%d)", hwnd);
    g_hwnd = ResolveGameWindow((HWND)(INT_PTR)hwnd);
    if (g_hwnd != (HWND)(INT_PTR)hwnd) Log("  using this process's own window %p instead of %p", (void*)g_hwnd, (void*)(INT_PTR)hwnd);

    if (!EnsureEffectLock()) return 0;

    if (g_device) {
        if (g_strictSelection && (!g_preferredGuidSet || !IsEqualGUID(g_preferredGuid, g_activeGuid))) {
            Log("  strict GUID selection changed: releasing retained device and refusing initialization");
            // Reuse must obey the same identity rule as a fresh open. Stop the
            // old actuator while retaining the watchdog, exit guards and
            // independent readers. Do not silently switch devices in this call.
            ReleaseRetainedDevice();
            g_lastHr = DIERR_NOTFOUND;
            return 0;
        }
        Log("  already initialised");
        return 1;
    }

    g_hwnd = ResolveHwnd(hwnd);
    if (!g_hwnd) return 0;

    if (!EnsureDirectInput()) return 0;
    HRESULT hr = S_OK; (void)hr;

    g_candidateCount = 0;
    g_di->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumDeviceCallback, nullptr, DIEDFL_ATTACHEDONLY);

    if (g_candidateCount == 0) {
        Log("  no force-feedback device found");
        return 0;
    }

    // Honour a preferred name if one was set and matches; otherwise take the
    // first. Choosing explicitly beats whatever order DirectInput enumerated in.
    int chosen = 0;
    bool guidMatched = false;
    if (g_preferredGuidSet) {
        for (int i = 0; i < g_candidateCount; ++i) {
            if (IsEqualGUID(g_candidates[i].guid, g_preferredGuid)) { chosen = i; guidMatched = true; break; }
        }
        if (guidMatched) Log("  using [%d] by instance GUID as requested", chosen);
        else             Log("  requested instance GUID is not among the %d FFB device(s) - it may have no actuator; falling back", g_candidateCount);
    }
    if (g_strictSelection && (!g_preferredGuidSet || !guidMatched)) {
        Log("  strict GUID selection: requested device unavailable; refusing fallback");
        g_lastHr = DIERR_NOTFOUND;
        return 0;
    }
    if (guidMatched) { /* settled */ }
    else if (g_preferredIndex >= 0) {
        if (g_preferredIndex < g_candidateCount) {
            chosen = g_preferredIndex;
            Log("  using index %d as requested", chosen);
        } else {
            Log("  requested index %d but only %d FFB device(s) present; using [0]",
                g_preferredIndex, g_candidateCount);
        }
    }
    else if (g_preferred[0]) {
        bool matched = false;
        for (int i = 0; i < g_candidateCount; ++i) {
            if (ContainsNoCase(g_candidates[i].name, g_preferred)) {
                chosen = i; matched = true; break;
            }
        }
        if (!matched)
            Log("  preferred device '%s' not found among %d FFB device(s); using the first",
                g_preferred, g_candidateCount);
    }
    else if (g_candidateCount > 1) {
        Log("  %d FFB devices present and no preference set - using [0] '%s'. "
            "Set the preferred wheel in the mod settings if this is wrong.",
            g_candidateCount, g_candidates[0].name);
    }

    // Open the chosen device; if it cannot do constant force - a Fanatec base
    // presents two "FANATEC Wheel" devices and only one has the actuator - try
    // the others rather than give up. Which twin is which is not something to
    // make a user guess from a dropdown.
    for (int attempt = 0; attempt < (g_strictSelection ? 1 : g_candidateCount); ++attempt) {
        int idx = (chosen + attempt) % g_candidateCount;
        Log("  using: [%d] %s", idx, g_candidates[idx].name);
        if (OpenCandidate(idx)) return 1;
        Log("  [%d] %s cannot be used for force feedback%s", idx, g_candidates[idx].name,
            g_strictSelection ? " - strict selection refuses fallback" : " - trying the next");
    }
    Log("  no usable force-feedback device");
    return 0;
}

// Enumerates force-feedback devices without opening or acquiring anything, so
// the settings UI can show the user a list of real device names to pick from
// instead of asking for an index. Safe to call at any time.
__declspec(dllexport) int EnumerateDevices(void)
{
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex);
    if (!EnsureDirectInput()) return 0;

    g_candidateCount = 0;
    g_di->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumDeviceCallback, nullptr, DIEDFL_ATTACHEDONLY);

    return g_candidateCount;
}

// Copies the name of an enumerated device. Returns 0 if the index is out of
// range, so the caller can stop without knowing the count in advance.
__declspec(dllexport) int GetDeviceName(int index, char* buffer, int size)
{
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex);
    if (!buffer || size <= 0) return 0;
    buffer[0] = 0;
    if (index < 0 || index >= g_candidateCount) return 0;
    strncpy_s(buffer, (size_t)size, g_candidates[index].name, _TRUNCATE);
    return 1;
}

// Not part of the game's original contract - an eighth export the mod calls
// before InitDirectInput. Harmless to the game, which never calls it.
__declspec(dllexport) void SetPreferredDevice(const char* name)
{
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex);
    if (!name || !*name) { g_preferred[0] = 0; return; }
    strncpy_s(g_preferred, sizeof(g_preferred), name, _TRUNCATE);
    Log("SetPreferredDevice(\"%s\")", g_preferred);
}

// Selects by enumeration index. Necessary because product names are not unique -
// a Fanatec rig reports two devices both called "FANATEC Wheel", which no name
// filter can tell apart.
// Selects the wheel by DirectInput instance GUID - 16 raw bytes, i.e. a
// DIDEVICEINSTANCE::guidInstance passed straight through. A mod that already
// runs its own input layer has picked a device by then, and picking the same
// one here by name is guesswork exactly where it hurts: a Fanatec base shows
// two identically named devices. Takes priority over index and name; pass
// null to clear. [OutRun2006Tweaks-FFB shares its remap layer's choice]
__declspec(dllexport) void SetPreferredDeviceGuid(const void* guid16)
{
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex);
    if (!guid16) { g_preferredGuidSet = false; Log("SetPreferredDeviceGuid(null)"); return; }
    memcpy(&g_preferredGuid, guid16, sizeof(GUID));
    g_preferredGuidSet = true;
    Log("SetPreferredDeviceGuid(%08lX-%04X-%04X-...)", g_preferredGuid.Data1, g_preferredGuid.Data2, g_preferredGuid.Data3);
}

// Opt-in: a GUID is required and only its candidate may be opened. Existing
// native consumers retain their historical fallback unless they call this.
__declspec(dllexport) void SetStrictDeviceSelection(int enabled)
{
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex);
    g_strictSelection = enabled != 0;
}

__declspec(dllexport) void SetPreferredDeviceIndex(int index)
{
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex);
    g_preferredIndex = index;
    Log("SetPreferredDeviceIndex(%d)", index);
}

// ---- auxiliary (button-only) device: shifters, handbrakes -----------------

// Capabilities of an FFB candidate, for telling two same-named devices apart.
// The instance GUID of an enumerated force-feedback device, as 16 raw bytes -
// the value SetPreferredDeviceGuid wants. Without this a managed mod could
// never use that export: it has no DirectInput layer of its own to get a GUID
// from, which is exactly what a name-based match gets wrong on a Fanatec base
// showing two identically named devices. Returns 1 on success.
// [art-of-sim-rally, 2026-09-04]
__declspec(dllexport) int GetDeviceGuid(int index, void* guid16)
{
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex);
    if (!guid16 || index < 0 || index >= g_candidateCount) return 0;
    memcpy(guid16, &g_candidates[index].guid, sizeof(GUID));
    return 1;
}

// The same for the full controller list (EnumerateAllDevices), so a shifter or
// pedal set can be pinned by identity too.
__declspec(dllexport) int GetAnyDeviceGuid(int index, void* guid16)
{
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex);
    if (!guid16 || index < 0 || index >= g_auxCount) return 0;
    memcpy(guid16, &g_aux[index].guid, sizeof(GUID));
    return 1;
}

__declspec(dllexport) int GetDeviceInfo(int index, int* axes, int* buttons)
{
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex);
    if (index < 0 || index >= g_candidateCount || !axes || !buttons) return 0;
    *axes = (int)g_candidates[index].axes; *buttons = (int)g_candidates[index].buttons;
    return 1;
}

// Capabilities of any enumerated controller.
__declspec(dllexport) int GetAnyDeviceInfo(int index, int* axes, int* buttons, int* ffb)
{
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex);
    if (index < 0 || index >= g_auxCount || !axes || !buttons || !ffb) return 0;
    *axes = (int)g_aux[index].axes; *buttons = (int)g_aux[index].buttons; *ffb = g_aux[index].ffb;
    return 1;
}

__declspec(dllexport) int EnumerateAllDevices(void)
{
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex);
    if (!EnsureDirectInput()) return 0;
    Log("EnumerateAllDevices()");
    g_auxCount = 0;
    g_di->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumAuxCallback, nullptr, DIEDFL_ATTACHEDONLY);
    return g_auxCount;
}

__declspec(dllexport) int GetAnyDeviceName(int index, char* buffer, int size)
{
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex);
    if (!buffer || size <= 0) return 0;
    buffer[0] = 0;
    if (index < 0 || index >= g_auxCount) return 0;
    strncpy_s(buffer, (size_t)size, g_aux[index].name, _TRUNCATE);
    return 1;
}

// Opened NON-exclusively and in the background: we only read buttons, and taking
// it exclusively would stop anything else - including the game - from seeing it.
__declspec(dllexport) int OpenAuxDevice(int index)
{
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex);
    // Enumerate if we have not yet, rather than only when DirectInput itself is
    // missing. Force feedback initialises first and leaves g_di set, so keying off
    // that skipped enumeration entirely and every open failed as "out of range".
    if (g_auxCount == 0) { if (EnumerateAllDevices() <= 0) return 0; }

    if (index < 0 || index >= g_auxCount) {
        Log("OpenAuxDevice(%d): out of range (%d devices)", index, g_auxCount);
        return 0;
    }

    if (!EnsureDirectInput()) return 0;
    if (g_auxDevice) { g_auxDevice->Unacquire(); g_auxDevice->Release(); g_auxDevice = nullptr; }

    HRESULT hr = g_di->CreateDevice(g_aux[index].guid, &g_auxDevice, nullptr);
    if (FAILED(hr) || !g_auxDevice) { Log("OpenAuxDevice: CreateDevice failed 0x%08lX", hr); return 0; }

    if (FAILED(hr = g_auxDevice->SetDataFormat(&c_dfDIJoystick2))) {
        Log("OpenAuxDevice: SetDataFormat failed 0x%08lX", hr);
        g_auxDevice->Release(); g_auxDevice = nullptr; return 0;
    }

    HWND hwnd = g_hwnd ? g_hwnd : GetForegroundWindow();
    if (FAILED(hr = g_auxDevice->SetCooperativeLevel(hwnd, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND))) {
        Log("OpenAuxDevice: SetCooperativeLevel failed 0x%08lX", hr);
        g_auxDevice->Release(); g_auxDevice = nullptr; return 0;
    }

    g_auxDevice->Acquire();
    Log("OpenAuxDevice(%d): %s opened", index, g_aux[index].name);
    return 1;
}

// Fills one byte per button, 1 = pressed. Returns how many were written.
__declspec(dllexport) int ReadAuxButtons(unsigned char* buffer, int length)
{
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex);
    if (!g_auxDevice || !buffer || length <= 0) return 0;

    DIJOYSTATE2 state = {};
    HRESULT hr = g_auxDevice->Poll();
    if (FAILED(hr)) { g_auxDevice->Acquire(); g_auxDevice->Poll(); }

    hr = g_auxDevice->GetDeviceState(sizeof(state), &state);
    if (FAILED(hr)) {
        if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) g_auxDevice->Acquire();
        return 0;
    }

    int n = length < 128 ? length : 128;
    for (int i = 0; i < n; ++i) buffer[i] = (state.rgbButtons[i] & 0x80) ? 1 : 0;
    return n;
}

__declspec(dllexport) void CloseAuxDevice(void)
{
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex);
    if (!g_auxDevice) return;
    g_auxDevice->Unacquire();
    g_auxDevice->Release();
    g_auxDevice = nullptr;
    Log("CloseAuxDevice()");
}

// --- direct reading of any controller's axes and buttons ----------------------
// Used for wheels the game's input library cannot read (Fanatec bases under
// Rewired's Raw Input). Every controller is opened non-exclusively for reading,
// except the force-feedback wheel, which is read through the exclusive handle we
// already hold - a second instance of the same device is not needed.
static const int kMaxReadSlots = 8;
struct ReadSlot { LPDIRECTINPUTDEVICE8 dev; bool isWheel; GUID guid; char name[MAX_PATH]; };
static ReadSlot g_read[kMaxReadSlots];
static int      g_readCount = 0;

static void SetFullRange(LPDIRECTINPUTDEVICE8 dev)
{
    DIPROPRANGE range = {};
    range.diph.dwSize       = sizeof(DIPROPRANGE);
    range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    range.diph.dwObj        = 0;
    range.diph.dwHow        = DIPH_DEVICE;
    range.lMin = 0; range.lMax = 65535;
    HRESULT hr = dev->SetProperty(DIPROP_RANGE, &range.diph);
    if (FAILED(hr)) Log("  DIPROP_RANGE 0..65535 refused 0x%08lX (device keeps its native range)", hr);
}

// Returns the slot to read from, or -1.
__declspec(dllexport) int OpenReadDevice(int index)
{
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex);
    if (g_auxCount == 0) { if (EnumerateAllDevices() <= 0) return -1; }
    if (index < 0 || index >= g_auxCount) { Log("OpenReadDevice(%d): out of range (%d devices)", index, g_auxCount); return -1; }
    if (g_readCount >= kMaxReadSlots) { Log("OpenReadDevice(%d): no free slot", index); return -1; }
    ReadSlot& s = g_read[g_readCount];
    s.dev = nullptr; s.isWheel = false; s.guid = g_aux[index].guid;
    strncpy_s(s.name, sizeof(s.name), g_aux[index].name, _TRUNCATE);
    if (g_device && IsEqualGUID(g_aux[index].guid, g_activeGuid)) {
        s.isWheel = true;
        Log("OpenReadDevice(%d): %s is the force feedback wheel - read through that handle (slot %d)", index, s.name, g_readCount);
        return g_readCount++;
    }
    if (!EnsureDirectInput()) return -1;
    HRESULT hr = g_di->CreateDevice(g_aux[index].guid, &s.dev, nullptr);
    if (FAILED(hr) || !s.dev) { Log("OpenReadDevice(%d): CreateDevice failed 0x%08lX", index, hr); s.dev = nullptr; return -1; }
    if (FAILED(hr = s.dev->SetDataFormat(&c_dfDIJoystick2))) {
        Log("OpenReadDevice(%d): SetDataFormat failed 0x%08lX", index, hr);
        s.dev->Release(); s.dev = nullptr; return -1;
    }
    HWND hwnd = g_hwnd ? g_hwnd : GetForegroundWindow();
    if (FAILED(hr = s.dev->SetCooperativeLevel(hwnd, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND))) {
        Log("OpenReadDevice(%d): SetCooperativeLevel failed 0x%08lX", index, hr);
        s.dev->Release(); s.dev = nullptr; return -1;
    }
    SetFullRange(s.dev);
    s.dev->Acquire();
    Log("OpenReadDevice(%d): %s opened for reading (slot %d)", index, s.name, g_readCount);
    return g_readCount++;
}

// axes: 8 ints (X Y Z Rx Ry Rz Slider1 Slider2); buttons: one byte each, 1 = pressed.
// Returns 1 when a state was read.
__declspec(dllexport) int ReadDeviceState(int slot, int* axes, unsigned char* buttons, int buttonCount)
{
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex);
    if (slot < 0 || slot >= g_readCount || !axes) return 0;
    ReadSlot& s = g_read[slot];
    // A read slot belongs to the original controller, never whichever wheel
    // later replaces the FFB handle. Same-GUID reacquisition may resume it.
    if (s.isWheel && (!g_device || !IsEqualGUID(s.guid, g_activeGuid))) return 0;
    LPDIRECTINPUTDEVICE8 dev = s.isWheel ? g_device : s.dev;
    if (!dev) return 0;
    DIJOYSTATE2 st = {};
    HRESULT hr = dev->Poll();
    if (FAILED(hr) && !s.isWheel) { dev->Acquire(); dev->Poll(); }
    hr = dev->GetDeviceState(sizeof(st), &st);
    if (FAILED(hr)) {
        // The wheel's acquisition is the force-feedback path's business.
        if (!s.isWheel && (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED)) dev->Acquire();
        return 0;
    }
    axes[0] = st.lX;  axes[1] = st.lY;  axes[2] = st.lZ;
    axes[3] = st.lRx; axes[4] = st.lRy; axes[5] = st.lRz;
    axes[6] = st.rglSlider[0]; axes[7] = st.rglSlider[1];
    if (buttons && buttonCount > 0) {
        int n = buttonCount < 128 ? buttonCount : 128;
        for (int i = 0; i < n; ++i) buttons[i] = (st.rgbButtons[i] & 0x80) ? 1 : 0;
    }
    return 1;
}

__declspec(dllexport) void CloseReadDevices(void)
{
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex);
    for (int i = 0; i < g_readCount; ++i) {
        if (g_read[i].dev) { g_read[i].dev->Unacquire(); g_read[i].dev->Release(); g_read[i].dev = nullptr; }
    }
    if (g_readCount) Log("CloseReadDevices(): %d slot(s) released", g_readCount);
    g_readCount = 0;
}

__declspec(dllexport) void Aquire(void)
{
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex);
    if (!g_device) { Log("Aquire() with no device"); return; }
    HRESULT hr = g_device->Acquire();
    // S_FALSE simply means "already acquired" - not worth logging every frame.
    if (hr != S_FALSE) Log("Aquire() -> 0x%08lX", hr);
}

// Called every frame while driving. Deliberately quiet unless the magnitude
// actually changes, so the trace log stays readable instead of 60 lines/second.
static int SetDeviceForcesXYInternal(int x, int y)
{
    if (!g_effect) return 0;

    if (x >  kForceMax) x =  kForceMax;
    if (x < -kForceMax) x = -kForceMax;
    if (y >  kForceMax) y =  kForceMax;
    if (y < -kForceMax) y = -kForceMax;

    static int lastX = INT_MIN, lastY = INT_MIN;
    if (x != lastX || y != lastY) {
        Log("SetDeviceForcesXY(%d, %d)", x, y);
        lastX = x; lastY = y;
    }

    EnterCriticalSection(&g_lock);

    // Direction {|x|, |y|} - always the +X half-plane - and a SIGNED magnitude.
    // Three wheels, three readings of the same call, and this is the one
    // encoding all of them accept:
    //   - MOZA R5 honours the direction AND the magnitude sign, so signing
    //     both (the original code) double-negated - one side inverted;
    //   - MOZA R12 ignores the direction and reads the magnitude sign, so an
    //     unsigned magnitude left it with no sign (anti-centring);
    //   - single-axis wheels (Fanatec) ignore direction, signed magnitude.
    // The direction's LENGTH is kept equal to |magnitude|, not a constant
    // {1,0}: every build that used a constant unit direction saw the R12 start
    // rejecting updates with DIERR_INCOMPLETEEFFECT roughly 45 s after the
    // effect was created, and no build that kept the lengths equal ever did.
    // Direction is re-sent every update; some drivers drop it otherwise.
    // Bookkeeping for the hold watchdog and the recreate ramp.
    g_lastForce = x;
    g_lastUpdateTick = GetTickCount64();
    if (g_rampStart) {
        // After an effect is recreated the wheel is at zero; stepping straight
        // back to the requested magnitude is the felt jerk, and a slew limiter
        // upstream cannot help because its state never saw the zero. Ramp in.
        ULONGLONG el = GetTickCount64() - g_rampStart;
        if (el >= kRampMs) g_rampStart = 0;
        else { x = (int)((double)x * (double)el / (double)kRampMs); y = (int)((double)y * (double)el / (double)kRampMs); }
    }

    LONG direction[2] = { x < 0 ? -(LONG)x : (LONG)x, y < 0 ? -(LONG)y : (LONG)y };
    DICONSTANTFORCE constant = {};
    constant.lMagnitude = (LONG)x;

    DIEFFECT update      = {};
    update.dwSize        = sizeof(DIEFFECT);
    update.dwFlags       = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
    // MUST match the axis count the effect was created with. Hard-coding 2 here
    // while the effect fell back to 1 axis makes every update fail with
    // E_INVALIDARG, and the wheel simply never moves.
    update.cAxes         = g_effectAxes;
    update.rglDirection  = direction;
    update.cbTypeSpecificParams  = sizeof(DICONSTANTFORCE);
    update.lpvTypeSpecificParams = &constant;

    HRESULT hr = g_effect->SetParameters(
        &update, DIEP_DIRECTION | DIEP_TYPESPECIFICPARAMS | DIEP_START);

    // DIERR_NOTEXCLUSIVEACQUIRED (0x80040205): the wheel is acquired, but not
    // exclusively, and force feedback needs exclusive access. It happens
    // when the game loses the foreground (alt-tab) and the device comes back
    // non-exclusive; nothing then re-acquires it, so every update from that
    // moment is refused - 4,401 in one session on a MOZA R12, no force at
    // all. (Misread as DIERR_INCOMPLETEEFFECT and DIERR_EFFECTPLAYING for a
    // day; the SDK header says 0x80040205 is NOTEXCLUSIVEACQUIRED.)
    // Fix: drop the device and acquire it again, which is exclusive once the
    // game is in front, then retry the update. Rate-limited.
    if (hr == DIERR_NOTEXCLUSIVEACQUIRED || hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
        static DWORD lastReacquire = 0;
        static long  reacquires = 0;
        DWORD now = GetTickCount();
        if (lastReacquire == 0 || now - lastReacquire > 250) {
            lastReacquire = now;
            HRESULT lost = hr;
            g_device->Unacquire();
            HRESULT ah = g_device->Acquire();
            if (SUCCEEDED(ah))
                hr = g_effect->SetParameters(
                    &update, DIEP_DIRECTION | DIEP_TYPESPECIFICPARAMS | DIEP_START);
            if (++reacquires <= 20 || reacquires % 100 == 0)
                Log("access lost (0x%08lX); re-acquire -> 0x%08lX, update -> 0x%08lX (#%ld)",
                    lost, ah, hr, reacquires);
        }
    }

    // Self-heal for anything else. A MOZA R12 started answering every update with
    // DIERR_INCOMPLETEEFFECT a few minutes into a session and never recovered
    // - 3,230 rejected updates in a minute, a dead wheel. The effect object
    // is unrecoverable at that point; a fresh one accepts parameters again.
    // Rate-limited so a genuinely broken device cannot spin this.
    if (FAILED(hr) && hr != DIERR_INPUTLOST && hr != DIERR_NOTACQUIRED && hr != DIERR_NOTEXCLUSIVEACQUIRED) {
        static DWORD lastHeal = 0;
        static int   healCount = 0;
        DWORD status = 0;
        if (SUCCEEDED(g_effect->GetEffectStatus(&status)))
            Log("  effect status 0x%08lX before heal", status);
        DWORD now = GetTickCount();
        if (lastHeal == 0 || now - lastHeal > 1000) {
            lastHeal = now;
            Log("SetParameters failed 0x%08lX - recreating the effect (heal #%d)", hr, ++healCount);
            g_device->Acquire();
            if (g_effect) { g_effect->Stop(); g_effect->Release(); g_effect = nullptr; }
            HRESULT ch = CreateForceEffect();
            if (SUCCEEDED(ch) && g_effect) {
                g_rampStart = GetTickCount64();
                hr = g_effect->SetParameters(
                    &update, DIEP_DIRECTION | DIEP_TYPESPECIFICPARAMS | DIEP_START);
                Log("  recreated effect: update now 0x%08lX (ramping in over %llu ms)", hr, kRampMs);
            } else {
                Log("  recreate failed 0x%08lX", ch);
            }
        }
    }

    if (FAILED(hr)) g_lastHr = hr;
    LeaveCriticalSection(&g_lock);

    if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
        g_device->Acquire();
        return 0;
    }

    // Log failures, rate-limited. Without this a wheel that accepts the effect
    // and then rejects every update looks identical in the log to one that is
    // working perfectly - which is exactly how a real bug went unnoticed until a
    // user with different hardware reported it.
    if (FAILED(hr)) {
        static DWORD lastReport = 0;
        static long  failures   = 0;
        ++failures;
        DWORD now = GetTickCount();
        if (lastReport == 0 || now - lastReport > 5000) {
            lastReport = now;
            Log("SetParameters FAILED 0x%08lX (%ld failures so far; effect has %lu axis/axes) "
                "- no force is reaching the wheel", hr, failures, g_effectAxes);
        }
        return 0;
    }
    return 1;
}

// --- periodic (sine) effects -------------------------------------------------
// Road texture and tyre slip rendered by the wheelbase itself. Synthesising
// 25-40 Hz content through 60 Hz constant-force updates loses ~26% to
// zero-order-hold roll-off, more to driver smoothing, and 30-60% to the soft
// saturation when it rides on steering load; a hardware GUID_Sine plays at
// full fidelity whatever the update rate. [OutRun2006Tweaks-FFB]
//
// Slots are independent effects on X. Create once after InitDirectInput, then
// update the envelope (magnitude, frequency) at ~15 Hz; an update is skipped
// unless the magnitude moved >5%, the period >10%, or the effect must fall
// silent. A slot whose handle dies (E_HANDLE / NOTDOWNLOADED after the device
// was re-acquired) is recreated on the next update after a 500 ms hold-off,
// without the caller noticing. Whether a driver accepts SetParameters without
// DIEP_START (no phase reset) is probed once and remembered.
static const int kMaxPeriodic = 4;
struct PeriodicSlot { LPDIRECTINPUTEFFECT fx; DWORD lastMag; DWORD lastPeriod; int hz; int durationMs; bool used; bool started; ULONGLONG holdoff; };
static PeriodicSlot g_periodic[kMaxPeriodic] = {};
static int  g_periodicStrategy = -1;   // -1 untested, 0 without DIEP_START, 1 with
static bool g_periodicProbed = false;

// Start(), retried once through a re-acquire. An effect started while the
// device is merely acquired - not exclusively - fails with
// DIERR_NOTEXCLUSIVEACQUIRED (0x80040205) and never plays. SetDeviceForcesXY
// has always recovered from that because every update carries DIEP_START; a
// periodic effect does not, since the chosen update strategy is usually
// SetParameters WITHOUT DIEP_START. Without this an effect that lost the race
// with a focus change at creation time stays silent for the whole session.
static HRESULT StartEffectRetrying(LPDIRECTINPUTEFFECT fx, const char* what)
{
    if (!fx) return E_POINTER;
    HRESULT hr = fx->Start(1, 0);
    if (hr == DIERR_NOTEXCLUSIVEACQUIRED || hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
        if (g_device) {
            g_device->Unacquire();
            HRESULT ah = g_device->Acquire();
            HRESULT first = hr;
            hr = fx->Start(1, 0);
            Log("  %s Start 0x%08lX -> re-acquire 0x%08lX -> 0x%08lX", what, first, ah, hr);
            return hr;
        }
    }
    if (FAILED(hr)) Log("  %s Start -> 0x%08lX", what, hr);
    return hr;
}

static BOOL CALLBACK EnumPeriodicCb(LPCDIEFFECTINFO info, LPVOID)
{
    Log("  driver periodic effect: %s", info->tszName);
    return DIENUM_CONTINUE;
}

static HRESULT CreatePeriodicInSlot(int i)
{
    PeriodicSlot& s = g_periodic[i];
    if (s.durationMs > 0) {
        HRESULT hr = wheel_burst::create(g_device, s.fx, s.hz, s.durationMs);
        s.started = false; s.lastMag = 0; s.holdoff = 0;
        if (FAILED(hr)) g_lastHr = hr;
        return hr;
    }
    DWORD axes[1] = { DIJOFS_X };
    LONG  dir[1]  = { 0 };
    DIPERIODIC pf = {};
    pf.dwPeriod   = (DWORD)(1000000 / (s.hz > 0 ? s.hz : 25));
    DIEFFECT eff  = {};
    eff.dwSize    = sizeof(DIEFFECT);
    eff.dwFlags   = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
    eff.dwDuration = INFINITE;
    eff.dwGain    = DI_FFNOMINALMAX;
    eff.dwTriggerButton = DIEB_NOTRIGGER;
    eff.cAxes     = 1;
    eff.rgdwAxes  = axes;
    eff.rglDirection = dir;
    eff.cbTypeSpecificParams  = sizeof(DIPERIODIC);
    eff.lpvTypeSpecificParams = &pf;
    HRESULT hr = g_device->CreateEffect(GUID_Sine, &eff, &s.fx, nullptr);
    if (FAILED(hr)) { s.fx = nullptr; g_lastHr = hr; return hr; }
    // Started silent, so envelope updates only ever need SetParameters - but
    // only once it has actually started; until then every update forces
    // DIEP_START (see UpdatePeriodicEffect).
    char what[32]; sprintf_s(what, sizeof what, "periodic[%d]", i);
    s.started = SUCCEEDED(StartEffectRetrying(s.fx, what));
    s.lastMag = 0; s.lastPeriod = pf.dwPeriod; s.holdoff = 0;
    return hr;
}

static void StopPeriodics()
{
    for (int i = 0; i < kMaxPeriodic; ++i) {
        auto& s = g_periodic[i];
        HRESULT hr = wheel_burst::stop(s.fx);
        if (FAILED(hr)) g_lastHr = hr;
        s.started = false; s.lastMag = 0;
    }
}

static void ReleasePeriodics()
{
    for (int i = 0; i < kMaxPeriodic; ++i) {
        PeriodicSlot& s = g_periodic[i];
        if (s.fx) { s.fx->Stop(); s.fx->Release(); s.fx = nullptr; }
        s.durationMs = 0; s.lastMag = 0; s.lastPeriod = 0; s.used = false; s.started = false; s.holdoff = 0;
    }
}

// Creates a sine effect on the held wheel, initially silent. Returns the slot
// (0..3) to pass to UpdatePeriodicEffect, or -1 (no device, no free slot, or
// the driver refused GUID_Sine - fall back to synthesising at <=15 Hz through
// the constant force). Call after InitDirectInput succeeded.
__declspec(dllexport) int SetDeviceForcesXY(int x, int y)
{
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex); return SetDeviceForcesXYInternal(x, y); }

__declspec(dllexport) int CreatePeriodicEffect(int freqHz)
{
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex);
    if (!g_device) { Log("CreatePeriodicEffect(%d): no device", freqHz); return -1; }
    EnterCriticalSection(&g_lock);
    if (!g_periodicProbed) {
        g_periodicProbed = true;
        g_device->EnumEffects(EnumPeriodicCb, nullptr, DIEFT_PERIODIC);
    }
    int slot = -1;
    for (int i = 0; i < kMaxPeriodic; ++i) if (!g_periodic[i].used) { slot = i; break; }
    if (slot < 0) { LeaveCriticalSection(&g_lock); Log("CreatePeriodicEffect: all %d slots in use", kMaxPeriodic); return -1; }
    PeriodicSlot& s = g_periodic[slot];
    s.used = true; s.durationMs = 0; s.hz = freqHz > 0 ? freqHz : 25;
    HRESULT hr = CreatePeriodicInSlot(slot);
    if (FAILED(hr)) { s.used = false; Log("CreatePeriodicEffect(%d): CreateEffect(GUID_Sine) failed 0x%08lX", freqHz, hr); slot = -1; }
    else Log("CreatePeriodicEffect(%d Hz) -> slot %d", freqHz, slot);
    LeaveCriticalSection(&g_lock);
    return slot;
}

// Envelope update: magnitude 0..10000, frequency in millihertz (25 Hz = 25000),
// clamped to 1..100 Hz. Cheap to call every frame - unchanged envelopes never
// reach the driver. Returns 1 if the effect carries the envelope, 0 if the
// slot is empty, the update failed, or the effect is being recreated.
__declspec(dllexport) int UpdatePeriodicEffect(int slot, int magnitude, int freqMilliHz)
{
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex);
    if (slot < 0 || slot >= kMaxPeriodic || !g_periodic[slot].used || g_periodic[slot].durationMs != 0 || !g_device || g_panicked) return 0;
    PeriodicSlot& s = g_periodic[slot];
    if (magnitude < 0) magnitude = 0;
    if (magnitude > kForceMax) magnitude = kForceMax;
    if (freqMilliHz < 1000) freqMilliHz = 1000;
    if (freqMilliHz > 100000) freqMilliHz = 100000;
    DWORD mag    = (DWORD)magnitude;
    DWORD period = (DWORD)(1000000000LL / freqMilliHz);   // microseconds

    EnterCriticalSection(&g_lock);
    if (!s.fx) {
        if (GetTickCount64() < s.holdoff) { LeaveCriticalSection(&g_lock); return 0; }
        HRESULT ch = CreatePeriodicInSlot(slot);
        if (FAILED(ch)) { s.holdoff = GetTickCount64() + 500; LeaveCriticalSection(&g_lock); return 0; }
        Log("  periodic[%d] recreated", slot);
    }
    g_lastUpdateTick = GetTickCount64();
    if (!wheel_burst::needs_update(s.lastMag, s.lastPeriod, s.started, mag, period)) {
        LeaveCriticalSection(&g_lock); return 1;
    }

    DIPERIODIC pf = {}; pf.dwMagnitude = mag; pf.dwPeriod = period;
    DIEFFECT eff = {};
    eff.dwSize = sizeof(DIEFFECT);
    eff.cbTypeSpecificParams  = sizeof(DIPERIODIC);
    eff.lpvTypeSpecificParams = &pf;

    // A slot whose Start() never succeeded must carry DIEP_START itself,
    // whatever the driver's preferred strategy is, or it plays nothing.
    DWORD flags = DIEP_TYPESPECIFICPARAMS | ((g_periodicStrategy == 1 || !s.started) ? DIEP_START : 0);
    HRESULT hr = s.fx->SetParameters(&eff, flags);
    if (g_periodicStrategy == -1 && s.started) {
        if (SUCCEEDED(hr)) { g_periodicStrategy = 0; Log("  periodic update strategy: SetParameters without DIEP_START"); }
        else {
            hr = s.fx->SetParameters(&eff, DIEP_TYPESPECIFICPARAMS | DIEP_START);
            if (SUCCEEDED(hr)) { g_periodicStrategy = 1; Log("  periodic update strategy: SetParameters with DIEP_START"); }
        }
    }
    if (hr == E_HANDLE || hr == DIERR_NOTDOWNLOADED) {
        Log("  periodic[%d] handle lost (0x%08lX) - recreating after 500 ms", slot, hr);
        s.fx->Release(); s.fx = nullptr; s.lastMag = 0; s.lastPeriod = 0; s.holdoff = GetTickCount64() + 500;
        g_lastHr = hr;
        LeaveCriticalSection(&g_lock);
        return 0;
    }
    if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED || hr == DIERR_NOTEXCLUSIVEACQUIRED) {
        g_device->Unacquire(); g_device->Acquire();
        hr = s.fx->SetParameters(&eff, DIEP_TYPESPECIFICPARAMS | DIEP_START);
    }
    if (SUCCEEDED(hr)) { s.lastMag = mag; s.lastPeriod = period; if (flags & DIEP_START) s.started = true; }
    else {
        g_lastHr = hr;
        static DWORD lastReport = 0;
        DWORD now = GetTickCount();
        if (lastReport == 0 || now - lastReport > 5000) { lastReport = now; Log("  periodic[%d] SetParameters FAILED 0x%08lX", slot, hr); }
    }
    LeaveCriticalSection(&g_lock);
    return SUCCEEDED(hr) ? 1 : 0;
}

// Burst slots share the four periodic slots, but cannot be updated through the
// continuous API. Only an explicit Play can start/recreate a burst.
__declspec(dllexport) int CreatePeriodicBurst(int freqHz, int durationMs)
{
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex);
    if (!g_device || g_panicked || !wheel_burst::valid(freqHz, durationMs)) { g_lastHr = E_INVALIDARG; return -1; }
    EnterCriticalSection(&g_lock);
    int slot = -1;
    for (int i = 0; i < kMaxPeriodic; ++i) if (!g_periodic[i].used) { slot = i; break; }
    if (slot < 0) { g_lastHr = DIERR_DEVICEFULL; LeaveCriticalSection(&g_lock); return -1; }
    auto& s = g_periodic[slot]; s.used = true; s.hz = freqHz; s.durationMs = durationMs;
    HRESULT hr = CreatePeriodicInSlot(slot);
    if (FAILED(hr)) { s.used = false; s.durationMs = 0; slot = -1; }
    g_lastHr = hr;
    LeaveCriticalSection(&g_lock);
    return slot;
}

__declspec(dllexport) int PlayPeriodicBurst(int slot, int magnitude, int freqMilliHz)
{
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex);
    if (!g_device || g_panicked || slot < 0 || slot >= kMaxPeriodic || magnitude < 0 || magnitude > kForceMax || freqMilliHz < 1000 || freqMilliHz > 100000)
    { g_lastHr = E_INVALIDARG; return 0; }
    EnterCriticalSection(&g_lock);
    // Check after taking the effect lock: a queued event must not pass a
    // foreground check, wait behind focus-loss zeroing, then start in background.
    DWORD foregroundPid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &foregroundPid);
    if (magnitude > 0 && foregroundPid != GetCurrentProcessId()) { g_lastHr = DIERR_NOTACQUIRED; LeaveCriticalSection(&g_lock); return 0; }
    auto& s = g_periodic[slot];
    if (!s.used || s.durationMs == 0) { g_lastHr = E_INVALIDARG; LeaveCriticalSection(&g_lock); return 0; }
    HRESULT hr = S_OK;
    if (!s.fx && magnitude > 0) hr = CreatePeriodicInSlot(slot);
    if (SUCCEEDED(hr)) hr = magnitude == 0 ? wheel_burst::stop(s.fx) : wheel_burst::play(s.fx, s.durationMs, magnitude, freqMilliHz);
    s.started = SUCCEEDED(hr) && magnitude > 0; s.lastMag = s.started ? magnitude : 0;
    if (SUCCEEDED(hr)) { s.hz = freqMilliHz / 1000; g_lastUpdateTick = GetTickCount64(); }
    g_lastHr = hr;
    LeaveCriticalSection(&g_lock);
    return SUCCEEDED(hr) ? 1 : 0;
}

__declspec(dllexport) int StopPeriodicBurst(int slot)
{
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex);
    if (!g_lockInit || slot < 0 || slot >= kMaxPeriodic) { g_lastHr = E_INVALIDARG; return 0; }
    EnterCriticalSection(&g_lock);
    auto& s = g_periodic[slot];
    if (!s.used || s.durationMs == 0) { g_lastHr = E_INVALIDARG; LeaveCriticalSection(&g_lock); return 0; }
    HRESULT hr = wheel_burst::stop(s.fx);
    s.started = false; s.lastMag = 0; g_lastHr = hr;
    LeaveCriticalSection(&g_lock);
    return SUCCEEDED(hr) ? 1 : 0;
}

// Stops and releases every periodic slot. PanicStop and FreeDirectInput do
// this too; call it yourself only to switch effects off while the wheel stays.
__declspec(dllexport) void ReleasePeriodicEffects(void)
{
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex);
    Log("ReleasePeriodicEffects()");
    if (!EnsureEffectLock()) return;
    EnterCriticalSection(&g_lock);
    ReleasePeriodics();
    LeaveCriticalSection(&g_lock);
}

// --- condition effects (spring, damper, inertia, friction) -------------------
// A damper is not a periodic effect. DirectInput renders it as a CONDITION,
// where the force is a function of the axis position or velocity rather than a
// waveform, and the wheelbase computes it continuously instead of following
// 60 Hz updates from us. That makes a hardware damper both smoother and more
// stable than damping synthesised in the force model, which is why a
// direct-drive base oscillating is usually fixed here rather than by lowering
// gain. [requested by art-of-sim-rally, 2026-09-04]
//
// Slots work exactly like the periodic ones: create once, update cheaply, and
// a slot whose handle dies is recreated behind a hold-off.
static const int kMaxCondition = 4;
struct ConditionSlot {
    LPDIRECTINPUTEFFECT fx;
    int  type;
    LONG lastCoeff, lastSat, lastDead, lastOffset;
    bool used, started;
    ULONGLONG holdoff;
};
static ConditionSlot g_condition[kMaxCondition] = {};

static const GUID* ConditionGuid(int type)
{
    switch (type) {
    case 0: return &GUID_Spring;
    case 1: return &GUID_Damper;
    case 2: return &GUID_Inertia;
    case 3: return &GUID_Friction;
    default: return nullptr;
    }
}
static const char* ConditionName(int type)
{
    switch (type) {
    case 0: return "spring"; case 1: return "damper";
    case 2: return "inertia"; case 3: return "friction";
    default: return "?";
    }
}

static void FillCondition(DICONDITION* c, LONG coeff, LONG sat, LONG dead, LONG offset)
{
    c->lOffset              = offset;
    c->lPositiveCoefficient = coeff;
    c->lNegativeCoefficient = coeff;
    c->dwPositiveSaturation = (DWORD)sat;
    c->dwNegativeSaturation = (DWORD)sat;
    c->lDeadBand            = dead;
}

static HRESULT CreateConditionInSlot(int i)
{
    ConditionSlot& s = g_condition[i];
    const GUID* guid = ConditionGuid(s.type);
    if (!guid) return E_INVALIDARG;

    DWORD axes[1] = { DIJOFS_X };
    LONG  dir[1]  = { 0 };
    DICONDITION cond = {};
    FillCondition(&cond, s.lastCoeff, s.lastSat, s.lastDead, s.lastOffset);

    DIEFFECT eff  = {};
    eff.dwSize    = sizeof(DIEFFECT);
    eff.dwFlags   = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
    eff.dwDuration = INFINITE;
    eff.dwGain    = DI_FFNOMINALMAX;
    eff.dwTriggerButton = DIEB_NOTRIGGER;
    eff.cAxes     = 1;
    eff.rgdwAxes  = axes;
    eff.rglDirection = dir;
    eff.cbTypeSpecificParams  = sizeof(DICONDITION);
    eff.lpvTypeSpecificParams = &cond;

    HRESULT hr = g_device->CreateEffect(*guid, &eff, &s.fx, nullptr);
    if (FAILED(hr)) { s.fx = nullptr; g_lastHr = hr; return hr; }
    char what[32]; sprintf_s(what, sizeof what, "condition[%d]", i);
    s.started = SUCCEEDED(StartEffectRetrying(s.fx, what));
    s.holdoff = 0;
    return hr;
}

static void StopConditions()
{
    for (int i = 0; i < kMaxCondition; ++i) if (g_condition[i].fx) g_condition[i].fx->Stop();
}

static void ReleaseConditions()
{
    for (int i = 0; i < kMaxCondition; ++i) {
        ConditionSlot& s = g_condition[i];
        if (s.fx) { s.fx->Stop(); s.fx->Release(); s.fx = nullptr; }
        s.used = false; s.started = false; s.holdoff = 0;
        s.lastCoeff = s.lastSat = s.lastDead = s.lastOffset = 0;
    }
}

// Creates a condition effect on the held wheel, initially at zero strength.
// type: 0 spring, 1 damper, 2 inertia, 3 friction. Returns the slot, or -1 when
// there is no device, no free slot, or the driver has no such effect - a wheel
// without a hardware damper is normal, so treat -1 as "do it in the model".
__declspec(dllexport) int CreateConditionEffect(int type)
{
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex);
    if (!g_device) { Log("CreateConditionEffect(%s): no device", ConditionName(type)); return -1; }
    if (!ConditionGuid(type)) { Log("CreateConditionEffect: unknown type %d", type); return -1; }
    EnterCriticalSection(&g_lock);
    int slot = -1;
    for (int i = 0; i < kMaxCondition; ++i) if (!g_condition[i].used) { slot = i; break; }
    if (slot < 0) { LeaveCriticalSection(&g_lock); Log("CreateConditionEffect: all %d slots in use", kMaxCondition); return -1; }
    ConditionSlot& s = g_condition[slot];
    s.used = true; s.type = type;
    s.lastCoeff = s.lastSat = s.lastDead = s.lastOffset = 0;
    HRESULT hr = CreateConditionInSlot(slot);
    if (FAILED(hr)) { s.used = false; Log("CreateConditionEffect(%s) failed 0x%08lX", ConditionName(type), hr); slot = -1; }
    else Log("CreateConditionEffect(%s) -> slot %d", ConditionName(type), slot);
    LeaveCriticalSection(&g_lock);
    return slot;
}

// coefficient -10000..10000 (negative inverts the effect), saturation and
// deadband 0..10000, offset -10000..10000. Cheap to call every frame; an
// unchanged condition never reaches the driver. Returns 1 when the effect
// carries the values.
__declspec(dllexport) int UpdateConditionEffect(int slot, int coefficient, int saturation, int deadband, int offset)
{
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex);
    if (slot < 0 || slot >= kMaxCondition || !g_condition[slot].used || !g_device || g_panicked) return 0;
    ConditionSlot& s = g_condition[slot];
    if (coefficient >  kForceMax) coefficient =  kForceMax;
    if (coefficient < -kForceMax) coefficient = -kForceMax;
    if (saturation < 0) saturation = 0; if (saturation > kForceMax) saturation = kForceMax;
    if (deadband   < 0) deadband   = 0; if (deadband   > kForceMax) deadband   = kForceMax;
    if (offset >  kForceMax) offset =  kForceMax;
    if (offset < -kForceMax) offset = -kForceMax;

    EnterCriticalSection(&g_lock);
    if (!s.fx) {
        if (GetTickCount64() < s.holdoff) { LeaveCriticalSection(&g_lock); return 0; }
        HRESULT ch = CreateConditionInSlot(slot);
        if (FAILED(ch)) { s.holdoff = GetTickCount64() + 500; LeaveCriticalSection(&g_lock); return 0; }
        Log("  condition[%d] recreated", slot);
    }
    g_lastUpdateTick = GetTickCount64();
    bool silence = (coefficient == 0 && s.lastCoeff != 0);
    bool moved = (!s.started && coefficient != 0) || labs((long)coefficient - (long)s.lastCoeff) > 500 ||
                 labs((long)saturation  - (long)s.lastSat)   > 500 ||
                 s.lastDead != deadband || s.lastOffset != offset;
    if (!moved && !silence) { LeaveCriticalSection(&g_lock); return 1; }

    DICONDITION cond = {};
    FillCondition(&cond, coefficient, saturation, deadband, offset);
    DIEFFECT eff = {};
    eff.dwSize = sizeof(DIEFFECT);
    eff.cbTypeSpecificParams  = sizeof(DICONDITION);
    eff.lpvTypeSpecificParams = &cond;

    DWORD flags = DIEP_TYPESPECIFICPARAMS | ((g_periodicStrategy == 1 || !s.started) ? DIEP_START : 0);
    HRESULT hr = s.fx->SetParameters(&eff, flags);
    if (hr == E_HANDLE || hr == DIERR_NOTDOWNLOADED) {
        Log("  condition[%d] handle lost (0x%08lX) - recreating after 500 ms", slot, hr);
        s.fx->Release(); s.fx = nullptr; s.started = false;
        s.holdoff = GetTickCount64() + 500;
        g_lastHr = hr;
        LeaveCriticalSection(&g_lock);
        return 0;
    }
    if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED || hr == DIERR_NOTEXCLUSIVEACQUIRED) {
        g_device->Unacquire(); g_device->Acquire();
        hr = s.fx->SetParameters(&eff, DIEP_TYPESPECIFICPARAMS | DIEP_START);
    }
    if (SUCCEEDED(hr)) {
        s.lastCoeff = coefficient; s.lastSat = saturation;
        s.lastDead = deadband; s.lastOffset = offset;
        if (flags & DIEP_START) s.started = true;
    } else {
        g_lastHr = hr;
        static DWORD lastReport = 0;
        DWORD now = GetTickCount();
        if (lastReport == 0 || now - lastReport > 5000) { lastReport = now; Log("  condition[%d] SetParameters FAILED 0x%08lX", slot, hr); }
    }
    LeaveCriticalSection(&g_lock);
    return SUCCEEDED(hr) ? 1 : 0;
}

__declspec(dllexport) void ReleaseConditionEffects(void)
{
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex);
    Log("ReleaseConditionEffects()");
    if (!EnsureEffectLock()) return;
    EnterCriticalSection(&g_lock);
    ReleaseConditions();
    LeaveCriticalSection(&g_lock);
}

// --- lifecycle ---------------------------------------------------------------

// Everything off, in the order that leaves the base torque-free: zero, stop,
// STOPALL, actuators off, RESET (which destroys the effects, so release ours),
// unacquire, and only then restore autocentre - that property may only be
// written on an unacquired device (0x800700AA otherwise). Call it while the
// game window still exists: by DLL_PROCESS_DETACH the OS has force-unacquired
// the exclusive device and nothing reaches the wheel. [OutRun2006Tweaks-FFB]
__declspec(dllexport) void PanicStop(void)
{
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex);
    Log("PanicStop()");
    if (!g_device) return;
    if (!EnsureEffectLock()) return;
    EnterCriticalSection(&g_lock);
    if (g_effect) g_effect->Stop();
    StopPeriodics();
    StopConditions();
    g_device->SendForceFeedbackCommand(DISFFC_STOPALL);
    g_device->SendForceFeedbackCommand(DISFFC_SETACTUATORSOFF);
    g_device->SendForceFeedbackCommand(DISFFC_RESET);
    ReleasePeriodics();
    ReleaseConditions();
    ReleaseEffect();
    g_device->Unacquire();
    DIPROPDWORD prop = {};
    prop.diph.dwSize       = sizeof(DIPROPDWORD);
    prop.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    prop.diph.dwHow        = DIPH_DEVICE;
    prop.dwData            = DIPROPAUTOCENTER_ON;
    HRESULT hr = g_device->SetProperty(DIPROP_AUTOCENTER, &prop.diph);
    Log("  actuators off, effect released, autocentre restore -> 0x%08lX", hr);
    g_panicked = true;
    g_lastForce = 0;
    LeaveCriticalSection(&g_lock);
}

// Zero force without releasing anything; for focus loss and menus.
static void ZeroForcesInternal(void)
{
    if (!g_lockInit) return;
    EnterCriticalSection(&g_lock);
    if (g_effect) { SetDeviceForcesXYInternal(0, 0); if (g_effect) g_effect->Stop(); }
    g_lastForce = 0;
    StopPeriodics(); // Includes finite bursts; do not call an update that can restart them.
    for (int i = 0; i < kMaxCondition; ++i) {
        auto& s = g_condition[i];
        HRESULT hr = wheel_burst::stop(s.fx);
        if (FAILED(hr)) g_lastHr = hr;
        s.started = false; s.lastCoeff = s.lastSat = s.lastDead = s.lastOffset = 0;
    }
    LeaveCriticalSection(&g_lock);
}

__declspec(dllexport) void ZeroForces(void)
{
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex); ZeroForcesInternal(); }

static LRESULT CALLBACK GuardWndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CLOSE:
    case WM_QUERYENDSESSION:
    case WM_ENDSESSION:
    case WM_DESTROY:
        if (!g_panicked) { Log("exit guard: message 0x%04X", msg); PanicStop(); }
        break;
    case WM_ACTIVATEAPP:
        if (wp == FALSE) { Log("exit guard: app deactivated - silence all effects"); ZeroForces(); }
        break;
    default: break;
    }
    return g_guardOrigProc ? CallWindowProcW(g_guardOrigProc, h, msg, wp, lp) : DefWindowProcW(h, msg, wp, lp);
}

// Subclasses the game's main window so the wheel is zeroed and released
// before the window - and with it the exclusive acquisition - goes away.
// Returns 1 if installed. Must be called from the window's own thread, which
// a P/Invoke from the game's main thread is.
__declspec(dllexport) int InstallExitGuards(void)
{
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex);
    if (g_guardOrigProc) return 1;
    HWND h = g_hwnd ? g_hwnd : ResolveGameWindow(GetForegroundWindow());
    if (!h) { Log("InstallExitGuards: no window"); return 0; }
    WNDPROC prev = (WNDPROC)SetWindowLongPtrW(h, GWLP_WNDPROC, (LONG_PTR)GuardWndProc);
    if (!prev) { Log("InstallExitGuards: SetWindowLongPtr failed %lu", GetLastError()); return 0; }
    g_guardOrigProc = prev; g_guardHwnd = h;
    Log("InstallExitGuards: subclassed window %p", (void*)h);
    return 1;
}

static void RemoveExitGuards()
{
    if (g_guardOrigProc && g_guardHwnd) {
        SetWindowLongPtrW(g_guardHwnd, GWLP_WNDPROC, (LONG_PTR)g_guardOrigProc);
        g_guardOrigProc = nullptr; g_guardHwnd = nullptr;
    }
}

// Releases the force when the game stops updating it - pause, menu, crash of
// the managed side. A paused direct-drive base holding torque is a hazard.
// Measure that the game writes every frame before turning it on; all four
// Cruis'n games do, so it only fires when it should. [cruisn-collection]
// Shared with deterministic fake-effect tests; a zero constant force must not
// hide an active periodic or condition effect from the watchdog.
static bool ApplyHoldTimeout(ULONGLONG now)
{
    const int holdMs = g_holdMs.load();
    if (!g_lockInit.load(std::memory_order_acquire) || holdMs <= 0) return false;
    EnterCriticalSection(&g_lock);
    bool active = g_lastForce != 0;
    for (int i = 0; i < kMaxPeriodic; ++i) active = active || (g_periodic[i].started && g_periodic[i].lastMag != 0);
    for (int i = 0; i < kMaxCondition; ++i) active = active || (g_condition[i].started && g_condition[i].lastCoeff != 0);
    bool expired = active && now >= g_lastUpdateTick && now - g_lastUpdateTick > (ULONGLONG)holdMs;
    if (expired) ZeroForcesInternal();
    LeaveCriticalSection(&g_lock);
    return expired;
}
static DWORD WINAPI HoldWatchdog(LPVOID)
{
    while (!g_stopThreads) {
        Sleep(50);
        if (ApplyHoldTimeout(GetTickCount64())) Log("hold watchdog: silenced all effect families");
    }
    return 0;
}

__declspec(dllexport) void SetHoldTimeoutMs(int ms)
{
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex);
    g_holdMs = ms;
    Log("SetHoldTimeoutMs(%d)", ms);
    if (ms > 0 && !g_holdThread) {
        // Publish the lock before creating a worker, including pre-device startup.
        if (!EnsureEffectLock()) return;
        g_stopThreads = false;
        g_holdThread = CreateThread(nullptr, 0, HoldWatchdog, nullptr, 0, nullptr);
    }
}

// --- diagnostics ---------------------------------------------------------------

__declspec(dllexport) void SetLogPath(const char* path)
{
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex);
    std::lock_guard<std::recursive_mutex> logGuard(g_logMutex);
    if (!path || !path[0]) { g_logPath[0] = '\0'; return; }
    char prev[MAX_PATH]; strncpy_s(prev, sizeof(prev), g_logPath, _TRUNCATE);
    if (prev[0]) Log("log continues in %s", path);
    strncpy_s(g_logPath, sizeof(g_logPath), path, _TRUNCATE);
    // Create the folder so the first append succeeds.
    char dir[MAX_PATH]; strncpy_s(dir, sizeof(dir), path, _TRUNCATE);
    char* slash = strrchr(dir, '\\');
    if (slash) { *slash = 0; CreateDirectoryA(dir, nullptr); }
    Log("=== log redirected here (WheelFfb %d) ===", kVersion);
}

__declspec(dllexport) int GetLastHResult(void) {
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex); return (int)g_lastHr.load(); }
__declspec(dllexport) int GetWheelFfbVersion(void) {
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex); return kVersion; }

__declspec(dllexport) BOOL StartEffect(void)
{
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex);
    if (!g_effect) { Log("StartEffect() with no effect"); return FALSE; }
    HRESULT hr = StartEffectRetrying(g_effect, "constant force");
    Log("StartEffect() -> 0x%08lX", hr);
    if (FAILED(hr)) g_lastHr = hr;
    return SUCCEEDED(hr) ? TRUE : FALSE;
}

__declspec(dllexport) BOOL StopEffect(void)
{
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex);
    Log("StopEffect()");
    if (!g_effect) return FALSE;
    return SUCCEEDED(g_effect->Stop()) ? TRUE : FALSE;
}

__declspec(dllexport) BOOL SetAutoCenter(BOOL enable)
{
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex);
    Log("SetAutoCenter(%d)", enable);
    if (!g_device) return FALSE;

    DIPROPDWORD prop = {};
    prop.diph.dwSize       = sizeof(DIPROPDWORD);
    prop.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    prop.diph.dwObj        = 0;
    prop.diph.dwHow        = DIPH_DEVICE;
    prop.dwData            = enable ? DIPROPAUTOCENTER_ON : DIPROPAUTOCENTER_OFF;

    HRESULT hr = g_device->SetProperty(DIPROP_AUTOCENTER, &prop.diph);
    if (FAILED(hr)) Log("  SetProperty(AUTOCENTER) -> 0x%08lX", hr);
    return SUCCEEDED(hr) ? TRUE : FALSE;
}

__declspec(dllexport) void FreeDirectInput(void)
{
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex);
    Log("FreeDirectInput()");
    RemoveExitGuards();
    g_stopThreads.store(true);
    // Stop torque before waiting, without releasing any worker-visible object.
    if (g_lockInit) EnterCriticalSection(&g_lock);
    if (g_effect) g_effect->Stop();
    StopPeriodics();
    StopConditions();
    g_lastForce = 0;
    if (g_lockInit) LeaveCriticalSection(&g_lock);
    // Never join under g_lock: the watchdog may still need it to finish.
    if (g_holdThread)
    {
        if (GetThreadId(g_holdThread) == GetCurrentThreadId())
        {
            Log("FreeDirectInput: forbidden watchdog-thread shutdown; retaining resources");
            // Do not self-join or return an unsafe-to-unload success through the void ABI.
            for (;;) Sleep(INFINITE);
        }
        // Callers may unload this DLL as soon as FreeDirectInput returns.
        // Keep both the code and effects alive until the watchdog has exited.
        bool loggedWaitFailure = false;
        while (WaitForSingleObject(g_holdThread, INFINITE) != WAIT_OBJECT_0)
        {
            if (!loggedWaitFailure)
            {
                Log("FreeDirectInput: watchdog join failed; waiting before cleanup");
                loggedWaitFailure = true;
            }
            // The void ABI cannot report an unsafe-to-unload result. Fail closed.
            Sleep(50);
        }
        CloseHandle(g_holdThread);
        g_holdThread = nullptr;
    }
    ReleasePeriodics();
    ReleaseConditions();
    ReleaseEffect();
    if (g_device) { g_device->Unacquire(); g_device->Release(); g_device = nullptr; }
    g_activeGuid = GUID{}; g_activeName[0] = 0; g_activeAxes = g_activeButtons = 0;
    // The instance stays while the shifter or the direct reader still use it.
    if (g_di && !g_auxDevice && g_readCount == 0) { g_di->Release(); g_di = nullptr; }
}

} // extern "C"

BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID)
{
    // Logging can wait on another thread; never acquire its mutex under loader lock.
    // Only legal after all callers and the watchdog are quiescent; never wait here.
    if (reason == DLL_PROCESS_DETACH && g_lockInit.load(std::memory_order_acquire)) {
        DeleteCriticalSection(&g_lock);
        g_lockInit.store(false, std::memory_order_release);
    }
    return TRUE;
}
