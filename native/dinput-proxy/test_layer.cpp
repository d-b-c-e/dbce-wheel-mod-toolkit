// The force layer's production hook boundary (dbce_ffb_layer.cpp) against recording fake DirectInput objects: the layer
// patches the fakes' vtables exactly as it patches dinput8's, so CreateDevice, GetDeviceState and Release run through
// the real hooks. Checks pedal rest shaping by the reported instance, device churn beyond the 32-device table, the
// full-table and unreadable-identity reports, the per-shape status, and failed/successful FFB saves with fake effects.
// Panel feedback uses CPU-only ImGui text emission. No real device, window or renderer is opened.
#define DIRECTINPUT_VERSION 0x0800
#include "dbce_ffb_layer.h"
#include "dbce_shape_state.hpp"
#include "../panel/dbce_panel.h"
#include "../third_party/imgui/imgui.h"
#include "../third_party/imgui/imgui_internal.h"
#include <cstring>
#include <iostream>
#include <vector>

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
    LONG rz = 1743;   // the pedal's raw sample (the game's range is 0..65535 here)
    void (*onFinalRelease)() = nullptr;   // runs after the object is gone, before Release returns
    explicit FakeDevice(const GUID &g) : inst(g) { ++g_live; }
    static void *operator new(size_t n) { if (g_parked) { void *p = g_parked; g_parked = nullptr; return p; } return ::operator new(n); }
    static void operator delete(void *p) { if (g_parkFrees) { g_parked = p; return; } ::operator delete(p); }
    virtual ~FakeDevice() { --g_live; }
    STDMETHODIMP QueryInterface(REFIID, LPVOID *o) override { if (o) *o = this; return S_OK; }
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
    STDMETHODIMP Acquire() override { return DI_OK; }
    STDMETHODIMP Unacquire() override { return DI_OK; }
    STDMETHODIMP GetDeviceState(DWORD cb, LPVOID d) override
    {
        memset(d, 0, cb);
        auto *s = (DIJOYSTATE *)d;
        s->lX = 33265; s->lZ = 0; s->lRz = rz;
        for (auto &p : s->rgdwPOV) p = (DWORD)-1;
        return DI_OK;
    }
    STDMETHODIMP GetDeviceData(DWORD, LPDIDEVICEOBJECTDATA, LPDWORD k, DWORD) override { if (k) *k = 0; return DI_OK; }
    STDMETHODIMP SetDataFormat(LPCDIDATAFORMAT) override { return DI_OK; }
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
    STDMETHODIMP Initialize(HINSTANCE, DWORD, REFGUID) override { return DI_OK; }
    STDMETHODIMP CreateEffect(REFGUID, LPCDIEFFECT, LPDIRECTINPUTEFFECT *e, LPUNKNOWN) override { if (e) *e = effect; return effect ? DI_OK : DIERR_UNSUPPORTED; }
    STDMETHODIMP EnumEffects(LPDIENUMEFFECTSCALLBACKA, LPVOID, DWORD) override { return DI_OK; }
    STDMETHODIMP GetEffectInfo(LPDIEFFECTINFOA, REFGUID) override { return DI_OK; }
    STDMETHODIMP GetForceFeedbackState(LPDWORD) override { return DI_OK; }
    STDMETHODIMP SendForceFeedbackCommand(DWORD cmd) override { if (cmd == DISFFC_STOPALL) ++stopAll; return DI_OK; }
    STDMETHODIMP EnumCreatedEffectObjects(LPDIENUMCREATEDEFFECTOBJECTSCALLBACK, LPVOID, DWORD) override { return DI_OK; }
    STDMETHODIMP Escape(LPDIEFFESCAPE) override { return DI_OK; }
    STDMETHODIMP Poll() override { return DI_OK; }
    STDMETHODIMP SendDeviceData(DWORD, LPCDIDEVICEOBJECTDATA, LPDWORD, DWORD) override { return DI_OK; }
    STDMETHODIMP EnumEffectsInFile(LPCSTR, LPDIENUMEFFECTSINFILECALLBACK, LPVOID, DWORD) override { return DI_OK; }
    STDMETHODIMP WriteEffectToFile(LPCSTR, DWORD, LPDIFILEEFFECT, DWORD) override { return DI_OK; }
    STDMETHODIMP BuildActionMap(LPDIACTIONFORMATA, LPCSTR, DWORD) override { return DI_OK; }
    STDMETHODIMP SetActionMap(LPDIACTIONFORMATA, LPCSTR, DWORD) override { return DI_OK; }
    STDMETHODIMP GetImageInfo(LPDIDEVICEIMAGEINFOHEADERA) override { return DI_OK; }
};

struct FakeInput : IDirectInput8A {
    LONG refs = 1;
    GUID next{};
    bool nextInfoFails = false;
    FakeDevice *last = nullptr;
    STDMETHODIMP QueryInterface(REFIID, LPVOID *o) override { if (o) *o = this; return S_OK; }
    STDMETHODIMP_(ULONG) AddRef() override { return (ULONG)++refs; }
    STDMETHODIMP_(ULONG) Release() override { return (ULONG)--refs; }
    STDMETHODIMP CreateDevice(REFGUID, LPDIRECTINPUTDEVICE8A *out, LPUNKNOWN) override
    {
        last = new FakeDevice(next);
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

int main()
{
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
    if (g_fail) { std::cerr << g_fail << " of " << g_checks << " layer checks failed\n"; return 1; }
    std::cout << "PASS " << g_checks << " force layer hook checks (fake DirectInput, " << (sizeof(void *) * 8) << "-bit)\n";
    return 0;
}
