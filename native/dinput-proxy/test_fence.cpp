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
    HRESULT stateResult = DI_OK;   // what GetDeviceState answers; a failure writes nothing
    LONG lo = -32768, hi = 32767;
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
            ((LPDIPROPRANGE)h)->lMin = lo; ((LPDIPROPRANGE)h)->lMax = hi;
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
        if (FAILED(stateResult)) return stateResult;
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

    // The rebuilt standard formats equal DirectInput's own (dinput8.lib), object for object; a swapped copy is not standard.
    check(standardFormat(&c_dfDIJoystick) == 1 && standardFormat(&c_dfDIJoystick2) == 2, "the rebuilt c_dfDIJoystick and c_dfDIJoystick2 match the library's");
    std::vector<DIOBJECTDATAFORMAT> swapped(c_dfDIJoystick2.rgodf, c_dfDIJoystick2.rgodf + c_dfDIJoystick2.dwNumObjs);
    std::swap(swapped[0].dwOfs, swapped[1].dwOfs);
    DIDATAFORMAT custom = c_dfDIJoystick2;
    custom.rgodf = swapped.data();
    check(standardFormat(&custom) == 0 && standardFormat(nullptr) == 0, "a same-size format with X and Y swapped is not standard");

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
    dev->SetDataFormat(&c_dfDIJoystick);   // the 32-button format, read as DIJOYSTATE
    dev->GetDeviceState(sizeof narrow.s, &narrow.s);
    bool intact = true;
    for (unsigned char b : narrow.guard) intact &= b == 0x5a;
    check(narrow.s.lX == -32768 && intact, "a button beyond DIJOYSTATE is not written past the struct");
    check(inject(std::string("inject raw button 50 dev=") + kInstText + " value=1 ms=2000").empty(), "button 50 injection accepted");
    dev->GetDeviceState(sizeof narrow.s, &narrow.s);
    check(logged("button 50 on {000000a1-0000-0000-0000-000000000000} is outside the 32 buttons"), "a button the game's format lacks is reported");
    dev->SetDataFormat(&c_dfDIJoystick2);
    check(logged("inject: axis 0 on {000000a1-0000-0000-0000-000000000000} = -32768"), "the delivery is logged with the value");

    // An unreadable range is never guessed.
    FakeInput fake2;
    fake2.device.rangeFails = true;
    auto *di2w = new FencedDirectInput<IDirectInput8A>(&fake2);
    IDirectInputDevice8A *dev2 = nullptr;
    di2w->CreateDevice(kInst, &dev2, nullptr);
    dev2->SetDataFormat(&c_dfDIJoystick2);
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

    // A custom data format of the same size gets nothing; an action map clears the standard format too.
    {
        FakeInput fake3;
        auto *di3 = new FencedDirectInput<IDirectInput8A>(&fake3);
        IDirectInputDevice8A *dev3 = nullptr;
        di3->CreateDevice(kInst, &dev3, nullptr);
        dev3->SetDataFormat(&custom);
        DIJOYSTATE2 s3{};
        dev3->GetDeviceState(sizeof s3, &s3);
        check(s3.lX == 576 && logged("reads with a data format that is not c_dfDIJoystick/2"), "custom format: not delivered, logged");
        dev3->SetDataFormat(&c_dfDIJoystick2);
        dev3->GetDeviceState(sizeof s3, &s3);
        check(s3.lX == -32768, "standard format again: delivered");
        dev3->SetActionMap(nullptr, nullptr, 0);
        dev3->GetDeviceState(sizeof s3, &s3);
        check(s3.lX == 576, "after SetActionMap: not delivered");
        dev3->SetDataFormat(&c_dfDIJoystick);
        DIJOYSTATE s1{};
        dev3->GetDeviceState(sizeof s1, &s1);
        check(s1.lX == -32768, "c_dfDIJoystick with DIJOYSTATE: delivered");
        dev3->GetDeviceState(sizeof s3, &s3);
        check(s3.lX == 576, "c_dfDIJoystick format but a DIJOYSTATE2-sized read: not delivered");
        // The game sets a new range while an injection runs: the next read maps into it.
        dev3->SetDataFormat(&c_dfDIJoystick2);
        fake3.device.lo = 0; fake3.device.hi = 1000;
        dev3->GetDeviceState(sizeof s3, &s3);
        check(s3.lX == 0 && s3.lZ == 1000 && s3.lY == 500, "range changed mid-injection: mapped into the new range (" + std::to_string(s3.lY) + ")");
        dev3->Release(); di3->Release();
    }
    // Expired injections for a device that is never read free their slots by time.
    {
        dbce::controls::InjectionTable t3;
        dbce::controls::InjectCommand x; std::string w;
        dbce::controls::parseInject("inject raw button 1 dev={000000c3-0000-0000-0000-000000000000} value=1 ms=50", nullptr, x, w);
        bool filled = true;
        for (int i = 0; i < 16; ++i) { x.binding.index = i; filled &= t3.add(x, 0).empty(); }
        x.binding.index = 20;
        check(filled && !t3.add(x, 10).empty(), "16 running injections: the 17th is refused");
        check(t3.add(x, 1000).empty(), "after they expire, a new injection is accepted without any read");
    }

    // Expiry.
    dbce::controls::InjectCommand shortOne;
    dbce::controls::parseInject(std::string("inject raw axis 5 dev=") + kInstText + " value=65535 ms=50", nullptr, shortOne, why);
    table.add(shortOne, GetTickCount64());
    dev->GetDeviceState(sizeof s, &s);
    check(s.lRz == 32767, "short injection delivered");
    Sleep(80);
    dev->GetDeviceState(sizeof s, &s);
    check(s.lRz == 0 && logged("axis 5 on {000000a1-0000-0000-0000-000000000000} ended"), "short injection ended and logged");

    // Pedal rest shaping (dbce_shape_state.hpp): after injection, by the reported instance, standard reads only.
    {
        dbce::controls::Binding brake;
        std::string bw;
        check(dbce::controls::parse(std::string("axis 5 dev=") + kInstText + " prod={0006346e-0000-0000-0000-504944564944} range=0..65535 rest=1743 travel=+1", brake, bw), "shape binding parses");
        dbce::controls::ShapeTable shapes;
        check(shapes.add("brake", brake, bw), "brake shape added (" + bw + ")");
        // (Earlier 2-second injections on axes 0-2, button 40 and the hat may still run; they apply to both reads.)
        DIJOYSTATE2 plain{};
        dev->GetDeviceState(sizeof plain, &plain);
        check(plain.lRz == 0, "no shape table: the device's own axis 5 sample");
        settings().shapes = &shapes;
        DIJOYSTATE2 shaped{};
        dev->GetDeviceState(sizeof shaped, &shaped);
        check(shaped.lRz == dbce::controls::shapePedal("brake", brake, 0, -32768, 32767) && shaped.lRz < 0,
              "the shaped axis maps through the game's range -32768..32767 (" + std::to_string(shaped.lRz) + ")");
        DIJOYSTATE2 other = shaped;
        other.lRz = plain.lRz;
        check(memcmp(&other, &plain, sizeof plain) == 0, "every other field (axes, buttons, hats) is untouched");
        // Injection first, then the shape: an injected raw rest reads released, an injected full end reads full.
        check(inject(std::string("inject raw axis 5 dev=") + kInstText + " value=1743 ms=2000").empty(), "rest injection accepted");
        dev->GetDeviceState(sizeof shaped, &shaped);
        check(shaped.lRz == -32768, "injected rest, then shaped: released (" + std::to_string(shaped.lRz) + ")");
        check(inject(std::string("inject raw axis 5 dev=") + kInstText + " value=65535 ms=2000").empty(), "full injection accepted");
        dev->GetDeviceState(sizeof shaped, &shaped);
        check(shaped.lRz == 32767, "injected full end, then shaped: full");
        check(inject(std::string("inject raw axis 5 dev=") + kInstText + " value=33639 ms=2000").empty(), "half injection accepted");
        dev->GetDeviceState(sizeof shaped, &shaped);
        check(shaped.lRz >= -1 && shaped.lRz <= 0, "injected half travel, then shaped: the range's middle (" + std::to_string(shaped.lRz) + ", -0.5 rounded)");
        // A required shape that cannot be applied is reported, and the axis is left as read.
        FakeInput fake4;
        fake4.device.rangeFails = true;
        auto *di4 = new FencedDirectInput<IDirectInput8A>(&fake4);
        IDirectInputDevice8A *dev4 = nullptr;
        di4->CreateDevice(kInst, &dev4, nullptr);
        dev4->SetDataFormat(&c_dfDIJoystick2);
        DIJOYSTATE2 s4{};
        dev4->GetDeviceState(sizeof s4, &s4);
        check(s4.lRz == 0 && logged("NOT corrected: the game's range for the axis is unreadable"), "unreadable range: left as read, reported");
        g_log.clear();
        fake4.device.rangeFails = false;
        dev4->SetDataFormat(&custom);
        dev4->GetDeviceState(sizeof s4, &s4);
        check(s4.lRz == 0 && logged("NOT corrected: the game reads the device in a non-standard format"), "a custom format: left as read, reported");
        check(shapes.state(0).shaped > 0 && shapes.state(0).unavailable == 2 && shapes.state(0).lastMiss == dbce::controls::ShapeMiss::Format, "the corrections and both misses are recorded");
        dev4->Release(); di4->Release();
        settings().shapes = nullptr;
        dev->GetDeviceState(sizeof shaped, &shaped);
        check(shaped.lRz == dbce::controls::rescale(33639, 0, 65535, -32768, 32767), "no table: the injected sample unshaped (" + std::to_string(shaped.lRz) + ")");
    }

    // The observer (Settings::observe, sana 107988): the final standard-format state, after injection, for the wrapped
    // device; nothing for a failed, custom-format or mis-sized read; no observer, no call.
    {
        struct Seen { const void *device; int format; DWORD cb; LONG lX, lZ; BYTE button40; int calls; };
        static Seen seen;
        seen = Seen{};
        settings().observe = [](const void *device, int format, DWORD cb, const void *state) {
            auto *s = (const DIJOYSTATE *)state;
            seen = Seen{device, format, cb, s->lX, s->lZ, cb == sizeof(DIJOYSTATE2) ? ((const DIJOYSTATE2 *)state)->rgbButtons[40] : (BYTE)0, seen.calls + 1};
        };
        dev->SetDataFormat(&c_dfDIJoystick2);
        check(inject(std::string("inject raw axis 0 dev=") + kInstText + " value=0 ms=2000").empty() &&
              inject(std::string("inject raw button 40 dev=") + kInstText + " value=1 ms=2000").empty(), "observer: injections accepted");
        DIJOYSTATE2 got{};
        dev->GetDeviceState(sizeof got, &got);
        check(seen.calls == 1 && seen.device == static_cast<IDirectInputDevice8A *>(&f) && seen.format == 2 && seen.cb == sizeof(DIJOYSTATE2),
              "observer: one call for the read, with the wrapped device, format 2 and the DIJOYSTATE2 size");
        check(seen.lX == got.lX && seen.lX == -32768 && seen.lZ == got.lZ && seen.button40 == 0x80 && got.rgbButtons[40] == 0x80,
              "observer: sees the injected values the game received (lX " + std::to_string(seen.lX) + ")");
        dev->SetDataFormat(&c_dfDIJoystick);
        DIJOYSTATE got1{};
        dev->GetDeviceState(sizeof got1, &got1);
        check(seen.calls == 2 && seen.format == 1 && seen.cb == sizeof(DIJOYSTATE) && seen.lX == got1.lX, "observer: c_dfDIJoystick with a DIJOYSTATE is observed as format 1");
        dev->GetDeviceState(sizeof got, &got);
        check(seen.calls == 2, "observer: a c_dfDIJoystick format read with a DIJOYSTATE2 size is not observed");
        dev->SetDataFormat(&custom);
        dev->GetDeviceState(sizeof got, &got);
        check(seen.calls == 2, "observer: a custom-format read is not observed");
        dev->SetDataFormat(&c_dfDIJoystick2);
        f.stateResult = DIERR_INPUTLOST;
        check(dev->GetDeviceState(sizeof got, &got) == DIERR_INPUTLOST && seen.calls == 2, "observer: a failed read is not observed");
        f.stateResult = DI_OK;
        settings().observe = nullptr;
        dev->GetDeviceState(sizeof got, &got);
        check(seen.calls == 2 && got.lX == -32768, "observer: none set, no call; the read is unchanged");
    }

    // The effect observer (Settings::observeEffect, sana 109034): every request in order with the game's own parameters,
    // DIEP_START inside one Parameters event, Start's flags and iterations, exactly one Released; a throwing observer
    // changes no result; nothing reaches the device; no observer, no call.
    {
        struct Ev { const void *device, *effect; GUID type; EffectEvent event; const void *params; DWORD flags, value; LONG magnitude; };
        static std::vector<Ev> evs;
        evs.clear();
        auto record = [](const void *device, const void *effect, REFGUID type, EffectEvent event, LPCDIEFFECT p, DWORD flags, DWORD value) {
            LONG m = p && p->lpvTypeSpecificParams && p->cbTypeSpecificParams >= sizeof(DICONSTANTFORCE) ? ((const DICONSTANTFORCE *)p->lpvTypeSpecificParams)->lMagnitude : 0;
            evs.push_back(Ev{device, effect, type, event, p, flags, value, m});
        };
        settings().observeEffect = record;
        const int deviceEffects = f.n("CreateEffect"), deviceCommands = f.n("SendForceFeedbackCommand"), deviceEscapes = f.n("Escape");
        DICONSTANTFORCE cf{7500}; DWORD ax = DIJOFS_X; LONG dir = 1;
        DIEFFECT pe{}; pe.dwSize = sizeof pe; pe.dwGain = 8000; pe.cAxes = 1; pe.rgdwAxes = &ax; pe.rglDirection = &dir;
        pe.cbTypeSpecificParams = sizeof cf; pe.lpvTypeSpecificParams = &cf;
        LPDIRECTINPUTEFFECT fx = nullptr;
        check(dev->CreateEffect(GUID_ConstantForce, &pe, &fx, nullptr) == DI_OK && fx, "effect observer: CreateEffect answered");
        const void *id = fx;
        check(evs.size() == 1 && evs[0].event == EffectEvent::Created && evs[0].device == static_cast<IDirectInputDevice8A *>(&f) && evs[0].effect == id &&
              IsEqualGUID(evs[0].type, GUID_ConstantForce) && evs[0].params == &pe && evs[0].flags == DIEP_ALLPARAMS && evs[0].magnitude == 7500,
              "effect observer: Created with the wrapped device, the effect, its type and the game's own parameters");
        cf.lMagnitude = -5000;
        check(fx->SetParameters(&pe, DIEP_TYPESPECIFICPARAMS | DIEP_START) == DI_OK, "effect observer: SetParameters answered");
        check(evs.size() == 2 && evs[1].event == EffectEvent::Parameters && evs[1].flags == (DIEP_TYPESPECIFICPARAMS | DIEP_START) && evs[1].magnitude == -5000 && evs[1].effect == id,
              "effect observer: SetParameters with DIEP_START is one Parameters event, with no Start");
        check(fx->Start(3, DIES_SOLO) == DI_OK, "effect observer: Start answered");
        check(evs.size() == 3 && evs[2].event == EffectEvent::Start && evs[2].flags == DIES_SOLO && evs[2].value == 3 && !evs[2].params,
              "effect observer: Start with its flags and iteration count");
        check(fx->Stop() == DI_OK && evs.size() == 4 && evs[3].event == EffectEvent::Stop, "effect observer: Stop");
        fx->AddRef();
        check(fx->Release() == 1 && evs.size() == 4, "effect observer: a release that is not the last is no event");
        // A throwing observer changes nothing the game gets, including the last Release.
        settings().observeEffect = [](const void *, const void *, REFGUID, EffectEvent, LPCDIEFFECT, DWORD, DWORD) { throw 1; };
        LPDIRECTINPUTEFFECT fx2 = nullptr;
        check(fx->SetParameters(&pe, DIEP_GAIN) == DI_OK && fx->Start(1, 0) == DI_OK && fx->Stop() == DI_OK, "effect observer: a throw changes no effect result");
        check(dev->CreateEffect(GUID_Sine, &pe, &fx2, nullptr) == DI_OK && fx2 && fx2->Release() == 0, "effect observer: CreateEffect and the last Release still answer while one throws");
        check(logged("an effect observer threw; ignored"), "effect observer: a throw is logged");
        settings().observeEffect = record;
        check(fx->Release() == 0, "effect observer: the last Release");
        check(evs.size() == 5 && evs[4].event == EffectEvent::Released && evs[4].effect == id && !evs[4].params, "effect observer: exactly one Released, at the last Release");
        check(f.n("CreateEffect") == deviceEffects && f.n("SendForceFeedbackCommand") == deviceCommands && f.n("Escape") == deviceEscapes &&
              f.n("SetProperty FFGAIN") == 0, "effect observer: nothing reached the device");
        settings().observeEffect = nullptr;
        LPDIRECTINPUTEFFECT fx3 = nullptr;
        check(dev->CreateEffect(GUID_ConstantForce, &pe, &fx3, nullptr) == DI_OK && fx3 && fx3->Release() == 0 && evs.size() == 5, "effect observer: none set, no call");
    }
    // A throwing state observer changes no read.
    {
        settings().observe = [](const void *, int, DWORD, const void *) { throw 2; };
        dev->SetDataFormat(&c_dfDIJoystick2);
        DIJOYSTATE2 st{};
        check(dev->GetDeviceState(sizeof st, &st) == DI_OK && logged("a state observer threw; ignored"), "observer: a throwing state observer changes no read result");
        settings().observe = nullptr;
    }

    // Lifetime: releasing the wrapper releases the device once.
    const int before = f.n("Release");
    check(dev->Release() == 0 && f.n("Release") == before + 1, "the wrapper releases the device");
    di->Release();
    check(fake.n("Release") == 1, "the DirectInput wrapper releases the interface");

    if (g_fail) { std::cerr << g_fail << " of " << g_checks << " fence checks failed\n"; return 1; }
    std::cout << "PASS " << g_checks << " force fence checks (fake device)\n";
    return 0;
}
