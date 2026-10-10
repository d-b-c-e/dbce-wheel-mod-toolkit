// The force layer's production hook boundary (dbce_ffb_layer.cpp) against recording fake DirectInput objects: the layer
// patches the fakes' vtables exactly as it patches dinput8's, so CreateDevice, GetDeviceState and Release run through
// the real hooks. Checks pedal rest shaping by the reported instance, device churn beyond the 32-device table, the
// full-table and unreadable-identity reports, and the per-shape status. No real device is opened.
#define DIRECTINPUT_VERSION 0x0800
#include "dbce_ffb_layer.h"
#include "dbce_shape_state.hpp"
#include <cstring>
#include <iostream>
#include <vector>

static int g_checks, g_fail;
static void check(bool ok, const std::string &what) { ++g_checks; if (!ok) { ++g_fail; std::cerr << "FAIL " << what << "\n"; } }
static std::vector<std::string> g_log;
static void logLine(const char *l) { g_log.push_back(l); }
static int logged(const char *part) { int n = 0; for (auto &l : g_log) n += l.find(part) != std::string::npos ? 1 : 0; return n; }

static int g_live;   // fake devices alive
struct FakeDevice : IDirectInputDevice8A {
    LONG refs = 1;
    GUID inst;
    bool infoFails = false, rangeFails = false;
    LONG rz = 1743;   // the pedal's raw sample (the game's range is 0..65535 here)
    explicit FakeDevice(const GUID &g) : inst(g) { ++g_live; }
    virtual ~FakeDevice() { --g_live; }
    STDMETHODIMP QueryInterface(REFIID, LPVOID *o) override { if (o) *o = this; return S_OK; }
    STDMETHODIMP_(ULONG) AddRef() override { return (ULONG)++refs; }
    STDMETHODIMP_(ULONG) Release() override { LONG n = --refs; if (n == 0) delete this; return (ULONG)n; }
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
    STDMETHODIMP SetProperty(REFGUID, LPCDIPROPHEADER) override { return DI_OK; }
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
    STDMETHODIMP CreateEffect(REFGUID, LPCDIEFFECT, LPDIRECTINPUTEFFECT *e, LPUNKNOWN) override { if (e) *e = nullptr; return DIERR_UNSUPPORTED; }
    STDMETHODIMP EnumEffects(LPDIENUMEFFECTSCALLBACKA, LPVOID, DWORD) override { return DI_OK; }
    STDMETHODIMP GetEffectInfo(LPDIEFFECTINFOA, REFGUID) override { return DI_OK; }
    STDMETHODIMP GetForceFeedbackState(LPDWORD) override { return DI_OK; }
    STDMETHODIMP SendForceFeedbackCommand(DWORD) override { return DI_OK; }
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

    dev->Release();
    check(g_live == 0, "every fake device was released (" + std::to_string(g_live) + " left)");
    if (g_fail) { std::cerr << g_fail << " of " << g_checks << " layer checks failed\n"; return 1; }
    std::cout << "PASS " << g_checks << " force layer hook checks (fake DirectInput, " << (sizeof(void *) * 8) << "-bit)\n";
    return 0;
}
