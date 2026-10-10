// Live, game-less check of a proxy's force fence and test injection on the real devices (STD-033). Build it under the
// game's exe name (the proxies act only in their game's exe), put it beside a copy of the proxy's dinput8.dll (and the
// game's binding tables if the proxy writes them), set the proxy's ffb-fence.on and inject.on for the run only, then:
//   <exe> <inject.txt path> [product id hex, default 0006346E = MOZA R12]
// Read-only device use (non-exclusive, background, a hidden window of its own; no effect, property write or force call
// of any kind). It prints what the game would read before, during and after raw and action injections.
// First used 2026-10-09 21:49 on V-Rally 4 0.6.0: every sample delivered and ended as commanded.
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <stdio.h>
#include <string>
#include <vector>
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "user32.lib")

struct Dev { GUID inst, prod; char name[260]; };
static BOOL CALLBACK collect(LPCDIDEVICEINSTANCEA d, LPVOID ref)
{
    auto *v = (std::vector<Dev> *)ref;
    Dev x{d->guidInstance, d->guidProduct, {}};
    strcpy_s(x.name, d->tszProductName);
    v->push_back(x);
    return DIENUM_CONTINUE;
}
static std::string text(const GUID &g)
{
    char b[40];
    sprintf_s(b, "{%08lx-%04hx-%04hx-%02x%02x-%02x%02x%02x%02x%02x%02x}", g.Data1, g.Data2, g.Data3, g.Data4[0], g.Data4[1], g.Data4[2], g.Data4[3], g.Data4[4], g.Data4[5], g.Data4[6], g.Data4[7]);
    return b;
}
static const char *g_injectPath;
static void inject(const std::string &line)
{
    FILE *f = nullptr;
    fopen_s(&f, g_injectPath, "w");
    if (f) { fprintf(f, "%s\n", line.c_str()); fclose(f); }
    printf("  -> %s\n", line.c_str());
}

int main(int argc, char **argv)
{
    if (argc < 2) { printf("usage: %s <inject.txt path> [product id hex]\n", argv[0]); return 64; }
    g_injectPath = argv[1];
    const unsigned long product = argc > 2 ? strtoul(argv[2], nullptr, 16) : 0x0006346EUL;
    IDirectInput8A *di = nullptr;
    HRESULT hr = DirectInput8Create(GetModuleHandleA(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8A, (void **)&di, nullptr);
    printf("DirectInput8Create 0x%08lx\n", hr);
    if (FAILED(hr)) return 1;
    void *w = nullptr;
    printf("QueryInterface W: 0x%08lx (expect E_NOINTERFACE from the fence)\n", di->QueryInterface(IID_IDirectInput8W, &w));
    std::vector<Dev> devs;
    di->EnumDevices(DI8DEVCLASS_GAMECTRL, collect, &devs, DIEDFL_ATTACHEDONLY);
    IDirectInputDevice8A *wheel = nullptr;
    Dev wheelInfo{};
    for (auto &d : devs) {
        printf("device %s product %08lx %s\n", text(d.inst).c_str(), d.prod.Data1, d.name);
        if (d.prod.Data1 == product && !wheel) { di->CreateDevice(d.inst, &wheel, nullptr); wheelInfo = d; }
    }
    if (!wheel) { printf("no device with product %08lx\n", product); return 2; }
    // A hidden top-level window of this process (never shown, never focused): DirectInput refuses the console's window.
    WNDCLASSA wc{}; wc.lpfnWndProc = DefWindowProcA; wc.hInstance = GetModuleHandleA(nullptr); wc.lpszClassName = "dbceProbe";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, "dbceProbe", "probe", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, wc.hInstance, nullptr);
    const HRESULT fmt = wheel->SetDataFormat(&c_dfDIJoystick2);   // in order: argument evaluation order is unspecified
    const HRESULT coop = wheel->SetCooperativeLevel(hwnd, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);
    const HRESULT acq = wheel->Acquire();
    printf("SetDataFormat 0x%08lx, SetCooperativeLevel 0x%08lx, Acquire 0x%08lx\n", fmt, coop, acq);
    auto read = [&](const char *what) {
        DIJOYSTATE2 s{};
        wheel->Poll();
        HRESULT r = wheel->GetDeviceState(sizeof s, &s);
        printf("  %-22s 0x%08lx X=%ld Z=%ld Rz=%ld slider1=%ld b31=%d b35=%d pov=%ld\n", what, r, s.lX, s.lZ, s.lRz, s.rglSlider[1], !!s.rgbButtons[31], !!s.rgbButtons[35], (long)s.rgdwPOV[0]);
        return s;
    };
    read("rest");
    const std::string dev = text(wheelInfo.inst);
    inject("inject raw axis 0 dev=" + dev + " value=65535 ms=1200");
    Sleep(300); read("raw X max");
    Sleep(1200); read("after the raw X ends");
    inject("inject action throttle 1 ms=1000"); Sleep(300); read("action throttle 1");
    inject("inject action steer -0.5 ms=1000"); Sleep(300); read("action steer -0.5");
    inject("inject action start 1 ms=800"); Sleep(300); read("action start (b35)");
    inject("inject action navLeft 1 ms=800"); Sleep(300); read("action navLeft (hat)");
    Sleep(1200); read("all ended");
    wheel->Unacquire();
    wheel->Release();
    di->Release();
    return 0;
}
