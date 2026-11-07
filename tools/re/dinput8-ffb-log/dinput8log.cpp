// Read-only, muted DirectInput logger: installed as dinput8.dll next to a game exe (x64 or x86). Every export is
// forwarded to the system dinput8.dll. It logs the devices the game enumerates and opens, the properties and
// cooperative level it sets, and every force-feedback effect it creates and updates.
//
// Muted: IDirectInputEffect::Start is logged and NOT forwarded, and DIEP_START is stripped from SetParameters, so no
// effect ever plays on the hardware. Parameter updates are summarised once per second per effect (count, magnitude
// range, last value) after the first 20 are logged in full.
// Log: %LOCALAPPDATA%\dbce\dinput8log\dinput8log-<exe>-<pid>.txt.
#define DIRECTINPUT_VERSION 0x0800
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <dinput.h>
#include <stdio.h>
#include <stdarg.h>
#include <share.h>
#include <intrin.h>

#pragma comment(lib, "dxguid.lib")

static HMODULE g_real;
static CRITICAL_SECTION g_cs;
static FILE *g_log;
static volatile LONG g_inited;
static DWORD g_t0;
// Optional research switch: when dinput8log-retype.txt sits next to the game exe, devices that report themselves as
// DI8DEVTYPE_1STPERSON (a MOZA direct-drive base does) are presented as DI8DEVTYPE_DRIVING in enumeration,
// GetDeviceInfo and GetCapabilities (as Gravel's mod does), to test whether a game then accepts the wheel.
static bool g_retype;
// Optional research switch: when dinput8log-retype.txt sits next to the game exe, devices that report themselves as
// DI8DEVTYPE_1STPERSON (a MOZA direct-drive base does) are presented as DI8DEVTYPE_DRIVING in enumeration,
// GetDeviceInfo and GetCapabilities (as Gravel's mod does), to test whether a game then accepts the wheel. bool g_retype;

static void dlog(const char *fmt, ...)
{
    if (!g_log) return;
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf_s(buf, sizeof buf, _TRUNCATE, fmt, ap);
    va_end(ap);
    EnterCriticalSection(&g_cs);
    fprintf(g_log, "%9.3f %s\n", (GetTickCount() - g_t0) / 1000.0, buf);
    fflush(g_log);
    LeaveCriticalSection(&g_cs);
}

// True when the call comes from DirectInput itself or another system DLL (its own enumeration), not the game.
static bool fromSystem(void *addr)
{
    HMODULE mod = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)addr, &mod) || !mod) return false;
    if (mod == g_real) return true;
    wchar_t path[MAX_PATH], sys[MAX_PATH];
    GetModuleFileNameW(mod, path, MAX_PATH);
    UINT n = GetSystemDirectoryW(sys, MAX_PATH);
    return n && _wcsnicmp(path, sys, n) == 0;
}

static void where(void *addr, char *out, size_t cap)
{
    HMODULE mod = nullptr;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)addr, &mod) && mod) {
        char path[MAX_PATH];
        DWORD n = GetModuleFileNameA(mod, path, MAX_PATH);
        const char *base = path;
        for (DWORD i = 0; i < n; i++) if (path[i] == '\\') base = path + i + 1;
        sprintf_s(out, cap, "%s+0x%llx", base, (unsigned long long)((char *)addr - (char *)mod));
    } else {
        sprintf_s(out, cap, "%p", addr);
    }
}

static const char *effectName(REFGUID g)
{
    if (g == GUID_ConstantForce) return "ConstantForce";
    if (g == GUID_RampForce) return "RampForce";
    if (g == GUID_Square) return "Square";
    if (g == GUID_Sine) return "Sine";
    if (g == GUID_Triangle) return "Triangle";
    if (g == GUID_SawtoothUp) return "SawtoothUp";
    if (g == GUID_SawtoothDown) return "SawtoothDown";
    if (g == GUID_Spring) return "Spring";
    if (g == GUID_Damper) return "Damper";
    if (g == GUID_Inertia) return "Inertia";
    if (g == GUID_Friction) return "Friction";
    if (g == GUID_CustomForce) return "CustomForce";
    return "Other";
}

enum Kind { K_CONST, K_RAMP, K_PERIODIC, K_COND, K_OTHER };
static Kind kindOf(REFGUID g)
{
    if (g == GUID_ConstantForce) return K_CONST;
    if (g == GUID_RampForce) return K_RAMP;
    if (g == GUID_Square || g == GUID_Sine || g == GUID_Triangle || g == GUID_SawtoothUp || g == GUID_SawtoothDown) return K_PERIODIC;
    if (g == GUID_Spring || g == GUID_Damper || g == GUID_Inertia || g == GUID_Friction) return K_COND;
    return K_OTHER;
}

// Describes the type-specific block of an effect into out.
static void describe(Kind k, const DIEFFECT *e, char *out, size_t cap)
{
    out[0] = 0;
    if (!e) return;
    int len = sprintf_s(out, cap, "gain=%lu dur=%lu axes=%lu", e->dwGain, e->dwDuration, e->cAxes);
    if (e->rglDirection && e->cAxes) len += sprintf_s(out + len, cap - len, " dir0=%ld", e->rglDirection[0]);
    if (!e->lpvTypeSpecificParams) return;
    if (k == K_CONST && e->cbTypeSpecificParams >= sizeof(DICONSTANTFORCE))
        sprintf_s(out + len, cap - len, " mag=%ld", ((DICONSTANTFORCE *)e->lpvTypeSpecificParams)->lMagnitude);
    else if (k == K_RAMP && e->cbTypeSpecificParams >= sizeof(DIRAMPFORCE))
        sprintf_s(out + len, cap - len, " start=%ld end=%ld", ((DIRAMPFORCE *)e->lpvTypeSpecificParams)->lStart, ((DIRAMPFORCE *)e->lpvTypeSpecificParams)->lEnd);
    else if (k == K_PERIODIC && e->cbTypeSpecificParams >= sizeof(DIPERIODIC)) {
        auto p = (DIPERIODIC *)e->lpvTypeSpecificParams;
        sprintf_s(out + len, cap - len, " mag=%lu offset=%ld period=%lu", p->dwMagnitude, p->lOffset, p->dwPeriod);
    } else if (k == K_COND && e->cbTypeSpecificParams >= sizeof(DICONDITION)) {
        auto c = (DICONDITION *)e->lpvTypeSpecificParams;
        sprintf_s(out + len, cap - len, " offset=%ld pos=%ld neg=%ld possat=%lu negsat=%lu dead=%ld", c->lOffset, c->lPositiveCoefficient,
                  c->lNegativeCoefficient, c->dwPositiveSaturation, c->dwNegativeSaturation, c->lDeadBand);
    }
}

static long magnitudeOf(Kind k, const DIEFFECT *e)
{
    if (!e || !e->lpvTypeSpecificParams) return 0;
    if (k == K_CONST && e->cbTypeSpecificParams >= sizeof(DICONSTANTFORCE)) return ((DICONSTANTFORCE *)e->lpvTypeSpecificParams)->lMagnitude;
    if (k == K_PERIODIC && e->cbTypeSpecificParams >= sizeof(DIPERIODIC)) return (long)((DIPERIODIC *)e->lpvTypeSpecificParams)->dwMagnitude;
    if (k == K_COND && e->cbTypeSpecificParams >= sizeof(DICONDITION)) return ((DICONDITION *)e->lpvTypeSpecificParams)->lPositiveCoefficient;
    if (k == K_RAMP && e->cbTypeSpecificParams >= sizeof(DIRAMPFORCE)) return ((DIRAMPFORCE *)e->lpvTypeSpecificParams)->lEnd;
    return 0;
}

// ---------------------------------------------------------------------------------------------------------------
// Vtable patching, one original per (hook, vtable).
enum Hook { H_GETCAPS, H_GETINFO_W, H_GETINFO_A, H_CREATEDEV_W, H_CREATEDEV_A, H_ENUM_W, H_ENUM_A, H_SETPROP, H_ACQUIRE, H_SETCOOP, H_SETFMT, H_CREATEEFF, H_SENDCMD,
            H_ESCAPE, H_E_SETPARAMS, H_E_START, H_E_STOP, H_E_RELEASE, H_COUNT };
struct Slot { void **vt; void *orig; };
static Slot g_slots[H_COUNT][4];

static void *orig(Hook h, void *self)
{
    void **vt = *(void ***)self;
    for (auto &s : g_slots[h]) if (s.vt == vt) return s.orig;
    return g_slots[h][0].orig;
}

static void patch(Hook h, void *obj, int index, void *hook)
{
    void **vt = *(void ***)obj;
    if (vt[index] == hook) return;
    for (auto &s : g_slots[h]) {
        if (s.vt == vt) return;
        if (!s.vt) {
            DWORD old;
            if (!VirtualProtect(&vt[index], sizeof(void *), PAGE_EXECUTE_READWRITE, &old)) return;
            s.vt = vt;
            s.orig = vt[index];
            vt[index] = hook;
            VirtualProtect(&vt[index], sizeof(void *), old, &old);
            return;
        }
    }
}

// Per-effect bookkeeping for the per-second summaries.
struct Eff { void *p; Kind k; int id; long n, mn, mx, last; DWORD lastLog; DWORD gainLast; long starts; };
static Eff g_eff[64];
static int g_nextId;

static Eff *effFor(void *p)
{
    for (auto &e : g_eff) if (e.p == p) return &e;
    return nullptr;
}


typedef HRESULT(STDMETHODCALLTYPE *SetParamsFn)(IDirectInputEffect *, LPCDIEFFECT, DWORD);
static HRESULT STDMETHODCALLTYPE hkSetParams(IDirectInputEffect *self, LPCDIEFFECT e, DWORD flags)
{
    EnterCriticalSection(&g_cs);
    Eff *r = effFor(self);
    if (r) {
        long m = magnitudeOf(r->k, e);
        static int fullLogged;
        if (fullLogged < 20) {
            fullLogged++;
            char d[400];
            describe(r->k, e, d, sizeof d);
            LeaveCriticalSection(&g_cs);
            dlog("  E%d SetParameters flags=0x%lx %s", r->id, flags, d);
            EnterCriticalSection(&g_cs);
        }
        if (!r->n) { r->mn = r->mx = m; }
        r->n++;
        if (m < r->mn) r->mn = m;
        if (m > r->mx) r->mx = m;
        r->last = m;
        if (e && (flags & DIEP_GAIN)) r->gainLast = e->dwGain;
        DWORD now = GetTickCount();
        if (now - r->lastLog >= 1000) {
            Eff copy = *r;
            r->n = 0;
            r->lastLog = now;
            LeaveCriticalSection(&g_cs);
            dlog("  E%d per-second: %ld updates, mag %ld..%ld, last %ld, gain %lu, start requests so far %ld", copy.id, copy.n, copy.mn, copy.mx, copy.last, copy.gainLast, copy.starts);
            EnterCriticalSection(&g_cs);
        }
    }
    LeaveCriticalSection(&g_cs);
    if (flags & DIEP_START) {
        if (r) InterlockedIncrement(&r->starts);
        flags &= ~DIEP_START;   // muted: never start on the hardware
    }
    if (!flags) return DI_OK;
    return ((SetParamsFn)orig(H_E_SETPARAMS, self))(self, e, flags);
}

typedef HRESULT(STDMETHODCALLTYPE *StartFn)(IDirectInputEffect *, DWORD, DWORD);
static HRESULT STDMETHODCALLTYPE hkStart(IDirectInputEffect *self, DWORD iter, DWORD flags)
{
    Eff *r = effFor(self);
    if (r) {
        LONG n = InterlockedIncrement(&r->starts);
        if (n <= 5) dlog("  E%d Start(iter=%lu flags=0x%lx) swallowed (muted)", r->id, iter, flags);
    }
    return DI_OK;
}

typedef HRESULT(STDMETHODCALLTYPE *StopFn)(IDirectInputEffect *);
static HRESULT STDMETHODCALLTYPE hkStop(IDirectInputEffect *self)
{
    Eff *r = effFor(self);
    static LONG stops;
    if (r && InterlockedIncrement(&stops) <= 10) dlog("  E%d Stop", r->id);
    return ((StopFn)orig(H_E_STOP, self))(self);
}

typedef ULONG(STDMETHODCALLTYPE *ReleaseFn)(IUnknown *);
static ULONG STDMETHODCALLTYPE hkEffRelease(IUnknown *self)
{
    ULONG n = ((ReleaseFn)orig(H_E_RELEASE, self))(self);
    if (!n) {
        EnterCriticalSection(&g_cs);
        Eff *r = effFor(self);
        if (r) {
            Eff copy = *r;
            r->p = nullptr;
            LeaveCriticalSection(&g_cs);
            dlog("  E%d released (%ld start requests swallowed in total)", copy.id, copy.starts);
            return n;
        }
        LeaveCriticalSection(&g_cs);
    }
    return n;
}

typedef HRESULT(STDMETHODCALLTYPE *CreateEffectFn)(void *, REFGUID, LPCDIEFFECT, LPDIRECTINPUTEFFECT *, LPUNKNOWN);
static HRESULT STDMETHODCALLTYPE hkCreateEffect(void *self, REFGUID g, LPCDIEFFECT e, LPDIRECTINPUTEFFECT *out, LPUNKNOWN unk)
{
    HRESULT hr = ((CreateEffectFn)orig(H_CREATEEFF, self))(self, g, e, out, unk);
    Kind k = kindOf(g);
    char d[400], at[200];
    describe(k, e, d, sizeof d);
    where(_ReturnAddress(), at, sizeof at);
    int id = 0;
    if (SUCCEEDED(hr) && out && *out) {
        IDirectInputEffect *fx = *out;
        patch(H_E_SETPARAMS, fx, 6, (void *)&hkSetParams);
        patch(H_E_START, fx, 7, (void *)&hkStart);
        patch(H_E_STOP, fx, 8, (void *)&hkStop);
        patch(H_E_RELEASE, fx, 2, (void *)&hkEffRelease);
        EnterCriticalSection(&g_cs);
        for (auto &r : g_eff) {
            if (!r.p) { r = {}; r.p = fx; r.k = k; r.id = id = ++g_nextId; break; }
        }
        LeaveCriticalSection(&g_cs);
    }
    dlog("CreateEffect %s on dev %p -> hr=0x%08lx E%d  %s  at %s", effectName(g), self, (unsigned long)hr, id, d, at);
    return hr;
}

typedef HRESULT(STDMETHODCALLTYPE *SetPropFn)(void *, REFGUID, LPCDIPROPHEADER);
static HRESULT STDMETHODCALLTYPE hkSetProp(void *self, REFGUID prop, LPCDIPROPHEADER h)
{
    HRESULT hr = ((SetPropFn)orig(H_SETPROP, self))(self, prop, h);
    const char *name = "?";
    char val[160] = "";
    // Predefined DIPROP_* are small integers cast to GUID references, so compare addresses.
    if (&prop == &DIPROP_RANGE) name = "RANGE";
    else if (&prop == &DIPROP_DEADZONE) name = "DEADZONE";
    else if (&prop == &DIPROP_SATURATION) name = "SATURATION";
    else if (&prop == &DIPROP_FFGAIN) name = "FFGAIN";
    else if (&prop == &DIPROP_AUTOCENTER) name = "AUTOCENTER";
    else if (&prop == &DIPROP_BUFFERSIZE) name = "BUFFERSIZE";
    else if (&prop == &DIPROP_AXISMODE) name = "AXISMODE";
    if (h && h->dwSize == sizeof(DIPROPRANGE) && &prop == &DIPROP_RANGE)
        sprintf_s(val, "obj=%lu how=%lu min=%ld max=%ld", h->dwObj, h->dwHow, ((DIPROPRANGE *)h)->lMin, ((DIPROPRANGE *)h)->lMax);
    else if (h && h->dwSize == sizeof(DIPROPDWORD))
        sprintf_s(val, "obj=%lu how=%lu value=%lu", h->dwObj, h->dwHow, ((DIPROPDWORD *)h)->dwData);
    static LONG n;
    if (InterlockedIncrement(&n) <= 60) dlog("SetProperty %s (%p) dev %p %s -> 0x%08lx", name, (const void *)&prop, self, val, (unsigned long)hr);
    return hr;
}

typedef HRESULT(STDMETHODCALLTYPE *AcquireFn)(void *);
static HRESULT STDMETHODCALLTYPE hkAcquire(void *self)
{
    HRESULT hr = ((AcquireFn)orig(H_ACQUIRE, self))(self);
    static LONG n;
    if ((FAILED(hr) || hr == DI_OK) && InterlockedIncrement(&n) <= 30) dlog("Acquire dev %p -> 0x%08lx", self, (unsigned long)hr);
    return hr;
}

typedef HRESULT(STDMETHODCALLTYPE *SetCoopFn)(void *, HWND, DWORD);
static HRESULT STDMETHODCALLTYPE hkSetCoop(void *self, HWND w, DWORD flags)
{
    HRESULT hr = ((SetCoopFn)orig(H_SETCOOP, self))(self, w, flags);
    dlog("SetCooperativeLevel dev %p hwnd %p flags 0x%lx%s%s%s -> 0x%08lx", self, (void *)w, flags, (flags & DISCL_EXCLUSIVE) ? " EXCLUSIVE" : "",
         (flags & DISCL_BACKGROUND) ? " BACKGROUND" : "", (flags & DISCL_FOREGROUND) ? " FOREGROUND" : "", (unsigned long)hr);
    return hr;
}

typedef HRESULT(STDMETHODCALLTYPE *SetFmtFn)(void *, LPCDIDATAFORMAT);
static HRESULT STDMETHODCALLTYPE hkSetFmt(void *self, LPCDIDATAFORMAT f)
{
    HRESULT hr = ((SetFmtFn)orig(H_SETFMT, self))(self, f);
    dlog("SetDataFormat dev %p objects=%lu size=%lu -> 0x%08lx", self, f ? f->dwNumObjs : 0, f ? f->dwDataSize : 0, (unsigned long)hr);
    return hr;
}

typedef HRESULT(STDMETHODCALLTYPE *SendCmdFn)(void *, DWORD);
static HRESULT STDMETHODCALLTYPE hkSendCmd(void *self, DWORD cmd)
{
    HRESULT hr = ((SendCmdFn)orig(H_SENDCMD, self))(self, cmd);
    static LONG n;
    if (InterlockedIncrement(&n) <= 30) dlog("SendForceFeedbackCommand dev %p cmd 0x%lx -> 0x%08lx", self, cmd, (unsigned long)hr);
    return hr;
}

typedef HRESULT(STDMETHODCALLTYPE *EscapeFn)(void *, LPDIEFFESCAPE);
static HRESULT STDMETHODCALLTYPE hkEscape(void *self, LPDIEFFESCAPE e)
{
    dlog("Escape dev %p command %lu in %lu bytes (blocked: muted)", self, e ? e->dwCommand : 0, e ? e->cbInBuffer : 0);
    return DIERR_UNSUPPORTED;
}

static bool retype(DWORD &t)
{
    if (!g_retype || (t & 0xFF) != DI8DEVTYPE_1STPERSON) return false;
    t = (t & 0xFFFF0000) | DI8DEVTYPE_DRIVING | (DI8DEVTYPEDRIVING_THREEPEDALS << 8);
    return true;
}

typedef HRESULT(STDMETHODCALLTYPE *GetCapsFn)(void *, LPDIDEVCAPS);
static HRESULT STDMETHODCALLTYPE hkGetCaps(void *self, LPDIDEVCAPS c)
{
    HRESULT hr = ((GetCapsFn)orig(H_GETCAPS, self))(self, c);
    if (SUCCEEDED(hr) && c) retype(c->dwDevType);
    return hr;
}
typedef HRESULT(STDMETHODCALLTYPE *GetInfoWFn)(void *, LPDIDEVICEINSTANCEW);
static HRESULT STDMETHODCALLTYPE hkGetInfoW(void *self, LPDIDEVICEINSTANCEW d)
{
    HRESULT hr = ((GetInfoWFn)orig(H_GETINFO_W, self))(self, d);
    if (SUCCEEDED(hr) && d) retype(d->dwDevType);
    return hr;
}
typedef HRESULT(STDMETHODCALLTYPE *GetInfoAFn)(void *, LPDIDEVICEINSTANCEA);
static HRESULT STDMETHODCALLTYPE hkGetInfoA(void *self, LPDIDEVICEINSTANCEA d)
{
    HRESULT hr = ((GetInfoAFn)orig(H_GETINFO_A, self))(self, d);
    if (SUCCEEDED(hr) && d) retype(d->dwDevType);
    return hr;
}

static void hookDevice(void *dev, bool wide, bool describeIt = true)
{
    patch(H_GETCAPS, dev, 3, (void *)&hkGetCaps);
    patch(wide ? H_GETINFO_W : H_GETINFO_A, dev, 15, wide ? (void *)&hkGetInfoW : (void *)&hkGetInfoA);
    patch(H_SETPROP, dev, 6, (void *)&hkSetProp);
    patch(H_ACQUIRE, dev, 7, (void *)&hkAcquire);
    patch(H_SETFMT, dev, 11, (void *)&hkSetFmt);
    patch(H_SETCOOP, dev, 13, (void *)&hkSetCoop);
    patch(H_CREATEEFF, dev, 18, (void *)&hkCreateEffect);
    patch(H_SENDCMD, dev, 22, (void *)&hkSendCmd);
    patch(H_ESCAPE, dev, 24, (void *)&hkEscape);
    if (!describeIt) return;
    if (wide) {
        DIDEVICEINSTANCEW di = {sizeof di};
        ((IDirectInputDevice8W *)dev)->GetDeviceInfo(&di);
        DIDEVCAPS caps = {sizeof caps};
        ((IDirectInputDevice8W *)dev)->GetCapabilities(&caps);
        dlog("  device %p is \"%ls\" type 0x%lx axes %lu buttons %lu ff=%d", dev, di.tszProductName, di.dwDevType, caps.dwAxes, caps.dwButtons, (caps.dwFlags & DIDC_FORCEFEEDBACK) ? 1 : 0);
    } else {
        DIDEVICEINSTANCEA di = {sizeof di};
        ((IDirectInputDevice8A *)dev)->GetDeviceInfo(&di);
        DIDEVCAPS caps = {sizeof caps};
        ((IDirectInputDevice8A *)dev)->GetCapabilities(&caps);
        dlog("  device %p is \"%s\" type 0x%lx axes %lu buttons %lu ff=%d", dev, di.tszProductName, di.dwDevType, caps.dwAxes, caps.dwButtons, (caps.dwFlags & DIDC_FORCEFEEDBACK) ? 1 : 0);
    }
}

typedef HRESULT(STDMETHODCALLTYPE *CreateDevFn)(void *, REFGUID, void **, LPUNKNOWN);
static HRESULT STDMETHODCALLTYPE hkCreateDevW(void *self, REFGUID g, void **out, LPUNKNOWN unk)
{
    HRESULT hr = ((CreateDevFn)orig(H_CREATEDEV_W, self))(self, g, out, unk);
    if (fromSystem(_ReturnAddress())) { if (SUCCEEDED(hr) && out && *out) hookDevice(*out, true, false); return hr; }
    char at[200];
    where(_ReturnAddress(), at, sizeof at);
    dlog("CreateDevice(W) {%08lx-...} -> 0x%08lx dev %p at %s", g.Data1, (unsigned long)hr, out ? *out : nullptr, at);
    if (SUCCEEDED(hr) && out && *out) hookDevice(*out, true);
    return hr;
}
static HRESULT STDMETHODCALLTYPE hkCreateDevA(void *self, REFGUID g, void **out, LPUNKNOWN unk)
{
    HRESULT hr = ((CreateDevFn)orig(H_CREATEDEV_A, self))(self, g, out, unk);
    if (fromSystem(_ReturnAddress())) { if (SUCCEEDED(hr) && out && *out) hookDevice(*out, false, false); return hr; }
    char at[200];
    where(_ReturnAddress(), at, sizeof at);
    dlog("CreateDevice(A) {%08lx-...} -> 0x%08lx dev %p at %s", g.Data1, (unsigned long)hr, out ? *out : nullptr, at);
    if (SUCCEEDED(hr) && out && *out) hookDevice(*out, false);
    return hr;
}

struct EnumCtxW { LPDIENUMDEVICESCALLBACKW cb; LPVOID ref; };
struct EnumCtxA { LPDIENUMDEVICESCALLBACKA cb; LPVOID ref; };
static BOOL CALLBACK enumW(LPCDIDEVICEINSTANCEW d, LPVOID ref)
{
    auto c = (EnumCtxW *)ref;
    DIDEVICEINSTANCEW copy = *d;
    bool rt = retype(copy.dwDevType);
    BOOL r = c->cb(&copy, c->ref);
    dlog("  enum: \"%ls\" type 0x%lx%s {%08lx} -> %s", d->tszProductName, copy.dwDevType, rt ? " (retyped)" : "", d->guidInstance.Data1, r ? "continue" : "stop");
    return r;
}
static BOOL CALLBACK enumA(LPCDIDEVICEINSTANCEA d, LPVOID ref)
{
    auto c = (EnumCtxA *)ref;
    DIDEVICEINSTANCEA copy = *d;
    bool rt = retype(copy.dwDevType);
    BOOL r = c->cb(&copy, c->ref);
    dlog("  enum: \"%s\" type 0x%lx%s {%08lx} -> %s", d->tszProductName, copy.dwDevType, rt ? " (retyped)" : "", d->guidInstance.Data1, r ? "continue" : "stop");
    return r;
}
typedef HRESULT(STDMETHODCALLTYPE *EnumWFn)(void *, DWORD, LPDIENUMDEVICESCALLBACKW, LPVOID, DWORD);
typedef HRESULT(STDMETHODCALLTYPE *EnumAFn)(void *, DWORD, LPDIENUMDEVICESCALLBACKA, LPVOID, DWORD);
static HRESULT STDMETHODCALLTYPE hkEnumW(void *self, DWORD type, LPDIENUMDEVICESCALLBACKW cb, LPVOID ref, DWORD flags)
{
    static LONG n;
    bool show = InterlockedIncrement(&n) <= 6;
    if (show) dlog("EnumDevices(W) type %lu flags 0x%lx", type, flags);
    if ((!show && !g_retype) || !cb) return ((EnumWFn)orig(H_ENUM_W, self))(self, type, cb, ref, flags);
    EnumCtxW c = {cb, ref};
    return ((EnumWFn)orig(H_ENUM_W, self))(self, type, enumW, &c, flags);
}
static HRESULT STDMETHODCALLTYPE hkEnumA(void *self, DWORD type, LPDIENUMDEVICESCALLBACKA cb, LPVOID ref, DWORD flags)
{
    static LONG n;
    bool show = InterlockedIncrement(&n) <= 6;
    if (show) dlog("EnumDevices(A) type %lu flags 0x%lx", type, flags);
    if ((!show && !g_retype) || !cb) return ((EnumAFn)orig(H_ENUM_A, self))(self, type, cb, ref, flags);
    EnumCtxA c = {cb, ref};
    return ((EnumAFn)orig(H_ENUM_A, self))(self, type, enumA, &c, flags);
}

// ---------------------------------------------------------------------------------------------------------------
static void init()
{
    if (InterlockedExchange(&g_inited, 1)) return;
    InitializeCriticalSection(&g_cs);
    g_t0 = GetTickCount();
    char sys[MAX_PATH];
    GetSystemDirectoryA(sys, MAX_PATH);
    strcat_s(sys, "\\dinput8.dll");
    g_real = LoadLibraryA(sys);
    char dir[MAX_PATH];
    DWORD n = GetEnvironmentVariableA("LOCALAPPDATA", dir, MAX_PATH);
    if (n && n < MAX_PATH - 80) {
        strcat_s(dir, "\\dbce");
        CreateDirectoryA(dir, nullptr);
        strcat_s(dir, "\\dinput8log");
        CreateDirectoryA(dir, nullptr);
        char exe[MAX_PATH], path[MAX_PATH];
        GetModuleFileNameA(nullptr, exe, MAX_PATH);
        const char *base = strrchr(exe, '\\') ? strrchr(exe, '\\') + 1 : exe;
        sprintf_s(path, "%s\\dinput8log-%s-%lu.txt", dir, base, GetCurrentProcessId());
        g_log = _fsopen(path, "w", _SH_DENYWR);
    }
    char flag[MAX_PATH];
    GetModuleFileNameA(nullptr, flag, MAX_PATH);
    if (char *slash = strrchr(flag, '\\')) { slash[1] = 0; strcat_s(flag, "dinput8log-retype.txt"); g_retype = GetFileAttributesA(flag) != INVALID_FILE_ATTRIBUTES; }
    dlog("dinput8log 2 (muted: effects never start; retype 1st-person devices as driving: %s) pid %lu, system dinput8 %p", g_retype ? "on" : "off", GetCurrentProcessId(), (void *)g_real);
}

typedef HRESULT(WINAPI *DI8CreateFn)(HINSTANCE, DWORD, REFIID, LPVOID *, LPUNKNOWN);
extern "C" HRESULT WINAPI DirectInput8Create(HINSTANCE h, DWORD ver, REFIID riid, LPVOID *out, LPUNKNOWN unk)
{
    init();
    auto f = g_real ? (DI8CreateFn)GetProcAddress(g_real, "DirectInput8Create") : nullptr;
    HRESULT hr = f ? f(h, ver, riid, out, unk) : E_FAIL;
    bool wide = riid == IID_IDirectInput8W;
    dlog("DirectInput8Create ver 0x%lx %s -> 0x%08lx", ver, wide ? "W" : "A", (unsigned long)hr);
    if (SUCCEEDED(hr) && out && *out && !unk) {
        if (wide) { patch(H_CREATEDEV_W, *out, 3, (void *)&hkCreateDevW); patch(H_ENUM_W, *out, 4, (void *)&hkEnumW); }
        else { patch(H_CREATEDEV_A, *out, 3, (void *)&hkCreateDevA); patch(H_ENUM_A, *out, 4, (void *)&hkEnumA); }
    }
    return hr;
}

#define FWD(name, ret, sig, call, fail)                                                    \
    extern "C" ret WINAPI name sig                                                         \
    {                                                                                      \
        init();                                                                            \
        auto f = (ret(WINAPI *) sig)(g_real ? GetProcAddress(g_real, #name) : nullptr);    \
        return f ? f call : (fail);                                                        \
    }
FWD(DllCanUnloadNow, HRESULT, (void), (), S_FALSE)
FWD(DllGetClassObject, HRESULT, (REFCLSID c, REFIID i, LPVOID *o), (c, i, o), CLASS_E_CLASSNOTAVAILABLE)
FWD(DllRegisterServer, HRESULT, (void), (), E_FAIL)
FWD(DllUnregisterServer, HRESULT, (void), (), E_FAIL)
FWD(GetdfDIJoystick, LPCDIDATAFORMAT, (void), (), nullptr)

BOOL WINAPI DllMain(HINSTANCE self, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(self);
    if (reason == DLL_PROCESS_DETACH && g_log) {
        dlog("detach");
        fclose(g_log);
        g_log = nullptr;
    }
    return TRUE;
}
