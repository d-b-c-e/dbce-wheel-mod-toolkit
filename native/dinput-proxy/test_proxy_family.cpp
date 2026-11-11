// Offline checks for the attach-time proxy pieces: dbce_proxy_core.hpp's attach(), dbce_proxy_span.hpp (span rules,
// user32 import-slot wrappers, the import patcher), dbce_proxy_dxgi.hpp and dbce_proxy_muted.hpp. The test exe plays
// the game: it attaches, points its own import slots at the wrappers and calls through them. No game, no display
// change, no device is opened beyond the system keyboard. Built x64 at /W4 /WX by build_proxy_family_test.ps1, which
// removes the test's log folder (%LOCALAPPDATA%\dbce\proxyfamilytest) afterwards.
#include "dbce_proxy_dxgi.hpp"
#include "dbce_proxy_muted.hpp"
#include <stdio.h>
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

namespace proxy = dbce::proxy;
using namespace dbce::proxy::span;

static int failures;
static void check(bool ok, const char *what) { printf("%s %s\n", ok ? "ok  " : "FAIL", what); if (!ok) failures++; }
static RECT R(long l, long t, long r, long b) { return RECT{l, t, r, b}; }

static void spanRules()
{
    RECT span{};
    std::string why;
    const RECT P = R(0, 0, 2560, 1440), L = R(-2560, 0, 0, 1440), Rt = R(2560, 0, 5120, 1440), dash = R(314, -1080, 2234, 0);
    check(SpanFromMonitors({P, L, Rt}, P, span) && SameRect(span, R(-2560, 0, 5120, 1440)),
          "Sim Racing layout (primary in the middle) spans -2560..5120");
    check(!SpanFromMonitors({R(0, 0, 7680, 1440)}, R(0, 0, 7680, 1440), span), "Surround (one display) is not a span");
    check(!SpanFromMonitors({R(0, 0, 7680, 1440), R(7680, 0, 10240, 1440)}, R(0, 0, 7680, 1440), span),
          "Surround with a display beside it is still not a span");
    check(!SpanFromMonitors({P, Rt}, P, span), "two monitors are not a span");
    check(!SpanFromMonitors({R(-2570, 0, -10, 1440), P, Rt}, P, span), "a gap is not a span");
    check(!SpanFromMonitors({R(-2550, 0, 10, 1440), P, Rt}, P, span), "an overlap is not a span");
    check(!SpanFromMonitors({R(-1920, 0, 0, 1080), P, Rt}, P, span), "unequal monitors are not a span");
    check(!SpanFromMonitors({L, P, Rt, R(5120, 0, 7680, 1440)}, P, span, &why) && why.find("shares the racing row's band") != std::string::npos,
          "four monitors in one row are not a span");
    // The racing row (2026-10-13): a dashboard above the three racing screens is ignored, in any enumeration order.
    span = RECT{};
    check(SpanFromMonitors({L, P, Rt, dash}, P, span, &why) && SameRect(span, R(-2560, 0, 5120, 1440)) &&
          why.find("Separate 7680x1440@-2560,0") == 0, "the rig's four displays span the racing row, dashboard ignored");
    span = RECT{};
    check(SpanFromMonitors({dash, Rt, P, L}, P, span) && SameRect(span, R(-2560, 0, 5120, 1440)), "enumeration order does not matter");
    span = RECT{};
    check(SpanFromMonitors({L, P, Rt, R(0, 1440, 1920, 2520)}, P, span) && SameRect(span, R(-2560, 0, 5120, 1440)),
          "a display below the row is ignored too");
    check(!SpanFromMonitors({P, Rt, R(5120, 0, 7680, 1440)}, P, span, &why) && why.find("the left") != std::string::npos,
          "a primary at the left end is not a span");
    check(!SpanFromMonitors({L, R(0, 0, 2560, 1440), Rt}, R(0, 0, 0, 0), span, &why) && why.find("no primary") != std::string::npos,
          "no primary among the monitors is not a span");
    // The 4:1 Surround rule: three 32:9 monitors (3.6:1 each) still span, as they did before the racing row.
    span = RECT{};
    check(SpanFromMonitors({R(-5120, 0, 0, 1440), R(0, 0, 5120, 1440), R(5120, 0, 10240, 1440)}, R(0, 0, 5120, 1440), span) &&
          SameRect(span, R(-5120, 0, 10240, 1440)), "three 32:9 monitors span 15360 wide (4:1 rule)");

    const RECT primary = R(0, 0, 2560, 1440), s = R(-2560, 0, 5120, 1440);
    RECT d = primary;
    check(SpanDesktop(primary, s, d) && SameRect(d, s), "output 0's desktop rectangle becomes the span");
    d = R(-2560, 0, 0, 1440);
    check(!SpanDesktop(primary, s, d) && SameRect(d, R(-2560, 0, 0, 1440)), "a side output's rectangle is left alone");

    DWORD w = 2560, h = 1440; POINTL p = {0, 0};
    check(SpanMode(primary, s, w, h, p) && w == 7680 && h == 1440 && p.x == -2560 && p.y == 0, "the primary's current mode becomes the span");
    w = 1920; h = 1080; p = {0, 0};
    check(!SpanMode(primary, s, w, h, p) && w == 1920, "a different mode is left alone");
    w = 2560; h = 1440; p = {2560, 0};
    check(!SpanMode(primary, s, w, h, p) && p.x == 2560, "the same size on another monitor is left alone");

    int x = 0, y = 0; UINT f = SWP_NOMOVE;
    check(SpanPlacement(s, x, y, 7680, 1440, f) && x == -2560 && y == 0 && !(f & SWP_NOMOVE), "sizing to the span moves to its corner");
    x = 0; y = 0; f = SWP_NOMOVE;
    check(!SpanPlacement(s, x, y, 2560, 1440, f) && x == 0 && (f & SWP_NOMOVE), "sizing to the primary size is left alone");
    x = 5; y = 6; f = SWP_NOSIZE | SWP_NOZORDER;
    check(!SpanPlacement(s, x, y, 0, 0, f) && x == 5 && f == (SWP_NOSIZE | SWP_NOZORDER), "a move/z-order-only call is left alone");
    // DiRT 4 windowed: a captioned window with the span as its client area is created borderless over the span.
    DWORD style = 0xcf0000;   // WS_OVERLAPPEDWINDOW, as logged
    int cx = 0, cy = 0, cw = 7696, ch = 1479;
    check(SpanFramedCreation(s, 7696, 1479, style, cx, cy, cw, ch) && style == WS_POPUP && cx == -2560 && cy == 0 && cw == 7680 && ch == 1440,
          "a captioned window framing the span is created borderless at the span");
    style = 0xcf0000 | WS_VISIBLE | WS_CLIPCHILDREN; cx = 0; cy = 0; cw = 7696; ch = 1479;
    check(SpanFramedCreation(s, 7696, 1479, style, cx, cy, cw, ch) && style == (WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN),
          "visibility and clipping bits are kept");
    style = 0xcf0000; cx = 0; cy = 0; cw = 2576; ch = 1479;
    check(!SpanFramedCreation(s, 7696, 1479, style, cx, cy, cw, ch) && style == 0xcf0000 && cw == 2576, "a window of another size is left alone");
    style = WS_POPUP; cx = 0; cy = 0; cw = 7680; ch = 1440;
    check(!SpanFramedCreation(s, 7680, 1440, style, cx, cy, cw, ch) && cx == 0, "an already borderless window is not this rule's case");
}

static std::string readLog()
{
    std::string text;
    if (FILE *f = _fsopen(proxy::state().logPath, "rb", _SH_DENYNO)) {
        char b[4096];
        for (size_t n; (n = fread(b, 1, sizeof b, f)) > 0;) text.append(b, n);
        fclose(f);
    }
    return text;
}
static bool logHas(const char *needle) { return readLog().find(needle) != std::string::npos; }

static int devices, nonPads;
static BOOL CALLBACK onDeviceA(LPCDIDEVICEINSTANCEA d, LPVOID)
{
    devices++;
    if (d->guidProduct.Data1 != 0x028E045E) nonPads++;
    return DIENUM_CONTINUE;
}
static BOOL CALLBACK onDeviceW(LPCDIDEVICEINSTANCEW d, LPVOID)
{
    devices++;
    if (d->guidProduct.Data1 != 0x028E045E) nonPads++;
    return DIENUM_CONTINUE;
}

static proxy::imports::Hook g_imports[] = {
    DBCE_PROXY_IMPORT("user32.dll", dbce::proxy::span, GetSystemMetrics), DBCE_PROXY_IMPORT("user32.dll", dbce::proxy::span, EnumDisplaySettingsA),
    DBCE_PROXY_IMPORT("user32.dll", dbce::proxy::span, GetWindowRect), DBCE_PROXY_IMPORT("user32.dll", dbce::proxy::span, SetWindowLongA),
    DBCE_PROXY_IMPORT("dxgi.dll", dbce::proxy::dxgi, CreateDXGIFactory), DBCE_PROXY_IMPORT("dxgi.dll", dbce::proxy::dxgi, CreateDXGIFactory1),
};

int main()
{
    spanRules();

    static const proxy::Identity kTest = {"proxyfamilytest", "Proxy family test", "0.0.0"};
    proxy::attach(kTest, nullptr, false);
    const proxy::State &s = proxy::state();
    check(s.attached && s.pinned && s.logPath[0] && !s.mute && !s.fence, "attach: pinned, log open, no test switch");
    check(logHas("dbce Proxy family test 0.0.0 in test_proxy_family.exe, pid ") && !logHas("could not pin"), "attach writes the start line");
    check(!s.real && !logHas("system dinput8"), "attach loads nothing (the system dinput8 waits for start)");

    // The import slots: this exe calls the six functions through its own import table.
    proxy::imports::resolve(g_imports);
    BYTE *exe = (BYTE *)GetModuleHandleA(nullptr);
    check(proxy::imports::patch(exe, g_imports, true) == 6, "six import slots wrapped");
    check(proxy::imports::patch(exe, g_imports, true) == 0, "a second patch leaves wrapped slots alone");
    check(GetSystemMetrics(SM_CXSCREEN) == o_GetSystemMetrics(SM_CXSCREEN) && logHas("GetSystemMetrics(0) = "),
          "GetSystemMetrics goes through the wrapper and answers unchanged");
    DEVMODEA a{}, b{};
    a.dmSize = b.dmSize = sizeof a;
    BOOL ra = EnumDisplaySettingsA(nullptr, ENUM_CURRENT_SETTINGS, &a), rb = o_EnumDisplaySettingsA(nullptr, ENUM_CURRENT_SETTINGS, &b);
    check(ra == rb && a.dmPelsWidth == b.dmPelsWidth && a.dmPosition.x == b.dmPosition.x && logHas("EnumDisplaySettingsA(null, -1) = "),
          "EnumDisplaySettingsA answers unchanged while the span is off");
    HWND bogus = (HWND)(ULONG_PTR)0x7ffe1234;
    RECT r1{}, r2{};
    SetLastError(0); BOOL g1 = GetWindowRect(bogus, &r1); DWORD e1 = GetLastError();
    SetLastError(0); BOOL g2 = o_GetWindowRect(bogus, &r2); DWORD e2 = GetLastError();
    check(g1 == g2 && e1 == e2 && e1 != 0, "GetWindowRect on a bad window: same result and last error through the wrapper");
    SetLastError(0); LONG l1 = SetWindowLongA(bogus, -21, 5); e1 = GetLastError();
    SetLastError(0); LONG l2 = o_SetWindowLongA(bogus, -21, 5); e2 = GetLastError();
    check(l1 == l2 && e1 == e2 && e1 != 0, "SetWindowLongA on a bad window: same result and last error through the wrapper");
    SetLastError(1234);
    check(GetSystemMetrics(SM_CXSCREEN) > 0 && GetLastError() == 1234, "a successful wrapped call leaves the caller's last error alone");

    // DXGI: the factory, adapter and output methods are wrapped through the creation imports.
    IDXGIFactory1 *f1 = nullptr;
    int outputs = 0;
    bool same = true;
    if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void **)&f1)) && f1) {
        IDXGIAdapter1 *ad = nullptr;
        for (UINT i = 0; SUCCEEDED(f1->EnumAdapters1(i, &ad)); ++i) {
            IDXGIOutput *o = nullptr;
            for (UINT k = 0; SUCCEEDED(ad->EnumOutputs(k, &o)); ++k) {
                DXGI_OUTPUT_DESC d1{}, d2{};
                o->GetDesc(&d1);
                o->GetDesc(&d2);
                same &= !memcmp(&d1, &d2, sizeof d1) && d1.DesktopCoordinates.right > d1.DesktopCoordinates.left;
                ++outputs;
                o->Release();
            }
            ad->Release();
        }
        f1->Release();
    }
    check(outputs > 0 && same && logHas("CreateDXGIFactory1 = 0x00000000") && logHas("IDXGIOutput::GetDesc("),
          "CreateDXGIFactory1: adapters, outputs and descriptions through the wrapped methods");
    IDXGIFactory *f0 = nullptr;
    IDXGIAdapter *a0 = nullptr;
    IDXGIOutput *o0 = nullptr;
    DXGI_OUTPUT_DESC d0{};
    bool ok = SUCCEEDED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void **)&f0)) && f0 && SUCCEEDED(f0->EnumAdapters(0, &a0)) && a0 &&
              SUCCEEDED(a0->EnumOutputs(0, &o0)) && o0 && SUCCEEDED(o0->GetDesc(&d0)) && d0.DesktopCoordinates.right > d0.DesktopCoordinates.left;
    if (o0) o0->Release();
    if (a0) a0->Release();
    if (f0) f0->Release();
    check(ok, "CreateDXGIFactory(IDXGIFactory): adapters, outputs and descriptions work");

    // [Triple] Screens.
    char ini[MAX_PATH], temp[MAX_PATH];
    GetTempPathA(MAX_PATH, temp);
    GetTempFileNameA(temp, "dps", 0, ini);
    WritePrivateProfileStringA("Triple", "Screens", "Off", ini);
    const int off = readScreens(ini);
    WritePrivateProfileStringA("Triple", "Screens", "Separate monitors", ini);
    const int separate = readScreens(ini);
    const bool wanted = g_wanted;
    WritePrivateProfileStringA("Triple", "Screens", nullptr, ini);
    const int surround = readScreens(ini);
    DeleteFileA(ini);
    check(off == 0 && separate == 2 && wanted && surround == 1 && !g_wanted && logHas("triple screens: Separate monitors (span over three"),
          "[Triple] Screens: Off, Separate monitors (span wanted) and the Surround default");

    // start(): the rest of the start-up, once.
    proxy::start(kTest, nullptr);
    check(s.real && logHas("system dinput8 "), "start loads the system dinput8");

    // The muted DirectInput, A and W.
    IDirectInput8A *di = nullptr;
    HRESULT hr = proxy::muted::create(GetModuleHandleA(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8A, (void **)&di, nullptr, false);
    check(SUCCEEDED(hr) && di, "muted DirectInput8Create (8A) succeeds");
    if (di) {
        devices = nonPads = 0;
        check(SUCCEEDED(di->EnumDevices(DI8DEVCLASS_ALL, onDeviceA, nullptr, 0)) && nonPads == 0, "only no-force Xbox 360 pads are enumerated");
        devices = 0;
        check(SUCCEEDED(di->EnumDevices(DI8DEVCLASS_GAMECTRL, onDeviceA, nullptr, DIEDFL_FORCEFEEDBACK)) && devices == 0, "no force device is enumerated");
        IDirectInputDevice8A *dev = (IDirectInputDevice8A *)1;
        check(FAILED(di->CreateDevice(GUID_Joystick, &dev, nullptr)) && !dev, "GUID_Joystick cannot be created");
        dev = nullptr;
        check(SUCCEEDED(di->CreateDevice(GUID_SysKeyboard, &dev, nullptr)) && dev, "the system keyboard can be created");
        if (dev) dev->Release();
        void *other = (void *)1;
        check(di->QueryInterface(IID_IDirectInput8W, &other) == E_NOINTERFACE && !other, "QueryInterface hands out no other interface");
        check(di->Release() == 0, "the last Release frees the muted object");
    }
    IDirectInput8W *w = (IDirectInput8W *)1;
    check(FAILED(proxy::muted::create(GetModuleHandleA(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8W, (void **)&w, nullptr, false)) && !w,
          "8W is refused unless the game allows it");
    w = nullptr;
    hr = proxy::muted::create(GetModuleHandleA(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8W, (void **)&w, nullptr, true);
    check(SUCCEEDED(hr) && w, "muted DirectInput8Create (8W) succeeds when allowed");
    if (w) {
        devices = nonPads = 0;
        check(SUCCEEDED(w->EnumDevices(DI8DEVCLASS_ALL, onDeviceW, nullptr, 0)) && nonPads == 0, "8W: only no-force Xbox 360 pads are enumerated");
        IDirectInputDevice8W *dev = nullptr;
        check(SUCCEEDED(w->CreateDevice(GUID_SysKeyboard, &dev, nullptr)) && dev, "8W: the system keyboard can be created");
        if (dev) dev->Release();
        IDirectInput8W *again = nullptr;
        check(SUCCEEDED(w->QueryInterface(IID_IDirectInput8W, (void **)&again)) && again == w, "8W: QueryInterface returns the same muted object");
        if (again) again->Release();
        check(w->Release() == 0, "8W: the last Release frees the muted object");
    }
    check(logHas("ffb mute: DirectInput8Create (8A) gave the game the muted interface") &&
          logHas("ffb mute: DirectInput8Create (8W) gave the game the muted interface"), "the muted interfaces are logged");

    printf("%s proxy family, %d failures\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
