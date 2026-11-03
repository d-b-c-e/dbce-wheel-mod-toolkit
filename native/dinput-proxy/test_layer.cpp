// The force layer's production hook boundary (dbce_ffb_layer.cpp) against recording fake DirectInput objects: the layer
// patches the fakes' vtables exactly as it patches dinput8's, so CreateDevice, GetDeviceState and Release run through
// the real hooks. Checks pedal rest shaping by the reported instance, device churn beyond the 32-device table, the
// full-table and unreadable-identity reports, the per-shape status, and failed/successful FFB saves with fake effects.
// Panel feedback uses CPU-only ImGui text emission. No real device, window or renderer is opened.
#define DIRECTINPUT_VERSION 0x0800
#include "dbce_ffb_layer.h"
#include "dbce_shape_state.hpp"
#include "dbce_raw_snapshot.hpp"
#include "../panel/dbce_panel.h"
#include "../third_party/imgui/imgui.h"
#include "../third_party/imgui/imgui_internal.h"
#include <cstring>
#include <iostream>
#include <vector>
#include <algorithm>

static int g_checks, g_fail;
static void check(bool ok, const std::string &what) { ++g_checks; if (!ok) { ++g_fail; std::cerr << "FAIL " << what << "\n"; } }
static std::vector<std::string> g_log;
static void logLine(const char *l) { g_log.push_back(l); }
static int logged(const char *part) { int n = 0; for (auto &l : g_log) n += l.find(part) != std::string::npos ? 1 : 0; return n; }

static int g_live;   // fake devices alive
// Same-address reuse: while g_parkFrees is set, a deleted fake's block is parked and the next new FakeDevice gets it.
static void *g_parked;
static bool g_parkFrees;
struct FakeEffect : IDirectInputEffect {
    LONG refs = 1;
    int starts = 0, stops = 0, updates = 0;
    DWORD iterations = 0;
    LONG magnitude = 0;
    STDMETHODIMP QueryInterface(REFIID, LPVOID *o) override { if (o) *o = this; return S_OK; }
    STDMETHODIMP_(ULONG) AddRef() override { return (ULONG)++refs; }
    STDMETHODIMP_(ULONG) Release() override { return (ULONG)--refs; }
    STDMETHODIMP Initialize(HINSTANCE, DWORD, REFGUID) override { return DI_OK; }
    STDMETHODIMP GetEffectGuid(LPGUID) override { return DI_OK; }
    STDMETHODIMP GetParameters(LPDIEFFECT, DWORD) override { return DI_OK; }
    STDMETHODIMP SetParameters(LPCDIEFFECT e, DWORD) override {
        ++updates;
        if (e && e->lpvTypeSpecificParams) magnitude = ((const DICONSTANTFORCE *)e->lpvTypeSpecificParams)->lMagnitude;
        return DI_OK;
    }
    STDMETHODIMP Start(DWORD n, DWORD) override { ++starts; iterations = n; return DI_OK; }
    STDMETHODIMP Stop() override { ++stops; return DI_OK; }
    STDMETHODIMP GetEffectStatus(LPDWORD) override { return DI_OK; }
    STDMETHODIMP Download() override { return DI_OK; }
    STDMETHODIMP Unload() override { return DI_OK; }
    STDMETHODIMP Escape(LPDIEFFESCAPE) override { return DI_OK; }
};
struct FakeDevice : IDirectInputDevice8A {
    LONG refs = 1;
    GUID inst;
    bool infoFails = false, rangeFails = false;
    FakeEffect *effect = nullptr;
    int stopAll = 0, properties = 0;
    bool rawState = false;
    HRESULT acquireResult=DI_OK,unacquireResult=DI_OK,pollResult=DI_OK,initializeResult=DI_OK;
    void (*onState)()=nullptr;
    void (*onFormat)()=nullptr;
    void (*onInitialize)()=nullptr;
    int initializeCalls=0;
    HINSTANCE lastInitializeInstance=nullptr;DWORD lastInitializeVersion=0;GUID lastInitializeGuid{};
    void *queryAlias=nullptr;
    HRESULT stateResult = DI_OK, formatResult = DI_OK, actionResult = DI_OK, dataResult = DI_OK;
    DWORD queued = 0, lastFlags = 0, lastStride = 0;
    LONG rz = 1743;   // the pedal's raw sample (the game's range is 0..65535 here)
    void (*onFinalRelease)() = nullptr;   // runs after the object is gone, before Release returns
    explicit FakeDevice(const GUID &g) : inst(g) { ++g_live; }
    static void *operator new(size_t n) { if (g_parked) { void *p = g_parked; g_parked = nullptr; return p; } return ::operator new(n); }
    static void operator delete(void *p) { if (g_parkFrees) { g_parked = p; return; } ::operator delete(p); }
    virtual ~FakeDevice() { --g_live; }
    STDMETHODIMP QueryInterface(REFIID, LPVOID *o) override { if (o) *o = queryAlias?queryAlias:this; return S_OK; }
    STDMETHODIMP_(ULONG) AddRef() override { return (ULONG)++refs; }
    STDMETHODIMP_(ULONG) Release() override { LONG n = --refs; if (n == 0) { auto after = onFinalRelease; delete this; if (after) after(); } return (ULONG)n; }
    STDMETHODIMP GetCapabilities(LPDIDEVCAPS) override { return DI_OK; }
    STDMETHODIMP EnumObjects(LPDIENUMDEVICEOBJECTSCALLBACKA, LPVOID, DWORD) override { return DI_OK; }
    STDMETHODIMP GetProperty(REFGUID p, LPDIPROPHEADER h) override
    {
        if ((const void *)&p == &DIPROP_RANGE) {
            if (rangeFails) return DIERR_OBJECTNOTFOUND;
            ((LPDIPROPRANGE)h)->lMin = 0; ((LPDIPROPRANGE)h)->lMax = 65535;
        }
        return DI_OK;
    }
    STDMETHODIMP SetProperty(REFGUID, LPCDIPROPHEADER) override { ++properties; return DI_OK; }
    STDMETHODIMP Acquire() override { return acquireResult; }
    STDMETHODIMP Unacquire() override { return unacquireResult; }
    STDMETHODIMP GetDeviceState(DWORD cb, LPVOID d) override
    {
        if (FAILED(stateResult)) return stateResult;
        if (!d) return DIERR_INVALIDPARAM;
        if (rawState) { memset(d, 0x5a, cb);auto callback=onState;onState=nullptr;if(callback)callback();return stateResult; }
        memset(d, 0, cb);
        auto *s = (DIJOYSTATE *)d;
        s->lX = 33265; s->lZ = 0; s->lRz = rz;
        for (auto &p : s->rgdwPOV) p = (DWORD)-1;
        auto callback=onState;onState=nullptr;if(callback)callback();
        return DI_OK;
    }
    STDMETHODIMP GetDeviceData(DWORD stride, LPDIDEVICEOBJECTDATA d, LPDWORD k, DWORD flags) override
    {
        lastFlags = flags; lastStride = stride;
        if (FAILED(dataResult)) return dataResult;
        if (!k || (stride != sizeof(DIDEVICEOBJECTDATA) && stride != sizeof(DIDEVICEOBJECTDATA_DX3)) || (flags & ~DIGDD_PEEK)) return DIERR_INVALIDPARAM;
        const DWORD n = (std::min)(*k, queued);
        if (d) memset(d, 0x5a, n * stride);
        if (!(flags & DIGDD_PEEK)) queued -= n;
        *k = n;
        return dataResult;
    }
    STDMETHODIMP SetDataFormat(LPCDIDATAFORMAT) override { auto callback=onFormat;onFormat=nullptr;if(callback)callback();return formatResult; }
    STDMETHODIMP SetEventNotification(HANDLE) override { return DI_OK; }
    STDMETHODIMP SetCooperativeLevel(HWND, DWORD) override { return DI_OK; }
    STDMETHODIMP GetObjectInfo(LPDIDEVICEOBJECTINSTANCEA, DWORD, DWORD) override { return DI_OK; }
    STDMETHODIMP GetDeviceInfo(LPDIDEVICEINSTANCEA d) override
    {
        if (infoFails || !d || d->dwSize != sizeof(DIDEVICEINSTANCEA)) return DIERR_INVALIDPARAM;
        d->guidInstance = inst;
        return DI_OK;
    }
    STDMETHODIMP RunControlPanel(HWND, DWORD) override { return DI_OK; }
    STDMETHODIMP Initialize(HINSTANCE instance,DWORD version,REFGUID guid) override {
        ++initializeCalls;lastInitializeInstance=instance;lastInitializeVersion=version;lastInitializeGuid=guid;
        auto callback=onInitialize;onInitialize=nullptr;if(callback)callback();return initializeResult;
    }
    STDMETHODIMP CreateEffect(REFGUID, LPCDIEFFECT, LPDIRECTINPUTEFFECT *e, LPUNKNOWN) override { if (e) *e = effect; return effect ? DI_OK : DIERR_UNSUPPORTED; }
    STDMETHODIMP EnumEffects(LPDIENUMEFFECTSCALLBACKA, LPVOID, DWORD) override { return DI_OK; }
    STDMETHODIMP GetEffectInfo(LPDIEFFECTINFOA, REFGUID) override { return DI_OK; }
    STDMETHODIMP GetForceFeedbackState(LPDWORD) override { return DI_OK; }
    STDMETHODIMP SendForceFeedbackCommand(DWORD cmd) override { if (cmd == DISFFC_STOPALL) ++stopAll; return DI_OK; }
    STDMETHODIMP EnumCreatedEffectObjects(LPDIENUMCREATEDEFFECTOBJECTSCALLBACK, LPVOID, DWORD) override { return DI_OK; }
    STDMETHODIMP Escape(LPDIEFFESCAPE) override { return DI_OK; }
    STDMETHODIMP Poll() override { return pollResult; }
    STDMETHODIMP SendDeviceData(DWORD, LPCDIDEVICEOBJECTDATA, LPDWORD, DWORD) override { return DI_OK; }
    STDMETHODIMP EnumEffectsInFile(LPCSTR, LPDIENUMEFFECTSINFILECALLBACK, LPVOID, DWORD) override { return DI_OK; }
    STDMETHODIMP WriteEffectToFile(LPCSTR, DWORD, LPDIFILEEFFECT, DWORD) override { return DI_OK; }
    STDMETHODIMP BuildActionMap(LPDIACTIONFORMATA, LPCSTR, DWORD) override { return DI_OK; }
    STDMETHODIMP SetActionMap(LPDIACTIONFORMATA, LPCSTR, DWORD) override { return actionResult; }
    STDMETHODIMP GetImageInfo(LPDIDEVICEIMAGEINFOHEADERA) override { return DI_OK; }
};

struct FakeInput : IDirectInput8A {
    LONG refs = 1;
    GUID next{};
    bool nextInfoFails = false;
    FakeDevice *last = nullptr;
    FakeDevice *(*factory)()=nullptr;
    STDMETHODIMP QueryInterface(REFIID, LPVOID *o) override { if (o) *o = this; return S_OK; }
    STDMETHODIMP_(ULONG) AddRef() override { return (ULONG)++refs; }
    STDMETHODIMP_(ULONG) Release() override { return (ULONG)--refs; }
    STDMETHODIMP CreateDevice(REFGUID, LPDIRECTINPUTDEVICE8A *out, LPUNKNOWN) override
    {
        last = factory?factory():new FakeDevice(next);
        last->infoFails = nextInfoFails;
        *out = last;
        return DI_OK;
    }
    STDMETHODIMP EnumDevices(DWORD, LPDIENUMDEVICESCALLBACKA, LPVOID, DWORD) override { return DI_OK; }
    STDMETHODIMP GetDeviceStatus(REFGUID) override { return DI_OK; }
    STDMETHODIMP RunControlPanel(HWND, DWORD) override { return DI_OK; }
    STDMETHODIMP Initialize(HINSTANCE, DWORD) override { return DI_OK; }
    STDMETHODIMP FindDevice(REFGUID, LPCSTR, LPGUID) override { return DI_OK; }
    STDMETHODIMP EnumDevicesBySemantics(LPCSTR, LPDIACTIONFORMATA, LPDIENUMDEVICESBYSEMANTICSCBA, LPVOID, DWORD) override { return DI_OK; }
    STDMETHODIMP ConfigureDevices(LPDICONFIGUREDEVICESCALLBACK, LPDICONFIGUREDEVICESPARAMSA, DWORD, LPVOID) override { return DI_OK; }
};

static const GUID kWheel = {0x000000a1, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0}};
static const GUID kOther = {0x000000b2, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0}};
static const GUID kAskedFor = {0x0006346e, 0, 0, {0, 0, 0x50, 0x49, 0x44, 0x56, 0x49, 0x44}};   // a product GUID, as some games pass

// ImGui text emission only: no renderer, window, GPU, OS input or device. Exercise the actual panel note path.
static std::string panelText()
{
    ImGui::NewFrame();
    ImGui::LogToBuffer();
    dbce::panel::Draw(1920, 1080);
    const std::string text = ImGui::GetCurrentContext()->LogBuffer.c_str();
    ImGui::LogFinish();
    ImGui::Render();
    return text;
}

static void saveStateChecks(IDirectInputDevice8A *dev, dbce::ffb::Options o)
{
    auto *fake = static_cast<FakeDevice *>(dev);
    FakeEffect effect;
    fake->effect = &effect;
    o.mute = false;
    o.gainEmulation = true;
    dbce::ffb::Init(o);   // start the focused memory-only scenario with the existing fake device
    DICONSTANTFORCE force{2000};
    DIEFFECT params{};
    params.dwSize = sizeof params;
    params.dwGain = DI_FFNOMINALMAX;
    params.cbTypeSpecificParams = sizeof force;
    params.lpvTypeSpecificParams = &force;
    IDirectInputEffect *eff = nullptr;
    check(dev->CreateEffect(GUID_ConstantForce, &params, &eff, nullptr) == DI_OK && eff, "recording fake effect created through production hook");
    if (!eff) return;
    dbce::ffb::SetOn(false);
    eff->Start(7, 0);   // the game's request is remembered, but cannot reach the fake while Off
    check(effect.starts == 0 && !dbce::ffb::On(), "Off blocks game Start");
    dbce::ffb::SetOn(true);
    dbce::ffb::SetStrength(75);
    check(dbce::ffb::On() && effect.starts == 1 && effect.iterations == 7, "memory-only enable resumes the game's requested iterations");
    check(dbce::ffb::Strength() == 75 && effect.magnitude == 3000, "memory-only strength preserves gain law");

    char temp[MAX_PATH], root[MAX_PATH];
    const bool tempOk = GetTempPathA(MAX_PATH, temp) && GetTempFileNameA(temp, "dbf", 0, root);
    check(tempOk, "private temp path created");
    if (!tempOk) { eff->Release(); return; }
    const bool rootOk = DeleteFileA(root) && CreateDirectoryA(root, nullptr);
    check(rootOk, "private temp directory created");
    if (!rootOk) { eff->Release(); return; }
    const std::string good = std::string(root) + "\\settings.ini";
    const std::string bad = std::string(root) + "\\blocked.ini";
    check(CreateDirectoryA(bad.c_str(), nullptr) != FALSE, "save-failure target is a directory");
    check(WritePrivateProfileStringA("FFB", "Strength", "50", good.c_str()) != FALSE, "baseline INI created");
    char ini[MAX_PATH];
    strcpy_s(ini, good.c_str());
    o.iniPath = ini;
    dbce::ffb::Init(o);
    dbce::ffb::SetStrength(60);
    check(dbce::ffb::Strength() == 60 && effect.magnitude == 2400 && GetPrivateProfileIntA("FFB", "Strength", 0, ini) == 60, "successful strength save applies and persists exact gain");
    dbce::ffb::SetOn(false);
    check(!dbce::ffb::On() && GetPrivateProfileIntA("FFB", "Enabled", 1, ini) == 0, "successful Off stops and persists");

    strcpy_s(ini, bad.c_str());   // same options pointer, genuine Win32 profile-write failure
    const int starts = effect.starts, updates = effect.updates;
    dbce::ffb::SetOn(true);
    dbce::ffb::SetStrength(100);
    check(!dbce::ffb::On() && effect.starts == starts, "failed enable stays Off and never starts an effect");
    check(dbce::ffb::Strength() == 60 && effect.updates == updates && effect.magnitude == 2400, "failed strength retains last-confirmed value and sends no gain update");
    check(GetPrivateProfileIntA("FFB", "Enabled", 1, good.c_str()) == 0 && GetPrivateProfileIntA("FFB", "Strength", 0, good.c_str()) == 60, "failed writes leave confirmed INI untouched");

    strcpy_s(ini, good.c_str());
    dbce::ffb::SetOn(true);
    check(dbce::ffb::On() && effect.starts == starts + 1 && effect.iterations == 7 && GetPrivateProfileIntA("FFB", "Enabled", 0, ini) == 1, "retry enable persists and resumes the game request");
    strcpy_s(ini, bad.c_str());
    const int liveUpdates = effect.updates;
    dbce::ffb::SetStrength(0);
    dbce::ffb::SetOn(true);
    check(dbce::ffb::On() && dbce::ffb::Strength() == 60 && effect.updates == liveUpdates && effect.starts == starts + 1, "failed changes while On preserve confirmed gain and never restart effects");
    const int stops = effect.stops, stopAll = fake->stopAll;
    dbce::panel::Config c;
    c.ffbOn = dbce::ffb::On;
    c.stopFfb = [] { dbce::ffb::SetOn(false); };
    dbce::panel::Configure(c);
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1920, 1080);
    dbce::panel::Prepare(1920, 1080);
    dbce::panel::BuildFonts();
    unsigned char *pixels; int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    dbce::panel::SetOpen(true);
    panelText();   // settle initial window visibility before capturing text
    dbce::panel::StopFfbFromKey();
    check(!dbce::ffb::On() && effect.stops == stops + 1 && fake->stopAll == stopAll + 1, "F8 stops immediately despite failed persistence");
    check(panelText().find("FFB stopped; Off could not be saved") != std::string::npos, "F8 preserves stopped-but-not-saved panel feedback");
    check(GetPrivateProfileIntA("FFB", "Enabled", 0, good.c_str()) == 1, "failed Off leaves disk enabled but live state safely Off");
    eff->Start(3, 0);
    check(effect.starts == starts + 1, "game Start remains blocked after failed Off save");
    DIPROPDWORD spring{};
    spring.diph.dwSize = sizeof spring;
    spring.diph.dwHeaderSize = sizeof spring.diph;
    spring.dwData = DIPROPAUTOCENTER_ON;
    const int properties = fake->properties;
    dev->SetProperty(DIPROP_AUTOCENTER, &spring.diph);
    check(fake->properties == properties, "autocenter remains blocked after failed Off save");
    strcpy_s(ini, good.c_str());
    dbce::ffb::SetOn(false);
    check(GetPrivateProfileIntA("FFB", "Enabled", 1, ini) == 0 && panelText().find("Off could not be saved") == std::string::npos, "successful Off retry clears failure and persists without enabling");
    dbce::ffb::SetStrength(150);
    check(dbce::ffb::Strength() == 100 && effect.magnitude == 4000, "successful strength still clamps at 100");
    o.mute = true;
    dbce::ffb::Init(o);
    const int mutedStarts = effect.starts, mutedStops = effect.stops, mutedStopAll = fake->stopAll;
    dbce::ffb::SetOn(true);
    dbce::ffb::SetOn(false);
    check(effect.starts == mutedStarts && effect.stops == mutedStops && fake->stopAll == mutedStopAll, "mute preserves existing no-device-call enable/Off behavior");
    dbce::panel::SetOpen(false);
    ImGui::DestroyContext();
    eff->Release();
    fake->effect = nullptr;
    o.iniPath = nullptr;
    dbce::ffb::Init(o);   // no dangling options pointer after this fixture
    WritePrivateProfileStringA(nullptr, nullptr, nullptr, good.c_str());
    check(DeleteFileA(good.c_str()) && RemoveDirectoryA(bad.c_str()) && RemoveDirectoryA(root), "private INI and directories removed");
}

static void inputSuppressionChecks(IDirectInputDevice8A *dev)
{
    auto *fake = static_cast<FakeDevice *>(dev);
    fake->rawState = true;
    dbce::panel::SetOpen(true);
    for (const DIDATAFORMAT *f : {&c_dfDIJoystick, &c_dfDIJoystick2, &c_dfDIKeyboard, &c_dfDIMouse, &c_dfDIMouse2}) {
        alignas(DIJOYSTATE2) unsigned char storage[288]; memset(storage, 0x7b, sizeof storage);
        dev->SetDataFormat(f); dev->GetDeviceState(f->dwDataSize, storage);
        const bool joy = f == &c_dfDIJoystick || f == &c_dfDIJoystick2;
        check(storage[joy ? DIJOFS_BUTTON(0) : 0] == 0 && storage[f->dwDataSize] == 0x7b, "production hook: neutral state with canary");
        if (joy) check(((DIJOYSTATE *)storage)->lX == 0x5a5a5a5a && ((DIJOYSTATE *)storage)->rgdwPOV[0] == (DWORD)-1, "production hook: joystick axes preserved, POV neutral");
    }
    dev->SetDataFormat(&c_dfDIJoystick2);
    DIJOYSTATE2 got{};
    fake->formatResult = DIERR_INVALIDPARAM;
    check(dev->SetDataFormat(nullptr) == DIERR_INVALIDPARAM, "hook format failure preserved");
    dev->GetDeviceState(sizeof got, &got);
    check(got.rgbButtons[0] == 0, "hook failed format keeps layout");
    fake->formatResult = DI_OK; fake->actionResult = DIERR_INVALIDPARAM;
    check(dev->SetActionMap(nullptr, nullptr, 0) == DIERR_INVALIDPARAM, "hook action-map failure preserved");
    dev->GetDeviceState(sizeof got, &got);
    check(got.rgbButtons[0] == 0, "hook failed action map keeps layout");
    fake->actionResult = DI_OK; dev->SetActionMap(nullptr, nullptr, 0);
    dev->GetDeviceState(sizeof got, &got);
    check(got.rgbButtons[0] == 0x5a, "hook successful action map clears layout");
    dev->SetDataFormat(&c_dfDIJoystick2);
    char why[200]; dbce::ffb::AllowInjection(true);
    check(dbce::ffb::Inject(kAskedFor, 2, 40, 1, 0, 1, 2000, why, sizeof why), "hook button injection accepted");
    dev->GetDeviceState(sizeof got, &got);
    check(got.rgbButtons[40] == 0, "hook final suppression follows injection");
    dbce::ffb::AllowInjection(false);
    std::vector<DIOBJECTDATAFORMAT> objects(c_dfDIJoystick2.rgodf, c_dfDIJoystick2.rgodf + c_dfDIJoystick2.dwNumObjs);
    std::swap(objects[0].dwOfs, objects[1].dwOfs);
    DIDATAFORMAT custom = c_dfDIJoystick2; custom.rgodf = objects.data();
    dev->SetDataFormat(&custom); dev->GetDeviceState(sizeof got, &got);
    check(got.rgbButtons[0] == 0x5a, "hook custom same-size state untouched");
    fake->stateResult = DIERR_INPUTLOST;
    memset(&got, 0x7b, sizeof got);
    check(dev->GetDeviceState(sizeof got, &got) == DIERR_INPUTLOST && got.rgbButtons[0] == 0x7b, "hook failed state untouched");
    fake->stateResult = DI_OK;
    for (DWORD flags : {(DWORD)0, (DWORD)DIGDD_PEEK}) for (HRESULT hr : {DI_OK, DI_BUFFEROVERFLOW}) {
        DIDEVICEOBJECTDATA data[2]; fake->queued = 2; fake->dataResult = hr;
        DWORD n = 2;
        check(dev->GetDeviceData(sizeof data[0], data, &n, flags) == hr && n == 0 && fake->lastFlags == flags && fake->lastStride == sizeof data[0], "production buffered hook hides events and preserves flags/HRESULT");
        check(fake->queued == ((flags & DIGDD_PEEK) ? 2u : 0u), "production buffered hook preserves PEEK non-consumption");
    }
    fake->queued = 2; fake->dataResult = DI_OK;
    DWORD n = INFINITE;
    check(dev->GetDeviceData(sizeof(DIDEVICEOBJECTDATA_DX3), nullptr, &n, DIGDD_PEEK) == DI_OK && n == 0 && fake->queued == 2, "hook legacy/count-only PEEK hidden without drain");
    fake->dataResult = DIERR_INPUTLOST; n = 2;
    check(dev->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), nullptr, &n, 0) == DIERR_INPUTLOST && n == 2 && fake->queued == 2, "hook failed buffered call untouched");
    fake->dataResult = DI_OK;
    dbce::panel::SetOpen(false); n = 2;
    check(dev->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), nullptr, &n, 0) == DI_OK && n == 2 && fake->queued == 0, "hook close resumes normal buffered delivery");
    dev->SetDataFormat(&c_dfDIJoystick2); dev->GetDeviceState(sizeof got, &got);
    check(got.rgbButtons[0] == 0x5a, "hook close resumes immediate held button");
    fake->rawState = false;
}

#include "test_raw_snapshot.inc"

template<int N> struct SnapshotSlotDevice final:FakeDevice {
    SnapshotSlotDevice():FakeDevice(GUID{0x1000+N,0,0,{0}}) { }
    STDMETHODIMP GetDeviceState(DWORD cb,LPVOID data) override { return FakeDevice::GetDeviceState(cb,data); }
};
template<int N> static FakeDevice *snapshotFactory() { return new SnapshotSlotDevice<N>; }
static void rawInitializeReentryChecks() {
    using Store=dbce::input::RawSnapshotStore;
    static IDirectInputDevice8A *during=nullptr;static Store *activeStore=nullptr;
    static std::uint64_t activeEpoch=0;
    static bool entered=false,insideAdmission=false,insideSeal=false,originalEntered=false,originalAdmission=false,originalSeal=false;
    static HRESULT initResult=E_FAIL,formatResult=E_FAIL,readResult=E_FAIL;
    for(HRESULT outcome:{DI_OK,DIERR_INPUTLOST}) {
        Store store;dbce::ffb::Options options;options.snapshots=&store;options.mute=true;dbce::ffb::Init(options);
        FakeInput input;input.next=kWheel;IDirectInput8A *di=&input;dbce::ffb::HookDirectInput(di);
        IDirectInputDevice8A *dev=nullptr;di->CreateDevice(kAskedFor,&dev,nullptr);dev->SetDataFormat(&c_dfDIJoystick2);
        auto *fake=static_cast<FakeDevice *>(dev);fake->initializeResult=outcome;
        auto baseline=store.BeginInvocation();DIJOYSTATE2 baselineData{};dev->GetDeviceState(sizeof baselineData,&baselineData);store.Seal(baseline);Store::Snapshot baselineOut;
        check(store.ReadUnique(kWheel,baseline,GetTickCount64(),1000,baselineOut),"raw layer: reentry regression starts from healthy admitted sample");store.EndInvocation(baseline);
        const auto generation=store.Generation(dev);during=dev;activeStore=&store;activeEpoch=store.BeginInvocation();entered=originalEntered=false;
        fake->onInitialize=[] {
            originalEntered=true;during->SetDataFormat(&c_dfDIJoystick2);DIJOYSTATE2 data{};during->GetDeviceState(sizeof data,&data);
            originalSeal=activeStore->Seal(activeEpoch);Store::Snapshot out;
            originalAdmission=activeStore->ReadUnique(kWheel,activeEpoch,GetTickCount64(),1000,out);
        };
        fake->onFormat=[] {
            entered=true;initResult=during->Initialize(nullptr,0x0800,kOther);
            formatResult=during->SetDataFormat(&c_dfDIJoystick2);
            DIJOYSTATE2 data{};readResult=during->GetDeviceState(sizeof data,&data);
            insideSeal=activeStore->Seal(activeEpoch);Store::Snapshot out;
            insideAdmission=activeStore->ReadUnique(kWheel,activeEpoch,GetTickCount64(),1000,out);
        };
        check(dev->SetDataFormat(&c_dfDIJoystick2)==DI_OK&&entered&&initResult==outcome&&formatResult==DI_OK&&readResult==DI_OK,
              "raw layer: outer mutation reenters original Initialize then format/read with HRESULTs preserved");
        check(originalEntered&&!originalSeal&&!originalAdmission&&fake->initializeCalls==1&&fake->lastInitializeInstance==nullptr&&
              fake->lastInitializeVersion==0x0800&&IsEqualGUID(fake->lastInitializeGuid,kOther),
              "raw layer: disabled inside original Initialize; identical arguments forwarded exactly once");
        check(!insideSeal&&!insideAdmission&&store.Generation(dev)==generation,
              "raw layer: Initialize disables before reentrant publication without replacing outstanding mutation");
        DIJOYSTATE2 data{};dev->SetDataFormat(&c_dfDIJoystick2);dev->GetDeviceState(sizeof data,&data);store.Seal(activeEpoch);Store::Snapshot out;
        check(data.lRz==0&&!store.ReadUnique(kWheel,activeEpoch,GetTickCount64(),1000,out),
              "raw layer: after outer completion legacy shaping remains, Initialize admission stays disabled");
        store.EndInvocation(activeEpoch);check(store.BeginInvocation()==0,"raw layer: Initialize cannot reopen store via a fresh invocation");
        dev->Release();options.snapshots=nullptr;dbce::ffb::Init(options);during=nullptr;activeStore=nullptr;
        IDirectInputDevice8A *plain=nullptr;di->CreateDevice(kAskedFor,&plain,nullptr);
        check(plain->Initialize(nullptr,0x0800,kOther)==DI_OK&&static_cast<FakeDevice *>(plain)->initializeCalls==1,
              "raw layer: null option forwards through an already-patched vtable");plain->Release();
    }
}
static void rawSnapshotLayerChecks() {
    using Store=dbce::input::RawSnapshotStore;
    rawSnapshotStoreChecks();
    rawInitializeReentryChecks();
    Store store;dbce::ffb::Options options;options.snapshots=&store;options.mute=true;
    dbce::ffb::Init(options);
    FakeInput input;input.next=kWheel;IDirectInput8A *di=&input;dbce::ffb::HookDirectInput(di);
    IDirectInputDevice8A *dev=nullptr;di->CreateDevice(kAskedFor,&dev,nullptr);
    auto *fake=static_cast<FakeDevice *>(dev);dev->SetDataFormat(&c_dfDIJoystick2);
    auto epoch=store.BeginInvocation();DIJOYSTATE2 state{};
    check(dev->GetDeviceState(sizeof state,&state)==DI_OK,"raw layer: original success preserved");store.Seal(epoch);
    Store::Snapshot out;
    check(store.ReadUnique(kWheel,epoch,GetTickCount64(),1000,out)&&out.state.lRz==1743&&state.lRz==0,
          "raw layer: reported instance, copy before existing pedal shaping");
    const auto generation=out.generation;
    check(!store.ReadUnique(kAskedFor,epoch,GetTickCount64(),1000,out),"raw layer: requested product GUID never substitutes for instance");
    fake->acquireResult=DIERR_INPUTLOST;
    check(dev->Acquire()==DIERR_INPUTLOST&&!store.ReadUnique(kWheel,epoch,GetTickCount64(),1000,out),"raw layer: Acquire failure preserves HRESULT and invalidates");
    store.EndInvocation(epoch);fake->acquireResult=DI_OK;
    epoch=store.BeginInvocation();dev->GetDeviceState(sizeof state,&state);store.Seal(epoch);dev->Unacquire();
    check(!store.ReadUnique(kWheel,epoch,GetTickCount64(),1000,out),"raw layer: Unacquire invalidates sealed sample");store.EndInvocation(epoch);
    DIPROPRANGE range{};range.diph.dwSize=sizeof range;range.diph.dwHeaderSize=sizeof range.diph;range.diph.dwHow=DIPH_DEVICE;range.lMax=65535;
    epoch=store.BeginInvocation();dev->GetDeviceState(sizeof state,&state);store.Seal(epoch);dev->SetProperty(DIPROP_RANGE,&range.diph);
    check(!store.ReadUnique(kWheel,epoch,GetTickCount64(),1000,out),"raw layer: range/property change invalidates");store.EndInvocation(epoch);
    epoch=store.BeginInvocation();dev->GetDeviceState(sizeof state,&state);store.Seal(epoch);dev->SetCooperativeLevel(nullptr,DISCL_NONEXCLUSIVE|DISCL_BACKGROUND);
    check(!store.ReadUnique(kWheel,epoch,GetTickCount64(),1000,out),"raw layer: cooperative-level change invalidates");store.EndInvocation(epoch);
    epoch=store.BeginInvocation();dev->GetDeviceState(sizeof state,&state);fake->stateResult=DIERR_INPUTLOST;
    check(dev->GetDeviceState(sizeof state,&state)==DIERR_INPUTLOST,"raw layer: failed read HRESULT unchanged");store.Seal(epoch);
    check(!store.ReadUnique(kWheel,epoch,GetTickCount64(),1000,out),"raw layer: failed read clears prior success");store.EndInvocation(epoch);fake->stateResult=DI_OK;
    epoch=store.BeginInvocation();dev->GetDeviceState(sizeof state,&state);store.Seal(epoch);fake->pollResult=DIERR_INPUTLOST;
    check(dev->Poll()==DIERR_INPUTLOST&&!store.ReadUnique(kWheel,epoch,GetTickCount64(),1000,out),"raw layer: Poll failure invalidates");store.EndInvocation(epoch);fake->pollResult=DI_OK;

    static IDirectInputDevice8A *during=nullptr;during=dev;
    fake->onFormat=[] { DIJOYSTATE2 local{};during->GetDeviceState(sizeof local,&local); };
    epoch=store.BeginInvocation();dev->SetDataFormat(&c_dfDIJoystick2);store.Seal(epoch);
    check(!store.ReadUnique(kWheel,epoch,GetTickCount64(),1000,out),"raw layer: reentrant read during format call cannot publish");store.EndInvocation(epoch);
    fake->onState=[] { during->Unacquire(); };
    epoch=store.BeginInvocation();dev->GetDeviceState(sizeof state,&state);store.Seal(epoch);
    check(!store.ReadUnique(kWheel,epoch,GetTickCount64(),1000,out),"raw layer: in-flight success loses to mutation");store.EndInvocation(epoch);
    fake->formatResult=DIERR_INVALIDPARAM;check(dev->SetDataFormat(&c_dfDIJoystick2)==DIERR_INVALIDPARAM,"raw layer: format failure outcome preserved");
    epoch=store.BeginInvocation();dev->GetDeviceState(sizeof state,&state);store.Seal(epoch);
    check(state.lRz==0&&!store.ReadUnique(kWheel,epoch,GetTickCount64(),1000,out),"raw layer: legacy layout retained on failure, raw descriptor unavailable");store.EndInvocation(epoch);
    fake->formatResult=DI_OK;dev->SetDataFormat(&c_dfDIJoystick2);
    epoch=store.BeginInvocation();dev->GetDeviceState(sizeof state,&state);store.Seal(epoch);dev->SetActionMap(nullptr,nullptr,0);
    check(!store.ReadUnique(kWheel,epoch,GetTickCount64(),1000,out),"raw layer: action map invalidates standard raw descriptor");store.EndInvocation(epoch);dev->SetDataFormat(&c_dfDIJoystick2);
    epoch=store.BeginInvocation();dev->GetDeviceState(sizeof state,&state);store.Seal(epoch);dev->AddRef();dev->Release();
    check(!store.ReadUnique(kWheel,epoch,GetTickCount64(),1000,out),"raw layer: nonfinal Release requires fresh read");store.EndInvocation(epoch);
    IDirectInputDevice8A *duplicate=nullptr;di->CreateDevice(kAskedFor,&duplicate,nullptr);duplicate->SetDataFormat(&c_dfDIJoystick2);
    epoch=store.BeginInvocation();dev->GetDeviceState(sizeof state,&state);store.Seal(epoch);
    check(!store.ReadUnique(kWheel,epoch,GetTickCount64(),1000,out),"raw layer: two live same-GUID objects are not collapsed");duplicate->Release();store.EndInvocation(epoch);
    input.nextInfoFails=true;IDirectInputDevice8A *blind=nullptr;di->CreateDevice(kAskedFor,&blind,nullptr);input.nextInfoFails=false;
    epoch=store.BeginInvocation();dev->GetDeviceState(sizeof state,&state);store.Seal(epoch);
    check(!store.ReadUnique(kWheel,epoch,GetTickCount64(),1000,out),"raw layer: failed actual identity refuses admission");blind->Release();store.EndInvocation(epoch);

    // Reuse happens inside the original final Release, before the outer hook's cleanup.
    static IDirectInput8A *reuseDi=nullptr;static IDirectInputDevice8A *replacement=nullptr;reuseDi=di;
    g_parkFrees=true;fake->onFinalRelease=[] { g_parkFrees=false;reuseDi->CreateDevice(kAskedFor,&replacement,nullptr); };
    void *address=dev;dev->Release();dev=replacement;fake=static_cast<FakeDevice *>(dev);dev->SetDataFormat(&c_dfDIJoystick2);
    epoch=store.BeginInvocation();dev->GetDeviceState(sizeof state,&state);store.Seal(epoch);
    check(dev==address&&store.ReadUnique(kWheel,epoch,GetTickCount64(),1000,out)&&out.generation>generation,
          "raw layer: final Release reuse keeps new generation alive");store.EndInvocation(epoch);
    int alias=0;fake->queryAlias=&alias;void *answer=nullptr;
    check(dev->QueryInterface(IID_IUnknown,&answer)==S_OK&&answer==&alias,"raw layer: alias result/refcount behavior unchanged");
    check(store.BeginInvocation()==0,"raw layer: unqualified QI alias disables snapshots instead of guessing lifetime");dev->Release();
    options.snapshots=nullptr;dbce::ffb::Init(options);

    Store capacity;options.snapshots=&capacity;dbce::ffb::Init(options);
    FakeDevice *(*factories[])()={snapshotFactory<1>,snapshotFactory<2>,snapshotFactory<3>,snapshotFactory<4>,snapshotFactory<5>,
        snapshotFactory<6>,snapshotFactory<7>,snapshotFactory<8>,snapshotFactory<9>};
    for(auto factory:factories) { input.factory=factory;IDirectInputDevice8A *device=nullptr;di->CreateDevice(kAskedFor,&device,nullptr);device->Release(); }
    check(capacity.BeginInvocation()==0,"raw layer: actual hook-table exhaustion disables optional store");
    options.snapshots=nullptr;dbce::ffb::Init(options);
    check(g_live==0,"raw layer: snapshot tests own no remaining fake device");
}
static void rawIdentityWithoutShapesChecks() {
    dbce::input::RawSnapshotStore store;dbce::ffb::Options options;options.snapshots=&store;options.mute=true;dbce::ffb::Init(options);
    FakeInput fake;fake.next=kOther;IDirectInput8A *di=&fake;dbce::ffb::HookDirectInput(di);
    IDirectInputDevice8A *dev=nullptr;di->CreateDevice(kAskedFor,&dev,nullptr);dev->SetDataFormat(&c_dfDIJoystick2);
    auto epoch=store.BeginInvocation();DIJOYSTATE2 data{};dev->GetDeviceState(sizeof data,&data);store.Seal(epoch);
    dbce::input::RawSnapshotStore::Snapshot out;
    check(store.ReadUnique(kOther,epoch,GetTickCount64(),1000,out)&&out.state.lRz==data.lRz,
          "raw layer: reported identity queried with shapes unset, caller state untouched");
    store.EndInvocation(epoch);dev->Release();options.snapshots=nullptr;dbce::ffb::Init(options);
}

int main()
{
    rawIdentityWithoutShapesChecks();
    dbce::ffb::Options o;
    o.log = logLine;
    o.mute = true;   // no effect ever starts (none is created here either)
    dbce::ffb::Init(o);

    dbce::controls::Binding brake;
    std::string why;
    check(dbce::controls::parse("axis 5 dev={000000a1-0000-0000-0000-000000000000} prod={0006346e-0000-0000-0000-504944564944} range=0..65535 rest=1743 travel=+1", brake, why), "binding parses");
    auto *table = new dbce::controls::ShapeTable;   // process-long, never freed (the documented contract)
    check(table->add("brake", brake, why), "brake shape added");
    check(!dbce::ffb::SetShapes(nullptr) && !dbce::ffb::SetShapes(new dbce::controls::ShapeTable), "an empty table is refused");
    check(dbce::ffb::SetShapes(table), "the table is set");
    auto *second = new dbce::controls::ShapeTable;
    second->add("brake", brake, why);
    check(!dbce::ffb::SetShapes(second) && logged("already set"), "a second table is refused");
    check(dbce::controls::describe(*table)[0].find("not seen yet") != std::string::npos, "status before any read: requested, not seen");

    FakeInput fake;
    IDirectInput8A *di = &fake;
    dbce::ffb::HookDirectInput(di);

    // The game asks for the device by its product GUID; the shape matches the instance the device itself reports.
    fake.next = kWheel;
    LPDIRECTINPUTDEVICE8A dev = nullptr;
    check(di->CreateDevice(kAskedFor, &dev, nullptr) == DI_OK && dev, "CreateDevice through the hook");
    DIJOYSTATE2 s{};
    check(dev->GetDeviceState(sizeof s, &s) == DI_OK, "GetDeviceState through the hook");
    // No SetDataFormat was hooked for this device yet: the read's format is unknown, so the shape is reported, not guessed.
    check(s.lRz == 1743 && logged("NOT corrected: the game reads the device in a non-standard format"), "unknown format: left as read, reported once");
    dev->SetDataFormat(&c_dfDIJoystick2);
    dev->GetDeviceState(sizeof s, &s);
    check(s.lRz == 0 && s.lX == 33265, "standard format: the brake at rest reads released; steering untouched");
    check(logged("first corrected (game range 0..65535)") == 1, "first correction logged once");
    dev->GetDeviceState(sizeof s, &s);
    check(logged("first corrected") == 1, "not logged again");
    const auto lines = dbce::controls::describe(*table);
    check(lines[0].find("corrected, but some reads were not") != std::string::npos, "status: corrected, with the earlier miss kept: " + lines[0]);
    check(table->state(0).shaped == 2 && table->state(0).unavailable == 1, "per-shape counts");

    inputSuppressionChecks(dev);

    // Churn: far more than 32 devices created and released; each new one is still tracked and shaped.
    bool allShaped = true;
    for (int i = 0; i < 80; ++i) {
        fake.next = i % 2 ? kWheel : kOther;
        LPDIRECTINPUTDEVICE8A d = nullptr;
        di->CreateDevice(kAskedFor, &d, nullptr);
        d->SetDataFormat(&c_dfDIJoystick2);
        DIJOYSTATE2 t{};
        d->GetDeviceState(sizeof t, &t);
        allShaped &= (i % 2) ? t.lRz == 0 : t.lRz == 1743;   // only the wheel's instance is shaped
        check(d->Release() == 0, "released");
    }
    check(allShaped && !logged("32 devices are already open"), "80 create/read/release cycles: every wheel read shaped, no capacity loss");

    // 32 open devices at once: the 33rd is reported (and counted, as it is the shaped instance).
    std::vector<LPDIRECTINPUTDEVICE8A> open;
    for (int i = 0; i < 31; ++i) { fake.next = kOther; LPDIRECTINPUTDEVICE8A d = nullptr; di->CreateDevice(kAskedFor, &d, nullptr); open.push_back(d); }
    fake.next = kWheel;
    LPDIRECTINPUTDEVICE8A full = nullptr;
    di->CreateDevice(kAskedFor, &full, nullptr);   // 31 + the first device + this = 33
    check(logged("32 devices are already open") == 1 && table->unidentified() == 1, "a full table is reported and counted");
    full->SetDataFormat(&c_dfDIJoystick2);
    DIJOYSTATE2 f{};
    full->GetDeviceState(sizeof f, &f);
    check(f.lRz == 1743, "the untracked device is left as read");
    full->Release();
    for (auto *d : open) d->Release();

    // A device whose own instance cannot be read is counted and reported.
    fake.next = kWheel; fake.nextInfoFails = true;
    LPDIRECTINPUTDEVICE8A blind = nullptr;
    di->CreateDevice(kAskedFor, &blind, nullptr);
    fake.nextInfoFails = false;
    check(table->unidentified() == 2 && logged("could not be read"), "an unreadable identity is counted and reported");
    blind->SetDataFormat(&c_dfDIJoystick2);
    DIJOYSTATE2 bl{};
    blind->GetDeviceState(sizeof bl, &bl);
    check(bl.lRz == 1743, "and left as read");
    blind->Release();
    check(dbce::controls::describe(*table).back().find("could not identify") != std::string::npos, "status lists the unidentified devices");

    // An unreadable range on the shaped device: left as read, reported once.
    fake.next = kWheel;
    LPDIRECTINPUTDEVICE8A noRange = nullptr;
    di->CreateDevice(kAskedFor, &noRange, nullptr);
    ((FakeDevice *)noRange)->rangeFails = true;
    noRange->SetDataFormat(&c_dfDIJoystick2);
    DIJOYSTATE2 nr{};
    noRange->GetDeviceState(sizeof nr, &nr);
    noRange->GetDeviceState(sizeof nr, &nr);
    check(nr.lRz == 1743 && logged("NOT corrected: the game's range for the axis is unreadable") == 1, "unreadable range: left as read, reported once");
    noRange->Release();

    // A freed device's address handed to a new device before the layer's cleanup (here inside the final Release, as
    // another thread could): the new record has another generation and stays tracked.
    {
        static IDirectInput8A *s_di;
        static LPDIRECTINPUTDEVICE8A s_next;
        s_di = di;
        g_parkFrees = true;
        fake.next = kWheel;
        LPDIRECTINPUTDEVICE8A first = nullptr;
        di->CreateDevice(kAskedFor, &first, nullptr);
        void *address = first;
        ((FakeDevice *)first)->onFinalRelease = [] { g_parkFrees = false; s_di->CreateDevice(kAskedFor, &s_next, nullptr); };
        first->Release();
        check(s_next == address, "the new device got the freed address");
        s_next->SetDataFormat(&c_dfDIJoystick2);
        DIJOYSTATE2 r{};
        s_next->GetDeviceState(sizeof r, &r);
        check(r.lRz == 0, "the new device at the same address is still tracked and shaped (" + std::to_string(r.lRz) + ")");
        s_next->Release();
    }

    saveStateChecks(dev, o);
    dev->Release();
    check(g_live == 0, "every fake device was released (" + std::to_string(g_live) + " left)");
    rawSnapshotLayerChecks();
    if (g_fail) { std::cerr << g_fail << " of " << g_checks << " layer checks failed\n"; return 1; }
    std::cout << "PASS " << g_checks << " force layer hook checks (fake DirectInput, " << (sizeof(void *) * 8) << "-bit)\n";
    return 0;
}
