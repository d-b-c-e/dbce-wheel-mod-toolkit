// Test injection and its process-latched no-force mode (STD-033 section 6; Astra 3842/3921), against the actual
// exports with fake COM devices. The latch is per process, so each scenario is its own run:
//   injection_test.exe armed | forcefirst | effectfirst | externalfirst | race
// No system DirectInput object, window, game or hardware is opened.
#include "../WheelFfb.cpp"
#include <cassert>
#include <cstdio>
#include <string>
#include <thread>

static int g_checks;
#define CHECK(cond) do { ++g_checks; if (!(cond)) { std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #cond); std::exit(1); } } while (0)

struct FakeDevice : IDirectInputDevice8A {
    DIJOYSTATE2 state = {};
    int reads = 0, creates = 0, commands = 0, acquires = 0, properties = 0;
    HRESULT readResult = S_OK;
    bool rangeFails = false;
    void (*onRange)() = nullptr;   // called from inside GetProperty(DIPROP_RANGE): a same-thread callback, as a driver could
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override { return E_NOINTERFACE; }
    ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
    ULONG STDMETHODCALLTYPE Release() override { return 1; }
    HRESULT STDMETHODCALLTYPE GetCapabilities(LPDIDEVCAPS) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE EnumObjects(LPDIENUMDEVICEOBJECTSCALLBACKA, LPVOID, DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetProperty(REFGUID p, LPDIPROPHEADER h) override
    {
        if (&p != &DIPROP_RANGE || rangeFails) return DIERR_UNSUPPORTED;
        if (onRange) onRange();
        ((LPDIPROPRANGE)h)->lMin = 0; ((LPDIPROPRANGE)h)->lMax = 65535;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetProperty(REFGUID, LPCDIPROPHEADER) override { ++properties; return S_OK; }
    HRESULT STDMETHODCALLTYPE Acquire() override { ++acquires; return S_OK; }
    HRESULT STDMETHODCALLTYPE Unacquire() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetDeviceState(DWORD size, LPVOID data) override
    {
        assert(size == sizeof(state)); ++reads;
        memcpy(data, &state, sizeof(state));
        return readResult;
    }
    HRESULT STDMETHODCALLTYPE GetDeviceData(DWORD, LPDIDEVICEOBJECTDATA, LPDWORD, DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetDataFormat(LPCDIDATAFORMAT) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetEventNotification(HANDLE) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetCooperativeLevel(HWND, DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetObjectInfo(LPDIDEVICEOBJECTINSTANCEA, DWORD, DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetDeviceInfo(LPDIDEVICEINSTANCEA) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE RunControlPanel(HWND, DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Initialize(HINSTANCE, DWORD, REFGUID) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateEffect(REFGUID, LPCDIEFFECT, LPDIRECTINPUTEFFECT*, LPUNKNOWN) override { ++creates; return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE EnumEffects(LPDIENUMEFFECTSCALLBACKA, LPVOID, DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetEffectInfo(LPDIEFFECTINFOA, REFGUID) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetForceFeedbackState(LPDWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SendForceFeedbackCommand(DWORD) override { ++commands; return S_OK; }
    HRESULT STDMETHODCALLTYPE EnumCreatedEffectObjects(LPDIENUMCREATEDEFFECTOBJECTSCALLBACK, LPVOID, DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Escape(LPDIEFFESCAPE) override { ++commands; return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Poll() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SendDeviceData(DWORD, LPCDIDEVICEOBJECTDATA, LPDWORD, DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE EnumEffectsInFile(LPCSTR, LPDIENUMEFFECTSINFILECALLBACK, LPVOID, DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE WriteEffectToFile(LPCSTR, DWORD, LPDIFILEEFFECT, DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE BuildActionMap(LPDIACTIONFORMATA, LPCSTR, DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetActionMap(LPDIACTIONFORMATA, LPCSTR, DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetImageInfo(LPDIDEVICEIMAGEINFOHEADERA) override { return E_NOTIMPL; }
};

// CreateDevice hands out `next`; EnumDevices lists one force-feedback candidate (the R12) so InitDirectInput gets as
// far as opening it when the gate lets it.
struct FakeInput : IDirectInput8A {
    std::atomic<int> creates{0};
    FakeDevice* next = nullptr;
    GUID ffbGuid = {};
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override { return E_NOINTERFACE; }
    ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
    ULONG STDMETHODCALLTYPE Release() override { return 1; }
    HRESULT STDMETHODCALLTYPE CreateDevice(REFGUID, LPDIRECTINPUTDEVICE8A* out, LPUNKNOWN) override
    {
        ++creates; if (!next) return E_NOTIMPL; *out = next; return S_OK;
    }
    HRESULT STDMETHODCALLTYPE EnumDevices(DWORD, LPDIENUMDEVICESCALLBACKA, LPVOID, DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetDeviceStatus(REFGUID) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE RunControlPanel(HWND, DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Initialize(HINSTANCE, DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE FindDevice(REFGUID, LPCSTR, LPGUID) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE EnumDevicesBySemantics(LPCSTR, LPDIACTIONFORMATA, LPDIENUMDEVICESBYSEMANTICSCBA, LPVOID, DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE ConfigureDevices(LPDICONFIGUREDEVICESCALLBACK, LPDICONFIGUREDEVICESPARAMSA, DWORD, LPVOID) override { return E_NOTIMPL; }
};

// Synthetic fixture identity (not any real device instance).
static const GUID kR12 = {0x000000a1, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01}};
static const char* kR12Text = "{000000a1-0000-0000-0000-000000000001}";
static std::string TStatus() { char b[256]; GetTestInjectionStatus(b, sizeof b); return b; }
static int TSubmit(const std::string& line, std::string* why = nullptr)
{
    char w[200] = "x";
    const int r = SubmitTestInjection(line.c_str(), w, sizeof w);
    if (why) *why = w;
    return r;
}
static std::string TRaw(const char* kind, int index, long value, int ms, const char* angle = nullptr)
{
    char b[300];
    if (angle) sprintf_s(b, "inject raw %s %d %s dev=%s value=%ld ms=%d", kind, index, angle, kR12Text, value, ms);
    else sprintf_s(b, "inject raw %s %d dev=%s value=%ld ms=%d", kind, index, kR12Text, value, ms);
    return b;
}

struct TRead {
    int axes[8], pov[4];
    unsigned char buttons[128];
    int ok = 0;
    void pov_(int slot) { ok = ReadDeviceStateWithPov(slot, axes, 8, buttons, 128, pov, 4); }
    void legacy(int slot) { ok = ReadDeviceState(slot, axes, buttons, 128); }
};

// Every force export, in the armed mode: refused, and the fake never touched.
static void forceExportsRefused(FakeInput& input, FakeDevice& ffb)
{
    const int creates = input.creates.load();
    input.next = &ffb;
    g_candidateCount = 1; g_candidates[0].guid = kR12; strcpy_s(g_candidates[0].name, "Fixture wheel");
    CHECK(InitDirectInput(0) == 0);
    CHECK(OpenCandidate(0) == 0);   // the internal open (reacquire/self-heal paths) refuses too
    Aquire();
    CHECK(SetDeviceForcesXY(5000, 0) == 0);
    CHECK(CreatePeriodicEffect(10) == -1);
    CHECK(UpdatePeriodicEffect(0, 5000, 10000) == 0);
    CHECK(CreatePeriodicBurst(10, 100) == -1);
    CHECK(PlayPeriodicBurst(0, 5000, 10000) == 0);
    CHECK(PlayShapedPeriodicBurst(0, 5000, 10000, 0, 0) == 0);
    CHECK(CreateConstantBurst(100) == -1);
    CHECK(PlayConstantBurst(0, 5000) == 0);
    CHECK(CreateConditionEffect(0) == -1);
    CHECK(UpdateConditionEffect(0, 5000, 10000, 0, 0) == 0);
    CHECK(StartEffect() == FALSE);
    CHECK(SetAutoCenter(TRUE) == FALSE);
    CHECK(ExternalFfbEnter((HWND)1) == EXT_BUSY && !g_externalMode.load());
    CHECK(input.creates.load() == creates && ffb.creates == 0 && ffb.commands == 0 && ffb.acquires == 0 && ffb.properties == 0);
    CHECK(g_device == nullptr && g_effect == nullptr);
}

static int armed()
{
    FakeInput input; FakeDevice ffb, reader;
    g_di = &input;
    CHECK(TStatus() == "off");
    CHECK(EnableTestInjection(0) == 0 && EnableTestInjection(2) == 0 && TStatus().find("refused: unknown flags") == 0);
    CHECK(g_forceState.load() == 0);   // a refused flag latches nothing
    CHECK(TSubmit(TRaw("button", 1, 1, 200)) == 0);   // not armed yet
    CHECK(EnableTestInjection(1) == 1 && EnableTestInjection(1) == 1 && TStatus().find("armed: no force") == 0);
    forceExportsRefused(input, ffb);

    // Reads still work, through OpenReadDevice's own non-exclusive handle (no force device is open).
    g_auxCount = 1; g_aux[0].guid = kR12; strcpy_s(g_aux[0].name, "Fixture wheel");
    input.next = &reader;
    for (int i = 0; i < 8; ++i) (&reader.state.lX)[i] = 1000 + i;
    for (auto& p : reader.state.rgdwPOV) p = (DWORD)-1;
    CHECK(OpenReadDevice(0) == 0 && !g_read[0].isWheel && g_read[0].dev == &reader);
    TRead r;
    r.pov_(0);
    CHECK(r.ok && r.axes[5] == 1005 && LastReadInjected(0) == 0);   // nothing running: the device's own sample

    // Submission: armed, bounded, printable, parseable, for an open instance.
    std::string why;
    CHECK(TSubmit("inject raw axis 5 dev={00000000-0000-0000-0000-0000000000b2} value=65535 ms=2000", &why) == 0 && why.find("not open") != std::string::npos);
    CHECK(TSubmit("inject raw axis 9 dev=" + std::string(kR12Text) + " value=1 ms=2000", &why) == 0 && !why.empty());
    CHECK(TSubmit(TRaw("axis", 5, 65535, 20)) == 0);   // ms below 50
    CHECK(TSubmit(std::string(600, 'a'), &why) == 0 && why.find("1-512") != std::string::npos);
    CHECK(TSubmit("inject raw button 1 dev=" + std::string(kR12Text) + " value=1 ms=200\xc3\xa9", &why) == 0 && why.find("ASCII") != std::string::npos);
    CHECK(SubmitTestInjection(nullptr, nullptr, 0) == 0);
    char tiny[4]; CHECK(SubmitTestInjection("nonsense", tiny, sizeof tiny) == 0 && strlen(tiny) == 3);   // truncated, NUL-terminated

    // Delivery: exact raw values into the one snapshot, both reader routes.
    CHECK(TSubmit(TRaw("axis", 5, 65535, 2000)) == 1 && TSubmit(TRaw("axis", 2, 0, 2000)) == 1);
    CHECK(TSubmit(TRaw("button", 31, 1, 2000)) == 1 && TSubmit(TRaw("hat", 0, 27000, 2000, "27000")) == 1);
    r.pov_(0);
    CHECK(r.ok && r.axes[5] == 65535 && r.axes[2] == 0 && r.axes[0] == 1000 && r.buttons[31] == 1 && r.pov[0] == 27000);
    CHECK(LastReadInjected(0) == 1 && LastReadInjected(1) == 0 && LastReadInjected(-1) == 0 && LastReadInjected(99) == 0);
    TRead legacy;
    legacy.legacy(0);
    CHECK(legacy.ok && legacy.axes[5] == 65535 && legacy.buttons[31] == 1 && LastReadInjected(0) == 1);
    CHECK(TStatus().find("samples running") != std::string::npos);

    // A failed read: no injected flag, and its running samples are dropped (never revived on reconnect).
    reader.readResult = DIERR_INPUTLOST;
    r.pov_(0);
    CHECK(!r.ok && LastReadInjected(0) == 0);
    reader.readResult = S_OK;
    r.pov_(0);
    CHECK(r.ok && r.axes[5] == 1005 && r.buttons[31] == 0 && r.pov[0] == -1 && LastReadInjected(0) == 0);

    // An unreadable range: the axis is not delivered and does not count as injected.
    reader.rangeFails = true;
    CHECK(TSubmit(TRaw("axis", 5, 65535, 2000)) == 1);
    r.pov_(0);
    CHECK(r.ok && r.axes[5] == 1005 && LastReadInjected(0) == 0);
    reader.rangeFails = false;
    r.pov_(0);
    CHECK(r.ok && r.axes[5] == 65535 && LastReadInjected(0) == 1);

    // Expiry ends a sample; the next read is the device's own and clears the flag.
    CHECK(TSubmit(TRaw("button", 31, 1, 50)) == 1);
    Sleep(80);
    CHECK(g_injections.clearFor(kR12Text) >= 1);   // drop the long axis sample so only expiry is under test below
    CHECK(TSubmit(TRaw("button", 20, 1, 50)) == 1);
    Sleep(80);
    r.pov_(0);
    CHECK(r.ok && r.buttons[20] == 0 && LastReadInjected(0) == 0);

    // Caller extents (Astra 3990): only objects the caller actually gets back count as injected.
    {
        CHECK(TSubmit(TRaw("hat", 0, 9000, 2000, "9000")) == 1);
        TRead legacyHat; legacyHat.legacy(0);
        CHECK(legacyHat.ok && LastReadInjected(0) == 0);   // the legacy reader returns no POV
        TRead withHat; withHat.pov_(0);
        CHECK(withHat.ok && withHat.pov[0] == 9000 && LastReadInjected(0) == 1);   // matched positive
        CHECK(g_injections.clearFor(kR12Text) >= 1);
        CHECK(TSubmit(TRaw("button", 31, 1, 2000)) == 1);
        int axes[8]; unsigned char four[4] = {};
        int pov[4];
        CHECK(ReadDeviceState(0, axes, four, 4) == 1 && LastReadInjected(0) == 0);   // button 31 is beyond the caller's 4
        CHECK(ReadDeviceState(0, axes, nullptr, 0) == 1 && LastReadInjected(0) == 0);
        CHECK(ReadDeviceStateWithPov(0, axes, 8, nullptr, 0, pov, 4) == 1 && LastReadInjected(0) == 0);
        CHECK(TSubmit(TRaw("button", 2, 1, 2000)) == 1);
        CHECK(ReadDeviceState(0, axes, four, 4) == 1 && four[2] == 1 && LastReadInjected(0) == 1);   // matched positive
        CHECK(g_injections.clearFor(kR12Text) >= 1);
    }
    // No-read expiry: a sample that ran out with no read since is not reported as running.
    CHECK(TSubmit(TRaw("button", 3, 1, 50)) == 1 && TStatus().find("samples running") != std::string::npos);
    Sleep(80);
    CHECK(TStatus().find("no sample running") != std::string::npos);
    // FreeDirectInput clears the per-slot flag together with the running samples.
    CHECK(TSubmit(TRaw("button", 3, 1, 2000)) == 1);
    r.pov_(0);
    CHECK(LastReadInjected(0) == 1);
    FreeDirectInput();
    CHECK(LastReadInjected(0) == 0 && !g_injections.any());
    // Same-thread reentry (Astra 3994): the device's GetProperty submits an injection while a read is delivering. No
    // crash; the new sample arrives with the next read.
    {
        static bool s_once;
        s_once = true;
        reader.onRange = [] {
            if (!s_once) return;
            s_once = false;
            char w[64];
            SubmitTestInjection("inject raw axis 2 dev={000000a1-0000-0000-0000-000000000001} value=65535 ms=2000", w, sizeof w);
        };
        CHECK(TSubmit(TRaw("axis", 5, 0, 2000)) == 1);
        r.pov_(0);
        CHECK(r.ok && r.axes[5] == 0 && r.axes[2] == 1002 && !s_once);   // the reentrant sample waits for the next read
        reader.onRange = nullptr;
        r.pov_(0);
        CHECK(r.ok && r.axes[2] == 65535 && r.axes[5] == 0);
        CHECK(g_injections.clearFor(kR12Text) == 2);
    }
    // The shared table alone: a range callback that adds to the same table on this thread does not deadlock.
    {
        dbce::controls::InjectionTable table;
        dbce::controls::InjectCommand c;
        std::string w;
        CHECK(dbce::controls::parseInject(TRaw("axis", 1, 0, 2000), nullptr, c, w) && table.add(c, 1000).empty());
        long ax[8] = {};
        dbce::controls::StateView sv;
        sv.axes = ax;
        std::vector<std::string> lines;
        int calls = 0;
        auto range = [&](int, long& mn, long& mx) {
            dbce::controls::InjectCommand c2;
            std::string w2;
            if (calls++ == 0 && dbce::controls::parseInject(TRaw("axis", 3, 65535, 2000), nullptr, c2, w2)) table.add(c2, 1001);
            mn = 0; mx = 65535;
            return true;
        };
        CHECK(table.apply(kR12Text, sv, 1001, range, lines) == 1 && ax[1] == 0 && ax[3] == 0);
        CHECK(table.apply(kR12Text, sv, 1002, range, lines) == 2 && ax[3] == 65535);
        // A reader with fewer hats than four: a hat beyond them is never delivered or counted.
        unsigned long povs[4] = {0xFFFFFFFFul, 0xFFFFFFFFul, 0xFFFFFFFFul, 0xFFFFFFFFul};
        dbce::controls::StateView hv; hv.povs = povs; hv.povCount = 1;
        CHECK(dbce::controls::parseInject(TRaw("hat", 2, 9000, 2000, "9000"), nullptr, c, w) && table.add(c, 1002).empty());
        CHECK(table.apply(kR12Text, hv, 1003, range, lines) == 0 && povs[2] == 0xFFFFFFFFul);
        hv.povCount = 4;
        CHECK(table.apply(kR12Text, hv, 1004, range, lines) == 1 && povs[2] == 9000);
    }

    // Close drops everything; a reopened slot gets nothing old back.
    CHECK(TSubmit(TRaw("axis", 5, 65535, 5000)) == 1);
    CloseReadDevices();
    CHECK(OpenReadDevice(0) == 0);
    r.pov_(0);
    CHECK(r.ok && r.axes[5] == 1005 && LastReadInjected(0) == 0);

    // The no-force mode outlives Free and Close.
    FreeDirectInput();
    CloseReadDevices();
    CHECK(TStatus().find("armed: no force") == 0 && EnableTestInjection(1) == 1);
    g_di = &input;
    forceExportsRefused(input, ffb);
    CHECK(ReadAuxButtons(nullptr, 0) == 0);   // the auxiliary reader has no injection path at all
    g_di = nullptr;
    std::printf("PASS armed: %d checks (no force touched, exact delivery on both readers, drop on failure/close, expiry, latch persists)\n", g_checks);
    return 0;
}

// A force path used first (here an InitDirectInput that finds no device) latches "force used": the test mode can
// never arm afterwards, and nothing is ever injected.
static int forceFirst()
{
    FakeInput input; g_di = &input;
    CHECK(InitDirectInput(0) == 0);
    CHECK(g_forceState.load() == 1 && EnableTestInjection(1) == 0 && TStatus().find("refused: a force path") == 0);
    CHECK(TSubmit(TRaw("button", 1, 1, 200)) == 0 && LastReadInjected(0) == 0);
    g_di = nullptr;
    std::printf("PASS forcefirst: %d checks\n", g_checks);
    return 0;
}
static int effectFirst()
{
    CHECK(CreatePeriodicEffect(10) == -1);   // no device: fails, but the attempt latches
    CHECK(EnableTestInjection(1) == 0 && TStatus().find("refused: a force path") == 0);
    std::printf("PASS effectfirst: %d checks\n", g_checks);
    return 0;
}
static int externalFirst()
{
    CHECK(ExternalFfbEnter(nullptr) != EXT_OK);   // refused by its own checks, but the attempt latches
    CHECK(EnableTestInjection(1) == 0);
    std::printf("PASS externalfirst: %d checks\n", g_checks);
    return 0;
}
// Enable and a force open at the same time: exactly one wins, and if the test mode won, nothing was opened.
static int race()
{
    FakeInput input; FakeDevice ffb;
    input.next = &ffb;
    g_di = &input;
    g_candidateCount = 1; g_candidates[0].guid = kR12;
    std::atomic<int> go{0};
    int enabled = -1, init = -1;
    std::thread a([&] { while (!go.load()) {} enabled = EnableTestInjection(1); });
    std::thread b([&] { while (!go.load()) {} init = InitDirectInput(0); });
    go.store(1);
    a.join(); b.join();
    if (enabled == 1) CHECK(g_forceState.load() == 2 && init == 0 && ffb.creates == 0 && ffb.commands == 0 && g_device == nullptr);
    else CHECK(g_forceState.load() == 1 && enabled == 0);
    FreeDirectInput();
    g_di = nullptr;
    std::printf("PASS race: %d checks (%s won)\n", g_checks, enabled == 1 ? "the test mode" : "the force open");
    return 0;
}

int main(int argc, char** argv)
{
    SetEnvironmentVariableA("DBCE_FFB_LOG", "0");
    const std::string s = argc > 1 ? argv[1] : "";
    if (s == "armed") return armed();
    if (s == "forcefirst") return forceFirst();
    if (s == "effectfirst") return effectFirst();
    if (s == "externalfirst") return externalFirst();
    if (s == "race") return race();
    std::fprintf(stderr, "usage: injection_test armed|forcefirst|effectfirst|externalfirst|race\n");
    return 2;
}
