// GetReadSlotCaps (native 0.11.0) against fake COM devices: the identity, objects, ranges and interface path read back
// from the slot's own handle (the shared FFB handle for the wheel's slot), the slot rules of the reads, and no state
// change. No system DirectInput object, window, game or hardware is opened.
#include "../WheelFfb.cpp"
#include <cassert>
#include <set>
#include <string>

struct FakeDevice : IDirectInputDevice8A {
    DIDEVICEINSTANCEA info = {};
    HRESULT infoResult = S_OK, pathResult = S_OK;
    std::set<DWORD> objects;                      // offsets GetObjectInfo knows
    struct Range { LONG min, max; HRESULT hr; };
    Range ranges[8] = {};                         // by axis index, for DIPROP_RANGE by offset
    std::wstring path;
    int acquires = 0, unacquires = 0, sets = 0, releases = 0, infos = 0;
    static int axisIndex(DWORD offset) {
        static const DWORD axes[8] = {DIJOFS_X, DIJOFS_Y, DIJOFS_Z, DIJOFS_RX, DIJOFS_RY, DIJOFS_RZ, DIJOFS_SLIDER(0), DIJOFS_SLIDER(1)};
        for (int i = 0; i < 8; ++i) if (axes[i] == offset) return i;
        return -1;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**) override {return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef() override {return 1;}
    ULONG STDMETHODCALLTYPE Release() override {++releases;return 1;}
    HRESULT STDMETHODCALLTYPE GetCapabilities(LPDIDEVCAPS) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE EnumObjects(LPDIENUMDEVICEOBJECTSCALLBACKA,LPVOID,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetProperty(REFGUID prop,LPDIPROPHEADER h) override {
        if (&prop == &DIPROP_RANGE) {
            assert(h->dwSize == sizeof(DIPROPRANGE) && h->dwHow == DIPH_BYOFFSET);
            const int a = axisIndex(h->dwObj);
            if (a < 0 || FAILED(ranges[a].hr)) return a < 0 ? DIERR_OBJECTNOTFOUND : ranges[a].hr;
            auto* r = reinterpret_cast<DIPROPRANGE*>(h); r->lMin = ranges[a].min; r->lMax = ranges[a].max; return S_OK;
        }
        if (&prop == &DIPROP_GUIDANDPATH) {
            assert(h->dwSize == sizeof(DIPROPGUIDANDPATH) && h->dwHow == DIPH_DEVICE);
            if (FAILED(pathResult)) return pathResult;
            auto* p = reinterpret_cast<DIPROPGUIDANDPATH*>(h);
            const size_t n = path.size() < MAX_PATH ? path.size() : MAX_PATH;
            memcpy(p->wszPath, path.data(), n * sizeof(wchar_t));
            if (n < MAX_PATH) p->wszPath[n] = 0;
            return S_OK;
        }
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE SetProperty(REFGUID,LPCDIPROPHEADER) override {++sets;return S_OK;}
    HRESULT STDMETHODCALLTYPE Acquire() override {++acquires;return S_OK;}
    HRESULT STDMETHODCALLTYPE Unacquire() override {++unacquires;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetDeviceState(DWORD,LPVOID) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetDeviceData(DWORD,LPDIDEVICEOBJECTDATA,LPDWORD,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetDataFormat(LPCDIDATAFORMAT) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetEventNotification(HANDLE) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetCooperativeLevel(HWND,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetObjectInfo(LPDIDEVICEOBJECTINSTANCEA oi,DWORD offset,DWORD how) override {
        assert(how == DIPH_BYOFFSET && oi->dwSize == sizeof(*oi));
        return objects.count(offset) ? S_OK : DIERR_OBJECTNOTFOUND;
    }
    HRESULT STDMETHODCALLTYPE GetDeviceInfo(LPDIDEVICEINSTANCEA out) override {
        assert(out->dwSize == sizeof(*out));++infos;
        if (FAILED(infoResult)) return infoResult;
        *out = info; out->dwSize = sizeof(*out); return S_OK;
    }
    HRESULT STDMETHODCALLTYPE RunControlPanel(HWND,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE Initialize(HINSTANCE,DWORD,REFGUID) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE CreateEffect(REFGUID,LPCDIEFFECT,LPDIRECTINPUTEFFECT*,LPUNKNOWN) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE EnumEffects(LPDIENUMEFFECTSCALLBACKA,LPVOID,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetEffectInfo(LPDIEFFECTINFOA,REFGUID) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetForceFeedbackState(LPDWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SendForceFeedbackCommand(DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE EnumCreatedEffectObjects(LPDIENUMCREATEDEFFECTOBJECTSCALLBACK,LPVOID,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE Escape(LPDIEFFESCAPE) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE Poll() override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SendDeviceData(DWORD,LPCDIDEVICEOBJECTDATA,LPDWORD,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE EnumEffectsInFile(LPCSTR,LPDIENUMEFFECTSINFILECALLBACK,LPVOID,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE WriteEffectToFile(LPCSTR,DWORD,LPDIFILEEFFECT,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE BuildActionMap(LPDIACTIONFORMATA,LPCSTR,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetActionMap(LPDIACTIONFORMATA,LPCSTR,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetImageInfo(LPDIDEVICEIMAGEINFOHEADERA) override {return E_NOTIMPL;}
};

static GUID guid(unsigned long d1, unsigned short d2) {GUID g = {};g.Data1 = d1;g.Data2 = d2;g.Data4[7] = 0x44;return g;}
static ReadSlotCapsV1 fresh() {ReadSlotCapsV1 c;memset(&c, 0xAB, sizeof c);c.cbSize = sizeof c;return c;}
static void cleared(const ReadSlotCapsV1& c) {
    const unsigned char* b = reinterpret_cast<const unsigned char*>(&c);
    for (size_t i = sizeof(uint32_t); i < sizeof c; ++i) assert(b[i] == 0);
    assert(c.cbSize == sizeof c);
}

int main() {
    SetEnvironmentVariableA("DBCE_FFB_LOG", "0");
    FakeDevice wheel;
    wheel.info.guidInstance = guid(0xd71b8350, 0x61b7);wheel.info.guidProduct = guid(0x0006346e, 0);
    // X and Rz with ranges, Y present but its range refused, Z present with an empty range; no sliders.
    wheel.objects = {DIJOFS_X, DIJOFS_Y, DIJOFS_Z, DIJOFS_RZ, DIJOFS_POV(0)};
    for (int b = 0; b < 24; ++b) wheel.objects.insert(DIJOFS_BUTTON(b));
    wheel.objects.insert(DIJOFS_BUTTON(127));
    wheel.ranges[0] = {0, 65535, S_OK};wheel.ranges[1] = {0, 0, DIERR_UNSUPPORTED};
    wheel.ranges[2] = {100, 100, S_OK};wheel.ranges[5] = {-32768, 32767, S_OK};
    wheel.path = L"\\\\?\\hid#vid_346e&pid_0006#\x00fc\x65e5";   // non-ASCII: exact UTF-8

    // The force-feedback wheel's slot reads its caps through the shared exclusive handle: no second device.
    g_device = &wheel;g_activeGuid = wheel.info.guidInstance;
    g_auxCount = 1;g_aux[0].guid = g_activeGuid;
    assert(OpenReadDevice(0) == 0 && g_read[0].isWheel && !g_read[0].dev);
    ReadSlotCapsV1 c = fresh();
    assert(GetReadSlotCaps(0, &c) == 1 && c.cbSize == sizeof c && c.version == 1);
    assert(!memcmp(c.instanceGuid, &wheel.info.guidInstance, 16) && !memcmp(c.productGuid, &wheel.info.guidProduct, 16));
    const int present[8] = {1, 1, 1, 0, 0, 1, 0, 0}, ok[8] = {1, 0, 0, 0, 0, 1, 0, 0};
    for (int a = 0; a < 8; ++a) assert(c.axisPresent[a] == present[a] && c.rangeOk[a] == ok[a]);
    assert(c.rangeMin[0] == 0 && c.rangeMax[0] == 65535 && c.rangeMin[5] == -32768 && c.rangeMax[5] == 32767);
    assert(c.rangeMin[1] == 0 && c.rangeMax[1] == 0 && c.rangeMin[2] == 0 && c.rangeMax[2] == 0);   // unqualified: nothing copied
    for (int b = 0; b < 128; ++b) assert(c.buttonPresent[b] == ((b < 24 || b == 127) ? 1 : 0));
    assert(c.povPresent[0] == 1 && !c.povPresent[1] && !c.povPresent[2] && !c.povPresent[3]);
    assert(std::string(c.interfacePath) == "\\\\?\\hid#vid_346e&pid_0006#\xc3\xbc\xe6\x97\xa5");
    assert(wheel.acquires == 0 && wheel.unacquires == 0 && wheel.sets == 0 && wheel.releases == 0);   // nothing changed

    // The interface path is optional and exact: unavailable, invalid UTF-16 or too long for the field leaves it empty.
    wheel.pathResult = DIERR_UNSUPPORTED;c = fresh();
    assert(GetReadSlotCaps(0, &c) == 1 && c.interfacePath[0] == 0 && c.axisPresent[0] == 1);
    wheel.pathResult = S_OK;wheel.path = std::wstring(L"\\\\?\\hid#") + L'\xD800';c = fresh();
    assert(GetReadSlotCaps(0, &c) == 1 && c.interfacePath[0] == 0);
    wheel.path = std::wstring(259, L'\x65e5');c = fresh();   // 777 bytes: fits
    assert(GetReadSlotCaps(0, &c) == 1 && strlen(c.interfacePath) == 777);
    wheel.path = std::wstring(MAX_PATH, L'a');c = fresh();   // no terminator within MAX_PATH
    assert(GetReadSlotCaps(0, &c) == 1 && c.interfacePath[0] == 0);
    wheel.path = L"\\\\?\\hid#1";

    // Wrong cbSize or no struct: nothing is touched.
    ReadSlotCapsV1 wrong;memset(&wrong, 0xAB, sizeof wrong);wrong.cbSize = sizeof wrong - 1;
    assert(GetReadSlotCaps(0, &wrong) == 0 && wrong.cbSize == sizeof wrong - 1 && wrong.version == 0xABABABABu);
    assert(GetReadSlotCaps(0, nullptr) == 0);
    // Slots out of range: cleared, refused.
    for (int slot : {-1, 1, 8, INT_MAX}) {c = fresh();assert(GetReadSlotCaps(slot, &c) == 0);cleared(c);}
    // The identity read failing refuses the whole answer.
    wheel.infoResult = DIERR_INPUTLOST;c = fresh();
    assert(GetReadSlotCaps(0, &c) == 0);cleared(c);
    wheel.infoResult = S_OK;

    // The wheel's slot never follows another FFB device, nor a released one; the same GUID resumes (the reads' rule).
    FakeDevice other;other.info.guidInstance = guid(1, 1);
    g_device = &other;g_activeGuid = other.info.guidInstance;c = fresh();
    assert(GetReadSlotCaps(0, &c) == 0 && other.infos == 0);cleared(c);
    g_device = nullptr;c = fresh();
    assert(GetReadSlotCaps(0, &c) == 0);cleared(c);
    g_device = &wheel;g_activeGuid = wheel.info.guidInstance;c = fresh();
    assert(GetReadSlotCaps(0, &c) == 1);
    CloseReadDevices();c = fresh();
    assert(GetReadSlotCaps(0, &c) == 0);cleared(c);
    assert(!wheel.releases && !wheel.unacquires);

    // An independent reader's slot reports its own handle.
    FakeDevice reader;reader.info.guidInstance = guid(0xd9b1d890, 0x6f3f);reader.info.guidProduct = guid(0x0002f3a1, 0);
    reader.objects = {DIJOFS_BUTTON(0), DIJOFS_BUTTON(1)};
    g_readCount = 1;g_read[0].isWheel = false;g_read[0].dev = &reader;c = fresh();
    assert(GetReadSlotCaps(0, &c) == 1 && !memcmp(c.instanceGuid, &reader.info.guidInstance, 16));
    for (int a = 0; a < 8; ++a) assert(!c.axisPresent[a] && !c.rangeOk[a]);
    assert(c.buttonPresent[0] && c.buttonPresent[1] && !c.buttonPresent[2] && c.interfacePath[0] == 0);
    assert(reader.acquires == 0 && reader.sets == 0);
    CloseReadDevices();
    g_device = nullptr;g_auxCount = 0;
    puts("PASS read slot caps: identity, objects, ranges and path from the slot's own handle; slot rules; no state change");
}
