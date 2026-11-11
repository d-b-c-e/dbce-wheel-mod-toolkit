// The muted DirectInput: an unattended-test switch for a game that reads its pad through DirectInput, on the proxy core
// (dbce_proxy_core.hpp). C++17, header-only. Moved from DiRT 4 (AD-5); DiRT Rally 2.0, GRID (2019) and V-Rally 4 carry
// the same code.
//
// Switch: %LOCALAPPDATA%\dbce\<id>\ffb-mute.on, or DBCE_<ID>_FFB_MUTE=1 in the environment, at process attach. The game
// then gets a DirectInput that shows only Xbox 360 pads without a force-feedback driver (the toolkit virtual pad and
// real 360 pads), each behind an output fence, plus the system keyboard and mouse; it cannot open the wheel, start a
// force or change auto-centring. A refusal can crash a game's cleanup, so DirectInput8Create still succeeds.
//
//   BOOL WINAPI DllMain(...) { ... dbce::proxy::attach(kMod, started, dbce::proxy::muted::requested(kMod.id)); ... }
//   extern "C" HRESULT WINAPI DirectInput8Create(HINSTANCE h, DWORD v, REFIID riid, LPVOID *out, LPUNKNOWN unk) {
//       dbce::proxy::start(kMod, started);
//       if (dbce::proxy::state().mute) return dbce::proxy::muted::create(h, v, riid, out, unk, false);
//       ...
#pragma once
#include "dbce_proxy_core.hpp"
#include <new>

namespace dbce { namespace proxy { namespace muted {

// The switch (kernel32 only, safe in DllMain): DBCE_<ID>_FFB_MUTE=1, or ffb-mute.on in the log folder.
inline bool requested(const char *id)
{
    char name[64], env[8];
    sprintf_s(name, "DBCE_%s_FFB_MUTE", id);
    _strupr_s(name);
    DWORD e = GetEnvironmentVariableA(name, env, sizeof env);
    if (e == 1 && env[0] == '1') return true;
    return dbce::testing::switchFile(id, "ffb-mute.on");
}

inline volatile LONG g_refusals;
inline void logRefusal(const char *what) { LONG k = InterlockedIncrement(&g_refusals); if (k <= 10) log("ffb mute: %s refused (#%ld)", what, k); }

inline const GUID kIidUnknown = {0x00000000, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
inline const GUID kIidDirectInput8A = {0xBF798030, 0x483A, 0x4DA2, {0xAA, 0x99, 0x5D, 0x64, 0xED, 0x36, 0x97, 0x00}};
inline const GUID kIidDirectInputDevice8A = {0x54D41080, 0xDC15, 0x4833, {0xA4, 0x1B, 0x74, 0x8F, 0x73, 0xA3, 0x81, 0x79}};
inline const GUID kIidDirectInput8W = {0xBF798031, 0x483A, 0x4DA2, {0xAA, 0x99, 0x5D, 0x64, 0xED, 0x36, 0x97, 0x00}};
inline const GUID kIidDirectInputDevice8W = {0x54D41081, 0xDC15, 0x4833, {0xA4, 0x1B, 0x74, 0x8F, 0x73, 0xA3, 0x81, 0x79}};
inline const GUID kGuidSysMouse = {0x6F1D2B60, 0xD5A0, 0x11CF, {0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
inline const GUID kGuidSysKeyboard = {0x6F1D2B61, 0xD5A0, 0x11CF, {0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};

// IDirectInput8A/W's method order (dinput.h; the two differ only in string and structure types, all passed as
// pointers), as plain virtuals for the wrapper.
struct DirectInput8 {
    virtual HRESULT __stdcall QueryInterface(REFIID, void **) = 0;
    virtual ULONG __stdcall AddRef() = 0;
    virtual ULONG __stdcall Release() = 0;
    virtual HRESULT __stdcall CreateDevice(REFGUID, void **, IUnknown *) = 0;
    virtual HRESULT __stdcall EnumDevices(DWORD, void *, void *, DWORD) = 0;
    virtual HRESULT __stdcall GetDeviceStatus(REFGUID) = 0;
    virtual HRESULT __stdcall RunControlPanel(HWND, DWORD) = 0;
    virtual HRESULT __stdcall Initialize(HINSTANCE, DWORD) = 0;
    virtual HRESULT __stdcall FindDevice(REFGUID, const void *, GUID *) = 0;
    virtual HRESULT __stdcall EnumDevicesBySemantics(const void *, void *, void *, void *, DWORD) = 0;
    virtual HRESULT __stdcall ConfigureDevices(void *, void *, DWORD, void *) = 0;
};
inline const HRESULT kDeviceNotRegistered = REGDB_E_CLASSNOTREG;   // DIERR_DEVICENOTREG
inline const HRESULT kUnsupported = E_NOTIMPL;                      // DIERR_UNSUPPORTED
inline const GUID kNoGuid = {};
inline const DWORD kXbox360Product = 0x028E045E;                    // guidProduct.Data1 = MAKELONG(VID 045E, PID 028E)

// The narrow (A) and wide (W) DirectInput flavours, as in the GRID (2019) mod 0.1.1: GRID asks for IDirectInput8W (its
// A-only mute refused it and the game closed).
struct NarrowDi {
    using Dev = IDirectInputDevice8A; using Inst = DIDEVICEINSTANCEA; using CInst = LPCDIDEVICEINSTANCEA;
    using EnumCb = LPDIENUMDEVICESCALLBACKA; using ObjCb = LPDIENUMDEVICEOBJECTSCALLBACKA; using ObjInst = LPDIDEVICEOBJECTINSTANCEA;
    using DevInst = LPDIDEVICEINSTANCEA; using EffCb = LPDIENUMEFFECTSCALLBACKA; using EffInfo = LPDIEFFECTINFOA; using Str = LPCSTR;
    using ActFmt = LPDIACTIONFORMATA; using ImgInfo = LPDIDEVICEIMAGEINFOHEADERA;
    static const GUID &iidDi() { return kIidDirectInput8A; }
    static const GUID &iidDev() { return kIidDirectInputDevice8A; }
    static void logPad(const Inst &i) { log("ffb mute: pad %s created behind the output fence", i.tszInstanceName); }
};
struct WideDi {
    using Dev = IDirectInputDevice8W; using Inst = DIDEVICEINSTANCEW; using CInst = LPCDIDEVICEINSTANCEW;
    using EnumCb = LPDIENUMDEVICESCALLBACKW; using ObjCb = LPDIENUMDEVICEOBJECTSCALLBACKW; using ObjInst = LPDIDEVICEOBJECTINSTANCEW;
    using DevInst = LPDIDEVICEINSTANCEW; using EffCb = LPDIENUMEFFECTSCALLBACKW; using EffInfo = LPDIEFFECTINFOW; using Str = LPCWSTR;
    using ActFmt = LPDIACTIONFORMATW; using ImgInfo = LPDIDEVICEIMAGEINFOHEADERW;
    static const GUID &iidDi() { return kIidDirectInput8W; }
    static const GUID &iidDev() { return kIidDirectInputDevice8W; }
    static void logPad(const Inst &i) { log("ffb mute: pad %ls created behind the output fence", i.tszInstanceName); }
};

// The created pad's output fence (Astra's review of 7081be8): admission by product and a missing force driver is not
// the output gate. The game gets this wrapper, never the real device: every force, escape and raw-output route is
// refused, auto-centre and force-gain properties cannot be set, and QueryInterface hands out only this wrapper.
// Reading (data format, cooperative level, acquire, state, data, objects, info, properties) is forwarded unchanged.
template <class T> struct MutedPad : T::Dev {
    typename T::Dev *real;
    volatile LONG refs = 1;
    explicit MutedPad(typename T::Dev *r) : real(r) {}
    static HRESULT refuse(const char *what) { logRefusal(what); return DIERR_UNSUPPORTED; }
    STDMETHODIMP QueryInterface(REFIID iid, LPVOID *out) override
    {
        if (!out) return E_POINTER;
        if (IsEqualGUID(iid, kIidUnknown) || IsEqualGUID(iid, T::iidDev())) { AddRef(); *out = this; return S_OK; }
        *out = nullptr;
        logRefusal("pad QueryInterface for another interface");
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return (ULONG)InterlockedIncrement(&refs); }
    STDMETHODIMP_(ULONG) Release() override
    {
        LONG n = InterlockedDecrement(&refs);
        if (n == 0) { real->Release(); delete this; }
        return (ULONG)n;
    }
    STDMETHODIMP GetCapabilities(LPDIDEVCAPS c) override
    {
        HRESULT hr = real->GetCapabilities(c);
        if (SUCCEEDED(hr) && c) c->dwFlags &= ~DWORD(DIDC_FORCEFEEDBACK | DIDC_FFATTACK | DIDC_FFFADE | DIDC_SATURATION |
                                                    DIDC_POSNEGCOEFFICIENTS | DIDC_POSNEGSATURATION | DIDC_DEADBAND | DIDC_STARTDELAY);
        return hr;
    }
    STDMETHODIMP EnumObjects(typename T::ObjCb cb, LPVOID ref, DWORD flags) override
    {
        if (flags & (DIDFT_FFACTUATOR | DIDFT_FFEFFECTTRIGGER | DIDFT_OUTPUT)) return S_OK;   // no force or output objects
        return real->EnumObjects(cb, ref, flags);
    }
    STDMETHODIMP GetProperty(REFGUID p, LPDIPROPHEADER h) override { return real->GetProperty(p, h); }
    STDMETHODIMP SetProperty(REFGUID p, LPCDIPROPHEADER h) override
    {
        if (&p == &DIPROP_AUTOCENTER || &p == &DIPROP_FFGAIN) return refuse("pad SetProperty (auto-centre or force gain)");
        return real->SetProperty(p, h);
    }
    STDMETHODIMP Acquire() override { return real->Acquire(); }
    STDMETHODIMP Unacquire() override { return real->Unacquire(); }
    STDMETHODIMP GetDeviceState(DWORD n, LPVOID d) override { return real->GetDeviceState(n, d); }
    STDMETHODIMP GetDeviceData(DWORD n, LPDIDEVICEOBJECTDATA d, LPDWORD k, DWORD f) override { return real->GetDeviceData(n, d, k, f); }
    STDMETHODIMP SetDataFormat(LPCDIDATAFORMAT f) override { return real->SetDataFormat(f); }
    STDMETHODIMP SetEventNotification(HANDLE e) override { return real->SetEventNotification(e); }
    STDMETHODIMP SetCooperativeLevel(HWND w, DWORD f) override { return real->SetCooperativeLevel(w, f); }
    STDMETHODIMP GetObjectInfo(typename T::ObjInst o, DWORD obj, DWORD how) override { return real->GetObjectInfo(o, obj, how); }
    STDMETHODIMP GetDeviceInfo(typename T::DevInst d) override { return real->GetDeviceInfo(d); }
    STDMETHODIMP RunControlPanel(HWND, DWORD) override { return refuse("pad RunControlPanel"); }
    STDMETHODIMP Initialize(HINSTANCE, DWORD, REFGUID) override { return refuse("pad Initialize (re-targeting the device)"); }
    STDMETHODIMP CreateEffect(REFGUID, LPCDIEFFECT, LPDIRECTINPUTEFFECT *e, LPUNKNOWN) override
    {
        if (e) *e = nullptr;
        return refuse("pad CreateEffect");
    }
    STDMETHODIMP EnumEffects(typename T::EffCb, LPVOID, DWORD) override { return S_OK; }   // none
    STDMETHODIMP GetEffectInfo(typename T::EffInfo, REFGUID) override { return refuse("pad GetEffectInfo"); }
    STDMETHODIMP GetForceFeedbackState(LPDWORD) override { return refuse("pad GetForceFeedbackState"); }
    STDMETHODIMP SendForceFeedbackCommand(DWORD) override { return refuse("pad SendForceFeedbackCommand"); }
    STDMETHODIMP EnumCreatedEffectObjects(LPDIENUMCREATEDEFFECTOBJECTSCALLBACK, LPVOID, DWORD) override { return S_OK; }   // none
    STDMETHODIMP Escape(LPDIEFFESCAPE) override { return refuse("pad Escape"); }
    STDMETHODIMP Poll() override { return real->Poll(); }
    STDMETHODIMP SendDeviceData(DWORD, LPCDIDEVICEOBJECTDATA, LPDWORD, DWORD) override { return refuse("pad SendDeviceData"); }
    STDMETHODIMP EnumEffectsInFile(typename T::Str, LPDIENUMEFFECTSINFILECALLBACK, LPVOID, DWORD) override { return refuse("pad EnumEffectsInFile"); }
    STDMETHODIMP WriteEffectToFile(typename T::Str, DWORD, LPDIFILEEFFECT, DWORD) override { return refuse("pad WriteEffectToFile"); }
    STDMETHODIMP BuildActionMap(typename T::ActFmt f, typename T::Str u, DWORD fl) override { return real->BuildActionMap(f, u, fl); }
    STDMETHODIMP SetActionMap(typename T::ActFmt f, typename T::Str u, DWORD fl) override { return real->SetActionMap(f, u, fl); }
    STDMETHODIMP GetImageInfo(typename T::ImgInfo h) override { return real->GetImageInfo(h); }
};

template <class T> struct MutedDirectInput : DirectInput8 {
    DirectInput8 *real;
    volatile LONG refs = 1;
    explicit MutedDirectInput(DirectInput8 *r) : real(r) {}
    HRESULT __stdcall QueryInterface(REFIID iid, void **out) override
    {
        if (!out) return E_POINTER;
        if (IsEqualGUID(iid, kIidUnknown) || IsEqualGUID(iid, T::iidDi())) { AddRef(); *out = this; return S_OK; }
        *out = nullptr;
        logRefusal("QueryInterface for another interface");
        return E_NOINTERFACE;
    }
    ULONG __stdcall AddRef() override { return (ULONG)InterlockedIncrement(&refs); }
    ULONG __stdcall Release() override
    {
        LONG n = InterlockedDecrement(&refs);
        if (n == 0) { real->Release(); delete this; }
        return (ULONG)n;
    }
    HRESULT __stdcall CreateDevice(REFGUID g, void **out, IUnknown *outer) override
    {
        if (!out) return E_POINTER;
        *out = nullptr;
        if (outer) { logRefusal("CreateDevice with aggregation"); return kDeviceNotRegistered; }
        if (IsEqualGUID(g, kGuidSysKeyboard) || IsEqualGUID(g, kGuidSysMouse)) return real->CreateDevice(g, out, nullptr);
        if (!allowedPad(g)) { logRefusal("CreateDevice (not the system keyboard, mouse or a no-force pad)"); return kDeviceNotRegistered; }
        // Revalidate the live device: still a 360-class pad without a force driver and without force capabilities.
        typename T::Dev *dev = nullptr;
        HRESULT hr = real->CreateDevice(g, (void **)&dev, nullptr);
        if (FAILED(hr) || !dev) return FAILED(hr) ? hr : E_FAIL;
        typename T::Inst info{}; info.dwSize = sizeof info;
        DIDEVCAPS caps{}; caps.dwSize = sizeof caps;
        if (FAILED(dev->GetDeviceInfo(&info)) || info.guidProduct.Data1 != kXbox360Product || !IsEqualGUID(info.guidFFDriver, kNoGuid) ||
            FAILED(dev->GetCapabilities(&caps)) || (caps.dwFlags & DIDC_FORCEFEEDBACK)) {
            dev->Release();
            logRefusal("CreateDevice (the created pad failed revalidation)");
            return kDeviceNotRegistered;
        }
        auto *pad = new (std::nothrow) MutedPad<T>(dev);
        if (!pad) { dev->Release(); return E_OUTOFMEMORY; }
        *out = static_cast<typename T::Dev *>(pad);
        static volatile LONG made;
        if (InterlockedIncrement(&made) <= 3) T::logPad(info);
        return hr;
    }
    // Only Xbox-360-class pads (VID 045E, PID 028E: the toolkit virtual pad and real 360 pads) whose instance has no
    // force-feedback driver are passed through; any device with a force driver stays hidden.
    struct EnumCtx { typename T::EnumCb cb; void *ref; MutedDirectInput *self; int passed; };
    static BOOL CALLBACK filterPads(typename T::CInst d, LPVOID p)
    {
        auto *c = (EnumCtx *)p;
        if (!d || d->guidProduct.Data1 != kXbox360Product || !IsEqualGUID(d->guidFFDriver, kNoGuid)) return DIENUM_CONTINUE;
        c->self->rememberPad(d->guidInstance);
        ++c->passed;
        return c->cb(d, c->ref);
    }
    HRESULT __stdcall EnumDevices(DWORD type, void *callback, void *ref, DWORD flags) override
    {
        static volatile LONG logged;
        if (!callback) return E_INVALIDARG;
        if (flags & DIEDFL_FORCEFEEDBACK) {
            if (InterlockedIncrement(&logged) <= 5) log("ffb mute: EnumDevices for force devices answered with none");
            return S_OK;
        }
        EnumCtx c{(typename T::EnumCb)callback, ref, this, 0};
        HRESULT hr = real->EnumDevices(type, (void *)&filterPads, &c, flags);
        if (InterlockedIncrement(&logged) <= 5) log("ffb mute: EnumDevices passed %d no-force pad(s), nothing else", c.passed);
        return hr;
    }
    HRESULT __stdcall GetDeviceStatus(REFGUID g) override
    {
        return IsEqualGUID(g, kGuidSysKeyboard) || IsEqualGUID(g, kGuidSysMouse) || allowedPad(g) ? real->GetDeviceStatus(g) : kDeviceNotRegistered;
    }
    GUID pads[8] = {};
    int padCount = 0;
    void rememberPad(const GUID &g)
    {
        EnterCriticalSection(&g_lock);
        bool known = false;
        for (int i = 0; i < padCount; ++i) known |= IsEqualGUID(pads[i], g) != 0;
        if (!known && padCount < 8) pads[padCount++] = g;
        LeaveCriticalSection(&g_lock);
    }
    bool allowedPad(const GUID &g)
    {
        EnterCriticalSection(&g_lock);
        bool ok = false;
        for (int i = 0; i < padCount; ++i) ok |= IsEqualGUID(pads[i], g) != 0;
        LeaveCriticalSection(&g_lock);
        return ok;
    }
    HRESULT __stdcall RunControlPanel(HWND, DWORD) override { logRefusal("RunControlPanel"); return kUnsupported; }
    HRESULT __stdcall Initialize(HINSTANCE h, DWORD v) override { return real->Initialize(h, v); }
    HRESULT __stdcall FindDevice(REFGUID, const void *, GUID *) override { logRefusal("FindDevice"); return kDeviceNotRegistered; }
    HRESULT __stdcall EnumDevicesBySemantics(const void *, void *, void *callback, void *, DWORD) override
    {
        logRefusal("EnumDevicesBySemantics (answered with no devices)");
        return callback ? S_OK : E_INVALIDARG;
    }
    HRESULT __stdcall ConfigureDevices(void *, void *, DWORD, void *) override { logRefusal("ConfigureDevices"); return kUnsupported; }
};

// DirectInput8Create while muted (call start() first). `allowWide`: also serve IDirectInput8W, for a game that asks for
// it; otherwise only IDirectInput8A is served. Aggregation and any other interface are refused, and so is everything
// without the pin (the wrapper's code must stay mapped for the rest of the process).
inline HRESULT create(HINSTANCE h, DWORD ver, REFIID riid, LPVOID *out, LPUNKNOWN unk, bool allowWide)
{
    const State &s = state();
    if (!out) return E_POINTER;
    *out = nullptr;
    if (!s.pinned) { log("DirectInput8Create refused: force feedback mute requested but the proxy could not be pinned"); return E_FAIL; }
    const bool wide = allowWide && IsEqualGUID(riid, kIidDirectInput8W) != 0;
    if (unk || (!wide && !IsEqualGUID(riid, kIidDirectInput8A))) { logRefusal("DirectInput8Create (aggregation or another interface)"); return E_NOINTERFACE; }
    auto f = s.real ? (dbce::testing::DI8CreateFn)GetProcAddress(s.real, "DirectInput8Create") : nullptr;
    DirectInput8 *real = nullptr;
    HRESULT hr = f ? f(h, ver, wide ? kIidDirectInput8W : kIidDirectInput8A, (LPVOID *)&real, nullptr) : E_FAIL;
    if (FAILED(hr) || !real) return FAILED(hr) ? hr : E_FAIL;
    DirectInput8 *muted = wide ? static_cast<DirectInput8 *>(new (std::nothrow) MutedDirectInput<WideDi>(real))
                               : static_cast<DirectInput8 *>(new (std::nothrow) MutedDirectInput<NarrowDi>(real));
    if (!muted) { real->Release(); return E_OUTOFMEMORY; }
    *out = muted;
    static volatile LONG created;
    if (InterlockedIncrement(&created) <= 3) log("ffb mute: DirectInput8Create (%s) gave the game the muted interface", wide ? "8W" : "8A");
    return hr;
}

}}} // namespace dbce::proxy::muted
