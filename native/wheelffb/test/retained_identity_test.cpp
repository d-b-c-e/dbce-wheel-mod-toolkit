// Strict device selection with a retained force handle (HCT native review 2026-10-07): the production
// InitDirectInput against bounded COM fakes. No real DirectInput, device, DLL loading or force output.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../WheelFfb.cpp"
#include <cstring>

static int checks = 0;
static void Require(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(2); }
    ++checks;
}

struct Effect : IDirectInputEffect {
    int stops = 0, releases = 0;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override { return E_NOINTERFACE; }
    ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
    ULONG STDMETHODCALLTYPE Release() override { ++releases; return 0; }
    HRESULT STDMETHODCALLTYPE Initialize(HINSTANCE, DWORD, REFGUID) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetEffectGuid(LPGUID) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetParameters(LPDIEFFECT, DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetParameters(LPCDIEFFECT, DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE Start(DWORD, DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE Stop() override { ++stops; return S_OK; }
    HRESULT STDMETHODCALLTYPE GetEffectStatus(LPDWORD status) override { *status = 0; return S_OK; }
    HRESULT STDMETHODCALLTYPE Download() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE Unload() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE Escape(LPDIEFFESCAPE) override { return E_NOTIMPL; }
};

// One fake wheel. Every CreateDevice of its GUID hands out this same object (inspection and the force handle alike),
// so open/release counts are per wheel.
struct Wheel : IDirectInputDevice8A {
    GUID guid{}; const char* name = ""; Effect effect; HRESULT acquireResult = S_OK;
    int opens = 0, releases = 0, unacquires = 0, acquires = 0, effects = 0;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override { return E_NOINTERFACE; }
    ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
    ULONG STDMETHODCALLTYPE Release() override { ++releases; return 0; }
    HRESULT STDMETHODCALLTYPE GetCapabilities(LPDIDEVCAPS caps) override { caps->dwFlags = DIDC_FORCEFEEDBACK; caps->dwAxes = 3; caps->dwButtons = 12; return S_OK; }
    HRESULT STDMETHODCALLTYPE EnumObjects(LPDIENUMDEVICEOBJECTSCALLBACKA, LPVOID, DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetProperty(REFGUID, LPDIPROPHEADER) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetProperty(REFGUID, LPCDIPROPHEADER) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE Acquire() override { ++acquires; return acquireResult; }
    HRESULT STDMETHODCALLTYPE Unacquire() override { ++unacquires; return S_OK; }
    HRESULT STDMETHODCALLTYPE GetDeviceState(DWORD, LPVOID) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetDeviceData(DWORD, LPDIDEVICEOBJECTDATA, LPDWORD, DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetDataFormat(LPCDIDATAFORMAT) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetEventNotification(HANDLE) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetCooperativeLevel(HWND, DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetObjectInfo(LPDIDEVICEOBJECTINSTANCEA, DWORD, DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetDeviceInfo(LPDIDEVICEINSTANCEA) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE RunControlPanel(HWND, DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Initialize(HINSTANCE, DWORD, REFGUID) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateEffect(REFGUID, LPCDIEFFECT, LPDIRECTINPUTEFFECT* out, LPUNKNOWN) override { ++effects; *out = &effect; return S_OK; }
    HRESULT STDMETHODCALLTYPE EnumEffects(LPDIENUMEFFECTSCALLBACKA, LPVOID, DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetEffectInfo(LPDIEFFECTINFOA, REFGUID) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetForceFeedbackState(LPDWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SendForceFeedbackCommand(DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE EnumCreatedEffectObjects(LPDIENUMCREATEDEFFECTOBJECTSCALLBACK, LPVOID, DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Escape(LPDIEFFESCAPE) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Poll() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SendDeviceData(DWORD, LPCDIDEVICEOBJECTDATA, LPDWORD, DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE EnumEffectsInFile(LPCSTR, LPDIENUMEFFECTSINFILECALLBACK, LPVOID, DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE WriteEffectToFile(LPCSTR, DWORD, LPDIFILEEFFECT, DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE BuildActionMap(LPDIACTIONFORMATA, LPCSTR, DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetActionMap(LPDIACTIONFORMATA, LPCSTR, DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetImageInfo(LPDIDEVICEIMAGEINFOHEADERA) override { return E_NOTIMPL; }
};

struct Input : IDirectInput8A {
    Wheel* wheels[2] = {}; int count = 0; HRESULT openResult = S_OK;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override { return E_NOINTERFACE; }
    ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
    ULONG STDMETHODCALLTYPE Release() override { return 0; }
    HRESULT STDMETHODCALLTYPE CreateDevice(REFGUID guid, LPDIRECTINPUTDEVICE8A* out, LPUNKNOWN) override {
        for (int i = 0; i < count; ++i) if (IsEqualGUID(guid, wheels[i]->guid)) { *out = wheels[i]; ++wheels[i]->opens; return S_OK; }
        *out = nullptr; return DIERR_DEVICENOTREG;
    }
    HRESULT STDMETHODCALLTYPE EnumDevices(DWORD, LPDIENUMDEVICESCALLBACKA callback, LPVOID ref, DWORD) override {
        for (int i = 0; i < count; ++i) {
            DIDEVICEINSTANCEA inst = {}; inst.dwSize = sizeof(inst); inst.guidInstance = wheels[i]->guid;
            strncpy_s(inst.tszProductName, sizeof(inst.tszProductName), wheels[i]->name, _TRUNCATE);
            if (callback(&inst, ref) == DIENUM_STOP) break;
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetDeviceStatus(REFGUID) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE RunControlPanel(HWND, DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Initialize(HINSTANCE, DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE FindDevice(REFGUID, LPCSTR, LPGUID) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE EnumDevicesBySemantics(LPCSTR, LPDIACTIONFORMATA, LPDIENUMDEVICESBYSEMANTICSCBA, LPVOID, DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE ConfigureDevices(LPDICONFIGUREDEVICESCALLBACK, LPDICONFIGUREDEVICESPARAMSA, DWORD, LPVOID) override { return E_NOTIMPL; }
};

static const GUID GuidA = { 0xaaaaaaaa, 1, 1, { 1, 1, 1, 1, 1, 1, 1, 1 } };
static const GUID GuidB = { 0xbbbbbbbb, 2, 2, { 2, 2, 2, 2, 2, 2, 2, 2 } };
static HWND window = nullptr;

// Fresh library state with wheels A (and B unless absent) attached, and A retained through a normal strict init.
struct Rig {
    Input input; Wheel a, b;
    Rig(bool bPresent = true, bool strict = true) {
        a.guid = GuidA; a.name = "Wheel A"; b.guid = GuidB; b.name = "Wheel B";
        input.wheels[0] = &a; if (bPresent) input.wheels[1] = &b; input.count = bPresent ? 2 : 1;
        g_di = &input;
        SetStrictDeviceSelection(strict ? 1 : 0);
        SetPreferredDeviceGuid(&GuidA);
        Require(InitDirectInput((int)(INT_PTR)window) == 1, "baseline: A opens");
        Require(g_device == &a && IsEqualGUID(g_activeGuid, GuidA), "baseline: A is retained");
    }
    ~Rig() { FreeDirectInput(); g_di = nullptr; SetPreferredDeviceGuid(nullptr); SetStrictDeviceSelection(0); }
};

int main() {
    SetEnvironmentVariableA("DBCE_FFB_LOG", "0");
    window = CreateWindowExA(0, "STATIC", "retained-identity-test", WS_OVERLAPPEDWINDOW, 0, 0, 10, 10, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    Require(window != nullptr, "test window");

    {   // Control: the same GUID is reused without another open or release.
        Rig r; int opens = r.a.opens, releases = r.a.releases;   // enumeration already inspected (opened/released) A once
        Require(InitDirectInput((int)(INT_PTR)window) == 1, "same GUID: success");
        Require(g_device == &r.a && r.a.opens == opens && r.a.releases == releases && r.a.effect.releases == 0, "same GUID: A reused, nothing released or reopened");
    }
    {   // A retained, B requested: A is stopped and released, then B opens; the identity is B's.
        Rig r; SetHoldTimeoutMs(500); HANDLE watchdog = g_holdThread;
        SetPreferredDeviceGuid(&GuidB);
        Require(InitDirectInput((int)(INT_PTR)window) == 1, "switch to B: success");
        Require(g_device == &r.b && IsEqualGUID(g_activeGuid, GuidB) && std::strcmp(g_activeName, "Wheel B") == 0, "switch to B: B is the held wheel and identity");
        Require(r.a.effect.stops >= 1 && r.a.effect.releases == 1, "switch to B: A's constant effect stopped and released once");
        Require(r.a.unacquires >= 1 && r.a.releases >= 2, "switch to B: A unacquired and its force handle released (after the inspection release)");
        Require(r.b.effects == 1 && g_effect == &r.b.effect, "switch to B: a fresh effect on B");
        Require(g_holdThread == watchdog && watchdog != nullptr && !g_stopThreads, "switch to B: the hold watchdog keeps running");
        Require(g_di == &r.input, "switch to B: the DirectInput instance is kept");
    }
    {   // A retained, request cleared under strict selection: refused, A released, nothing held.
        Rig r; SetPreferredDeviceGuid(nullptr);
        Require(InitDirectInput((int)(INT_PTR)window) == 0, "cleared request: refused");
        Require(g_device == nullptr && IsEqualGUID(g_activeGuid, GUID{}) && g_effect == nullptr, "cleared request: no wheel or identity claimed");
        Require(r.a.effect.releases == 1 && r.a.releases >= 2 && r.a.unacquires >= 1, "cleared request: A's effect and force handle released");
    }
    {   // A retained, B requested but absent: refused with no fallback to A.
        Rig r(false); int opens = r.a.opens;
        SetPreferredDeviceGuid(&GuidB);
        Require(InitDirectInput((int)(INT_PTR)window) == 0, "absent target: refused");
        Require(g_device == nullptr && IsEqualGUID(g_activeGuid, GUID{}), "absent target: nothing held");
        Require(r.a.effect.releases == 1, "absent target: A released, not kept");
        Require(r.a.opens == opens + 1, "absent target: A only inspected by enumeration, never reopened as the force handle");
    }
    {   // A retained, B requested but its open fails: refused, no fallback to A, identity cleared.
        Rig r; r.b.acquireResult = DIERR_OTHERAPPHASPRIO;
        SetPreferredDeviceGuid(&GuidB);
        Require(InitDirectInput((int)(INT_PTR)window) == 0, "failed target open: refused");
        Require(g_device == nullptr && IsEqualGUID(g_activeGuid, GUID{}) && g_effect == nullptr, "failed target open: nothing held");
        Require(r.a.effect.releases == 1, "failed target open: A released, not resumed");
    }
    {   // Historical non-strict behaviour is unchanged: the retained wheel stays.
        Rig r(true, false); SetPreferredDeviceGuid(&GuidB);
        Require(InitDirectInput((int)(INT_PTR)window) == 1 && g_device == &r.a && r.a.effect.releases == 0, "non-strict: retained A kept as before");
    }
    DestroyWindow(window);
    std::printf("PASS retained strict identity: %d checks (same-GUID reuse, switch with watchdog kept, cleared, absent, failed open, non-strict unchanged); fake devices only\n", checks);
    return 0;
}
