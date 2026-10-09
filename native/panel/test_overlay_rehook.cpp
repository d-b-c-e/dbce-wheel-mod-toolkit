// Offline check that the overlay's present hook survives another overlay hooking the same vtable slot again after us,
// the way Steam's overlay does for each new swap chain (V-Rally 4 crashed with a stack overflow on 2026-10-09: Steam's
// hook called ours as its "original" while ours called Steam's). A stand-in "other overlay" hooks Present before the
// overlay starts, then hooks it again keeping ours as its original. Before the fix the first Present recursed until
// the stack overflowed; now the loop is seen once, ours calls the system's Present, and every frame presents. WARP,
// hidden window, no game, no device.
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <stdio.h>
#include <string.h>
#include "dbce_panel.h"
#include "dbce_overlay_d3d11.h"
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "user32.lib")

static int failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); ++failures; } } while (0)
static bool g_installed, g_alreadyNoted, g_loopNoted;
static void logLine(const char *s)
{
    printf("  log: %s\n", s);
    if (strstr(s, "present hook installed")) g_installed = true;
    if (strstr(s, "Present was already hooked by")) g_alreadyNoted = true;
    if (strstr(s, "Present was hooked again over ours")) g_loopNoted = true;
}
static bool ffbOn() { return true; }
static void stopFfb() {}
static void drawPage(dbce::panel::Page, bool) {}

// The other overlay: one saved "original" per slot, overwritten each time it hooks (Steam's pattern).
typedef HRESULT(STDMETHODCALLTYPE *PresentFn)(IDXGISwapChain *, UINT, UINT);
static PresentFn g_otherOrig;
static int g_otherCalls, g_otherDepth, g_otherMaxDepth;
static HRESULT STDMETHODCALLTYPE otherPresent(IDXGISwapChain *sc, UINT sync, UINT flags)
{
    ++g_otherCalls;
    if (++g_otherDepth > g_otherMaxDepth) g_otherMaxDepth = g_otherDepth;
    HRESULT hr = g_otherDepth > 50 ? E_FAIL : g_otherOrig(sc, sync, flags);   // the test's own brake: fail, do not overflow
    --g_otherDepth;
    return hr;
}
static void otherHook(void **vt)
{
    DWORD old;
    VirtualProtect(&vt[8], sizeof(void *), PAGE_EXECUTE_READWRITE, &old);
    g_otherOrig = (PresentFn)vt[8];
    vt[8] = (void *)&otherPresent;
    VirtualProtect(&vt[8], sizeof(void *), old, &old);
}

static IDXGISwapChain *makeChain(HWND w, ID3D11Device **dev, ID3D11DeviceContext **ctx)
{
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = 640;
    sd.BufferDesc.Height = 360;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = w;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    IDXGISwapChain *sc = nullptr;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &sd, &sc, dev, nullptr, ctx);
    return SUCCEEDED(hr) ? sc : nullptr;
}

int main()
{
    dbce::panel::Config c;
    c.modName = "Re-hook check";
    c.pages[dbce::panel::PAGE_FFB] = true;
    c.drawPage = drawPage;
    c.ffbOn = ffbOn;
    c.stopFfb = stopFfb;
    c.log = logLine;
    dbce::panel::Configure(c);

    WNDCLASSEXW wc = {sizeof wc};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"dbce-rehook-check";
    RegisterClassExW(&wc);
    HWND w = CreateWindowExW(0, wc.lpszClassName, L"", WS_POPUP, 0, 0, 640, 360, nullptr, nullptr, wc.hInstance, nullptr);
    ID3D11Device *dev = nullptr;
    ID3D11DeviceContext *ctx = nullptr;
    IDXGISwapChain *sc = makeChain(w, &dev, &ctx);
    CHECK(sc != nullptr);
    if (!sc) return 1;
    void **vt = *(void ***)sc;

    // 1. The other overlay hooks first (its original: the system's Present); ours goes over it.
    otherHook(vt);
    CHECK(dbce::overlay::StartD3D11(logLine));
    for (int i = 0; i < 100 && !g_installed; ++i) Sleep(50);
    CHECK(g_installed);
    CHECK(g_alreadyNoted);
    g_otherCalls = 0;
    CHECK(SUCCEEDED(sc->Present(0, 0)));
    CHECK(g_otherCalls == 1);                 // game -> ours -> other -> system
    CHECK(!g_loopNoted);

    // 2. The other overlay hooks again (a new swap chain) and keeps ours as its original: before the fix, a loop.
    otherHook(vt);
    g_otherCalls = 0;
    g_otherMaxDepth = 0;
    dbce::panel::SetOpen(true);               // the panel draws in this frame too
    CHECK(SUCCEEDED(sc->Present(0, 0)));      // other -> ours -> other -> ours (loop seen) -> system
    CHECK(g_loopNoted);
    CHECK(g_otherMaxDepth == 2);
    g_otherCalls = 0;
    for (int i = 0; i < 3; ++i) CHECK(SUCCEEDED(sc->Present(0, 0)));
    CHECK(g_otherCalls == 3);                 // from then on: game -> other -> ours -> system, once per frame
    dbce::panel::SetOpen(false);

    sc->Release(); ctx->Release(); dev->Release();
    DestroyWindow(w);
    printf(failures ? "FAIL %d re-hook checks\n" : "PASS present hook re-hook check (another overlay hooking again over ours does not loop)\n", failures);
    return failures ? 1 : 0;
}
