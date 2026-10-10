// The production force fence (dbce_fence.hpp) against a recording fake DirectInput device: every force route must leave
// the fake untouched, reads must be forwarded, no interface escapes the wrapper, and test injection must deliver exact
// samples (including the lower range bound) or refuse when the game's range is unreadable. No real device is opened.
#define DIRECTINPUT_VERSION 0x0800
#include "dbce_fence.hpp"
#include <cstring>
#include <iostream>
#include <map>

static int g_checks, g_fail;
static void check(bool ok, const std::string &what) { ++g_checks; if (!ok) { ++g_fail; std::cerr << "FAIL " << what << "\n"; } }
static std::vector<std::string> g_log;
static void logLine(const char *l) { g_log.push_back(l); }
static bool logged(const char *part) { for (auto &l : g_log) if (l.find(part) != std::string::npos) return true; return false; }

static const GUID kInst = {0x000000a1, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0}};
static const char *kInstText = "{000000a1-0000-0000-0000-000000000000}";

struct FakeDevice final : IDirectInputDevice8A {
    std::map<std::string, int> calls;
    LONG refs = 1;
    bool rangeFails = false;
    DWORD lastSetProp = 0;
    void hit(const char *m) { ++calls[m]; }
    int n(const char *m) { return calls.count(m) ? calls[m] : 0; }
    STDMETHODIMP QueryInterface(REFIID, LPVOID *o) override { hit("QueryInterface"); if (o) *o = this; return S_OK; }   // a real device would hand itself out
    STDMETHODIMP_(ULONG) AddRef() override { return ++refs; }
    STDMETHODIMP_(ULONG) Release() override { hit("Release"); return --refs; }
    STDMETHODIMP GetCapabilities(LPDIDEVCAPS c) override { hit("GetCapabilities"); if (c) c->dwFlags = DIDC_ATTACHED | DIDC_FORCEFEEDBACK; return DI_OK; }
    STDMETHODIMP EnumObjects(LPDIENUMDEVICEOBJECTSCALLBACKA, LPVOID, DWORD) override { hit("EnumObjects"); return DI_OK; }
    STDMETHODIMP GetProperty(REFGUID p, LPDIPROPHEADER h) override
    {
        hit("GetProperty");
        if ((const void *)&p == &DIPROP_RANGE) {
            if (rangeFails) return DIERR_OBJECTNOTFOUND;
            ((LPDIPROPRANGE)h)->lMin = -32768; ((LPDIPROPRANGE)h)->lMax = 32767;
        }
        return DI_OK;
    }
    STDMETHODIMP SetProperty(REFGUID p, LPCDIPROPHEADER h) override
    {
        hit("SetProperty");
        lastSetProp = (DWORD)(ULONG_PTR)&p;
        if ((const void *)&p == &DIPROP_AUTOCENTER) { hit(((LPCDIPROPDWORD)h)->dwData == DIPROPAUTOCENTER_OFF ? "SetProperty AUTOCENTER off" : "SetProperty AUTOCENTER on"); }
        if ((const void *)&p == &DIPROP_FFGAIN) hit("SetProperty FFGAIN");
        if ((const void *)&p == &DIPROP_CALIBRATIONMODE) hit("SetProperty CALIBRATIONMODE");
        return DI_OK;
    }
    STDMETHODIMP Acquire() override { hit("Acquire"); return DI_OK; }
    STDMETHODIMP Unacquire() override { hit("Unacquire"); return DI_OK; }
    STDMETHODIMP GetDeviceState(DWORD cb, LPVOID d) override
    {
        hit("GetDeviceState");
        memset(d, 0, cb);
        auto *s = (DIJOYSTATE *)d;
        s->lX = 576; s->lZ = -32768; s->rgdwPOV[0] = (DWORD)-1; s->rgdwPOV[1] = s->rgdwPOV[2] = s->rgdwPOV[3] = (DWORD)-1;
        return DI_OK;
    }
    STDMETHODIMP GetDeviceData(DWORD, LPDIDEVICEOBJECTDATA, LPDWORD k, DWORD) override { hit("GetDeviceData"); if (k) *k = 0; return DI_OK; }
    STDMETHODIMP SetDataFormat(LPCDIDATAFORMAT) override { hit("SetDataFormat"); return DI_OK; }
    STDMETHODIMP SetEventNotification(HANDLE) override { hit("SetEventNotification"); return DI_OK; }
    STDMETHODIMP SetCooperativeLevel(HWND, DWORD) override { hit("SetCooperativeLevel"); return DI_OK; }
    STDMETHODIMP GetObjectInfo(LPDIDEVICEOBJECTINSTANCEA, DWORD, DWORD) override { hit("GetObjectInfo"); return DI_OK; }
    STDMETHODIMP GetDeviceInfo(LPDIDEVICEINSTANCEA d) override { hit("GetDeviceInfo"); if (d) d->guidInstance = kInst; return DI_OK; }
    STDMETHODIMP RunControlPanel(HWND, DWORD) override { hit("RunControlPanel"); return DI_OK; }
    STDMETHODIMP Initialize(HINSTANCE, DWORD, REFGUID) override { hit("Initialize"); return DI_OK; }
    STDMETHODIMP CreateEffect(REFGUID, LPCDIEFFECT, LPDIRECTINPUTEFFECT *, LPUNKNOWN) override { hit("CreateEffect"); return DI_OK; }
    STDMETHODIMP EnumEffects(LPDIENUMEFFECTSCALLBACKA, LPVOID, DWORD) override { hit("EnumEffects"); return DI_OK; }
    STDMETHODIMP GetEffectInfo(LPDIEFFECTINFOA, REFGUID) override { hit("GetEffectInfo"); return DI_OK; }
    STDMETHODIMP GetForceFeedbackState(LPDWORD) override { hit("GetForceFeedbackState"); return DI_OK; }
    STDMETHODIMP SendForceFeedbackCommand(DWORD) override { hit("SendForceFeedbackCommand"); return DI_OK; }
    STDMETHODIMP EnumCreatedEffectObjects(LPDIENUMCREATEDEFFECTOBJECTSCALLBACK, LPVOID, DWORD) override { hit("EnumCreatedEffectObjects"); return DI_OK; }
    STDMETHODIMP Escape(LPDIEFFESCAPE) override { hit("Escape"); return DI_OK; }
    STDMETHODIMP Poll() override { hit("Poll"); return DI_OK; }
    STDMETHODIMP SendDeviceData(DWORD, LPCDIDEVICEOBJECTDATA, LPDWORD, DWORD) override { hit("SendDeviceData"); return DI_OK; }
    STDMETHODIMP EnumEffectsInFile(LPCSTR, LPDIENUMEFFECTSINFILECALLBACK, LPVOID, DWORD) override { hit("EnumEffectsInFile"); return DI_OK; }
    STDMETHODIMP WriteEffectToFile(LPCSTR, DWORD, LPDIFILEEFFECT, DWORD) override { hit("WriteEffectToFile"); return DI_OK; }
    STDMETHODIMP BuildActionMap(LPDIACTIONFORMATA, LPCSTR, DWORD) override { hit("BuildActionMap"); return DI_OK; }
    STDMETHODIMP SetActionMap(LPDIACTIONFORMATA, LPCSTR, DWORD) override { hit("SetActionMap"); return DI_OK; }
    STDMETHODIMP GetImageInfo(LPDIDEVICEIMAGEINFOHEADERA) override { hit("GetImageInfo"); return DI_OK; }
};

struct FakeInput final : IDirectInput8A {
    std::map<std::string, int> calls;
    FakeDevice device;
    LONG refs = 1;
    void hit(const char *m) { ++calls[m]; }
    int n(const char *m) { return calls.count(m) ? calls[m] : 0; }
    STDMETHODIMP QueryInterface(REFIID, LPVOID *o) override { hit("QueryInterface"); if (o) *o = this; return S_OK; }
    STDMETHODIMP_(ULONG) AddRef() override { return ++refs; }
    STDMETHODIMP_(ULONG) Release() override { hit("Release"); return --refs; }
    STDMETHODIMP CreateDevice(REFGUID, LPDIRECTINPUTDEVICE8A *out, LPUNKNOWN) override { hit("CreateDevice"); *out = &device; return DI_OK; }
    STDMETHODIMP EnumDevices(DWORD, LPDIENUMDEVICESCALLBACKA, LPVOID, DWORD) override { hit("EnumDevices"); return DI_OK; }
    STDMETHODIMP GetDeviceStatus(REFGUID) override { hit("GetDeviceStatus"); return DI_OK; }
    STDMETHODIMP RunControlPanel(HWND, DWORD) override { hit("RunControlPanel"); return DI_OK; }
    STDMETHODIMP Initialize(HINSTANCE, DWORD) override { hit("Initialize"); return DI_OK; }
    STDMETHODIMP FindDevice(REFGUID, LPCSTR, LPGUID) override { hit("FindDevice"); return DI_OK; }
    STDMETHODIMP EnumDevicesBySemantics(LPCSTR, LPDIACTIONFORMATA, LPDIENUMDEVICESBYSEMANTICSCBA, LPVOID, DWORD) override { hit("EnumDevicesBySemantics"); return DI_OK; }
    STDMETHODIMP ConfigureDevices(LPDICONFIGUREDEVICESCALLBACK, LPDICONFIGUREDEVICESPARAMSA, DWORD, LPVOID) override { hit("ConfigureDevices"); return DI_OK; }
};

static BOOL CALLBACK semCb(LPCDIDEVICEINSTANCEA, LPDIRECTINPUTDEVICE8A, DWORD, DWORD, LPVOID) { return DIENUM_CONTINUE; }

// The W variants compile with the same rules (DiRT and GRID use IDirectInput8W).
template struct dbce::fence::FencedDevice<IDirectInputDevice8W>;
template struct dbce::fence::FencedDirectInput<IDirectInput8W>;

int main()
{
    using namespace dbce::fence;
    settings().log = logLine;
    dbce::controls::InjectionTable table;
    settings().injections = &table;

    FakeInput fake;
    auto *di = new FencedDirectInput<IDirectInput8A>(&fake);
    void *q = nullptr;
    check(di->QueryInterface(IID_IDirectInput8W, &q) == E_NOINTERFACE && !q, "DirectInput: QueryInterface for W refused");
    check(di->QueryInterface(IID_IDirectInput8A, &q) == S_OK && q == di, "DirectInput: QueryInterface for A gives the wrapper");
    di->Release();
    IDirectInputDevice8A *dev = nullptr;
    check(di->CreateDevice(kInst, &dev, (LPUNKNOWN)&fake) == DIERR_NOAGGREGATION && !dev && fake.n("CreateDevice") == 0, "aggregation refused before the real call");
    check(di->CreateDevice(kInst, &dev, nullptr) == DI_OK && dev && dev != &fake.device && fake.n("CreateDevice") == 1, "the game gets a wrapper, not the device");
    check(di->EnumDevicesBySemantics(nullptr, nullptr, semCb, nullptr, 0) == DI_OK && fake.n("EnumDevicesBySemantics") == 0, "EnumDevicesBySemantics answered without the real call");
    check(FAILED(di->ConfigureDevices(nullptr, nullptr, 0, nullptr)) && fake.n("ConfigureDevices") == 0, "ConfigureDevices refused");
    check(FAILED(di->RunControlPanel(nullptr, 0)) && fake.n("RunControlPanel") == 0, "RunControlPanel refused");
    di->EnumDevices(DI8DEVCLASS_GAMECTRL, nullptr, nullptr, DIEDFL_ATTACHEDONLY);
    check(fake.n("EnumDevices") == 1, "EnumDevices forwarded");

    FakeDevice &f = fake.device;
    for (const IID *iid : {&IID_IDirectInputDevice8W, &IID_IDirectInputDevice2A, &IID_IDirectInputDeviceA, &IID_IDirectInputEffect}) {
        q = nullptr;
        check(dev->QueryInterface(*iid, &q) == E_NOINTERFACE && !q, "device: QueryInterface for another interface refused");
    }
    check(f.n("QueryInterface") == 0, "the device's own QueryInterface is never reached");

    // Force routes: none reaches the device.
    LPDIRECTINPUTEFFECT eff = nullptr;
    DICONSTANTFORCE cf{8000};
    DIEFFECT e{};
    e.dwSize = sizeof e; e.dwFlags = DIEFF_CARTESIAN; e.dwDuration = INFINITE; e.dwGain = 10000; e.cbTypeSpecificParams = sizeof cf; e.lpvTypeSpecificParams = &cf;
    check(dev->CreateEffect(GUID_ConstantForce, &e, &eff, nullptr) == DI_OK && eff, "CreateEffect answered with a software effect");
    check(dev->CreateEffect(GUID_ConstantForce, &e, &eff, (LPUNKNOWN)&f) != DI_OK, "CreateEffect with aggregation refused");
    eff = nullptr;
    dev->CreateEffect(GUID_ConstantForce, &e, &eff, nullptr);
    eff->SetParameters(&e, DIEP_ALLPARAMS | DIEP_START);
    eff->Start(1, 0); eff->Download(); eff->Stop(); eff->Unload();
    DWORD st = 7;
    check(eff->GetEffectStatus(&st) == DI_OK && st == 0, "the software effect never plays");
    DIEFFECT back{}; back.dwSize = sizeof back;
    check(eff->GetParameters(&back, DIEP_GAIN | DIEP_DURATION) == DI_OK && back.dwGain == 10000 && back.dwDuration == INFINITE, "the software effect keeps the game's values");
    check(FAILED(eff->Escape(nullptr)), "effect Escape refused");
    eff->Release();
    dev->SendForceFeedbackCommand(DISFFC_SETACTUATORSON);
    dev->SendForceFeedbackCommand(DISFFC_CONTINUE);
    DWORD ffs = 0;
    check(dev->GetForceFeedbackState(&ffs) == DI_OK && (ffs & DIGFFS_STOPPED), "force feedback state answered");
    check(FAILED(dev->Escape(nullptr)), "device Escape refused");
    check(FAILED(dev->SendDeviceData(sizeof(DIDEVICEOBJECTDATA), nullptr, nullptr, 0)), "SendDeviceData refused");
    check(FAILED(dev->WriteEffectToFile("x", 0, nullptr, 0)) && FAILED(dev->EnumEffectsInFile("x", nullptr, nullptr, 0)), "effect files refused");
    check(FAILED(dev->RunControlPanel(nullptr, 0)) && FAILED(dev->Initialize(nullptr, 0, kInst)), "RunControlPanel and Initialize refused");
    DIPROPDWORD pd{}; pd.diph.dwSize = sizeof pd; pd.diph.dwHeaderSize = sizeof pd.diph; pd.diph.dwHow = DIPH_DEVICE;
    pd.dwData = 10000; dev->SetProperty(DIPROP_FFGAIN, &pd.diph);
    pd.dwData = DIPROPAUTOCENTER_ON; dev->SetProperty(DIPROP_AUTOCENTER, &pd.diph);
    pd.dwData = DIPROPCALIBRATIONMODE_RAW;
    check(FAILED(dev->SetProperty(DIPROP_CALIBRATIONMODE, &pd.diph)), "a property outside the whitelist is refused");
    for (const char *m : {"CreateEffect", "SendForceFeedbackCommand", "GetForceFeedbackState", "Escape", "SendDeviceData", "WriteEffectToFile",
                          "EnumEffectsInFile", "RunControlPanel", "Initialize", "EnumCreatedEffectObjects", "SetProperty FFGAIN", "SetProperty AUTOCENTER on",
                          "SetProperty CALIBRATIONMODE"})
        check(f.n(m) == 0, std::string("never reached the device: ") + m);
    pd.dwData = DIPROPAUTOCENTER_OFF;
    dev->SetProperty(DIPROP_AUTOCENTER, &pd.diph);
    check(f.n("SetProperty AUTOCENTER off") == 1, "auto-centring OFF is forwarded");
    DIPROPRANGE pr{}; pr.diph.dwSize = sizeof pr; pr.diph.dwHeaderSize = sizeof pr.diph; pr.diph.dwHow = DIPH_DEVICE; pr.lMin = -32768; pr.lMax = 32767;
    dev->SetProperty(DIPROP_RANGE, &pr.diph);
    check(f.n("SetProperty") == 2, "range forwarded (two SetProperty calls reached the device in all)");

    // Reads are forwarded.
    DIDEVCAPS caps{}; caps.dwSize = sizeof caps;
    dev->GetCapabilities(&caps); dev->EnumObjects(nullptr, nullptr, DIDFT_ALL); dev->Acquire(); dev->Poll(); dev->Unacquire();
    DWORD k = 0; dev->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), nullptr, &k, DIGDD_PEEK);
    dev->SetDataFormat(&c_dfDIJoystick2); dev->SetCooperativeLevel(nullptr, DISCL_EXCLUSIVE | DISCL_BACKGROUND);
    DIDEVICEINSTANCEA di2{}; di2.dwSize = sizeof di2; dev->GetDeviceInfo(&di2);
    dev->EnumEffects(nullptr, nullptr, DIEFT_ALL);
    for (const char *m : {"GetCapabilities", "EnumObjects", "Acquire", "Poll", "Unacquire", "GetDeviceData", "SetDataFormat", "SetCooperativeLevel", "EnumEffects"})
        check(f.n(m) == 1, std::string("forwarded: ") + m);
    check((caps.dwFlags & DIDC_FORCEFEEDBACK) && IsEqualGUID(di2.guidInstance, kInst), "capabilities and identity are the device's own");

    // Injection: exact samples by instance, the lower bound kept, buttons, hats; nothing for other instances.
    auto inject = [&](const std::string &line) {
        dbce::controls::InjectCommand c; std::string why;
        if (!dbce::controls::parseInject(line, nullptr, c, why)) return "parse: " + why;
        return table.add(c, GetTickCount64());
    };
    DIJOYSTATE2 s{};
    dev->GetDeviceState(sizeof s, &s);
    check(s.lX == 576 && s.lZ == -32768, "no injection: the device's own sample");
    check(inject(std::string("inject raw axis 0 dev=") + kInstText + " value=0 ms=2000").empty(), "axis 0 injection accepted");
    check(inject(std::string("inject raw axis 2 dev=") + kInstText + " value=65535 ms=2000").empty(), "axis 2 injection accepted");
    check(inject(std::string("inject raw axis 1 dev=") + kInstText + " value=32768 ms=2000").empty(), "axis 1 injection accepted");
    check(inject(std::string("inject raw button 40 dev=") + kInstText + " value=1 ms=2000").empty(), "button injection accepted");
    check(inject(std::string("inject raw hat 0 27000 dev=") + kInstText + " value=27000 ms=2000").empty(), "hat injection accepted");
    check(inject("inject raw axis 3 dev={000000b2-0000-0000-0000-000000000000} value=65535 ms=2000").empty(), "another instance's injection accepted");
    dev->GetDeviceState(sizeof s, &s);
    check(s.lX == -32768, "the lower bound is exact (-32768, not -32767): " + std::to_string(s.lX));
    check(s.lZ == 32767 && s.lY == 0, "upper bound and the middle: " + std::to_string(s.lZ) + ", " + std::to_string(s.lY));
    check(s.rgbButtons[40] == 0x80 && s.rgdwPOV[0] == 27000, "button 40 down, hat left");
    check(s.lRx == 0, "another instance's injection does not touch this device");
    struct { DIJOYSTATE s; unsigned char guard[96]; } narrow;
    memset(&narrow, 0x5a, sizeof narrow);
    dev->GetDeviceState(sizeof narrow.s, &narrow.s);
    bool intact = true;
    for (unsigned char b : narrow.guard) intact &= b == 0x5a;
    check(narrow.s.lX == -32768 && intact, "a button beyond DIJOYSTATE is not written past the struct");
    check(inject(std::string("inject raw button 50 dev=") + kInstText + " value=1 ms=2000").empty(), "button 50 injection accepted");
    dev->GetDeviceState(sizeof narrow.s, &narrow.s);
    check(logged("button 50 on {000000a1-0000-0000-0000-000000000000} is outside the 32 buttons"), "a button the game's format lacks is reported");
    check(logged("inject: axis 0 on {000000a1-0000-0000-0000-000000000000} = -32768"), "the delivery is logged with the value");

    // An unreadable range is never guessed.
    FakeInput fake2;
    fake2.device.rangeFails = true;
    auto *di2w = new FencedDirectInput<IDirectInput8A>(&fake2);
    IDirectInputDevice8A *dev2 = nullptr;
    di2w->CreateDevice(kInst, &dev2, nullptr);
    dbce::controls::InjectionTable t2;
    settings().injections = &t2;
    dbce::controls::InjectCommand c; std::string why;
    dbce::controls::parseInject(std::string("inject raw axis 0 dev=") + kInstText + " value=0 ms=2000", nullptr, c, why);
    t2.add(c, GetTickCount64());
    DIJOYSTATE2 s2{};
    dev2->GetDeviceState(sizeof s2, &s2);
    check(s2.lX == 576 && logged("not delivered: the game's range for it is unreadable"), "unreadable range: not delivered, logged");
    dev2->Release(); di2w->Release();
    settings().injections = &table;

    // Expiry.
    dbce::controls::InjectCommand shortOne;
    dbce::controls::parseInject(std::string("inject raw axis 5 dev=") + kInstText + " value=65535 ms=50", nullptr, shortOne, why);
    table.add(shortOne, GetTickCount64());
    dev->GetDeviceState(sizeof s, &s);
    check(s.lRz == 32767, "short injection delivered");
    Sleep(80);
    dev->GetDeviceState(sizeof s, &s);
    check(s.lRz == 0 && logged("axis 5 on {000000a1-0000-0000-0000-000000000000} ended"), "short injection ended and logged");

    // Lifetime: releasing the wrapper releases the device once.
    const int before = f.n("Release");
    check(dev->Release() == 0 && f.n("Release") == before + 1, "the wrapper releases the device");
    di->Release();
    check(fake.n("Release") == 1, "the DirectInput wrapper releases the interface");

    if (g_fail) { std::cerr << g_fail << " of " << g_checks << " fence checks failed\n"; return 1; }
    std::cout << "PASS " << g_checks << " force fence checks (fake device)\n";
    return 0;
}
