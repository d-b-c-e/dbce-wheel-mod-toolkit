// Offline check that the overlay's present hook survives another overlay taking ours as its "original" (Steam's overlay
// does; V-Rally 4 crashed with a stack overflow on 2026-10-09: Steam's hook and ours called each other). Two shapes, one
// per run (the hook's state is per process):
//   (default) the other overlay hooks the vtable slot before the overlay starts, then hooks it again keeping ours as its
//             original;
//   entry     the other overlay patches the first bytes of dxgi's Present with a jump to its hook and keeps ours as
//             its original (the shape in the V-Rally dumps).
// Before the fix the first Present recursed until the stack overflowed; now the loop is seen once, ours calls the
// system's Present (its original bytes put back for the call in the entry shape), and every frame presents. WARP,
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
static bool g_installed, g_alreadyNoted, g_loopNoted, g_bytesNoted, g_skipNoted, g_tailNoted;
static void logLine(const char *s)
{
    printf("  log: %s\n", s);
    if (strstr(s, "present hook installed")) g_installed = true;
    if (strstr(s, "Present was already hooked by")) g_alreadyNoted = true;
    if (strstr(s, "Present was hooked again over ours")) g_loopNoted = true;
    if (strstr(s, "Present: ours and test_overlay_rehook.exe call each other; dxgi.dll's first 14 bytes jump there") && strstr(s, "a copy of its first instructions")) g_bytesNoted = true;
    if (strstr(s, "changed past the copied instructions")) g_tailNoted = true;
    else if (strstr(s, "calls are skipped")) g_skipNoted = true;
}
static bool ffbOn() { return true; }
static void stopFfb() {}
static void drawPage(dbce::panel::Page, bool) {}

// The other overlay: one saved "original", overwritten each time it hooks (Steam's pattern).
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
static void otherHookSlot(void **vt)
{
    DWORD old;
    VirtualProtect(&vt[8], sizeof(void *), PAGE_EXECUTE_READWRITE, &old);
    g_otherOrig = (PresentFn)vt[8];
    vt[8] = (void *)&otherPresent;
    VirtualProtect(&vt[8], sizeof(void *), old, &old);
}
// jmp [rip+0] ; dq otherPresent  (14 bytes, the absolute jump hook libraries write on x64)
static BYTE g_entrySaved[14];
static void otherPatchEntry(void *fn, bool on)
{
    DWORD old;
    VirtualProtect(fn, sizeof g_entrySaved, PAGE_EXECUTE_READWRITE, &old);
    if (on) {
        memcpy(g_entrySaved, fn, sizeof g_entrySaved);
        BYTE j[14] = {0xFF, 0x25, 0, 0, 0, 0};
        void *to = (void *)&otherPresent;
        memcpy(j + 6, &to, 8);
        memcpy(fn, j, sizeof j);
    } else {
        memcpy(fn, g_entrySaved, sizeof g_entrySaved);
    }
    VirtualProtect(fn, sizeof g_entrySaved, old, &old);
    FlushInstructionCache(GetCurrentProcess(), fn, sizeof g_entrySaved);
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

int main(int argc, char **argv)
{
    const bool entryShape = argc > 1 && strcmp(argv[1], "entry") == 0;
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
    void *systemPresent = vt[8];

    if (!entryShape) {
        // 1. The other overlay hooks the slot first (its original: the system's Present); ours goes over it.
        otherHookSlot(vt);
        CHECK(dbce::overlay::StartD3D11(logLine));
        for (int i = 0; i < 100 && !g_installed; ++i) Sleep(50);
        CHECK(g_installed);
        CHECK(g_alreadyNoted);
        g_otherCalls = 0;
        CHECK(SUCCEEDED(sc->Present(0, 0)));
        CHECK(g_otherCalls == 1);                 // game -> ours -> other -> system
        CHECK(!g_loopNoted);
        // 2. It hooks the slot again (a new swap chain) and keeps ours as its original.
        otherHookSlot(vt);
        g_otherCalls = 0;
        g_otherMaxDepth = 0;
        dbce::panel::SetOpen(true);               // the panel draws in this frame too
        CHECK(SUCCEEDED(sc->Present(0, 0)));      // other -> ours -> other -> ours (loop seen) -> system
        CHECK(g_loopNoted);
        CHECK(g_otherMaxDepth == 2);
        g_otherCalls = 0;
        for (int i = 0; i < 3; ++i) CHECK(SUCCEEDED(sc->Present(0, 0)));
        CHECK(g_otherCalls == 3);                 // from then on: game -> other -> ours -> system, once per frame
    } else {
#ifdef _WIN64
        // 1. Ours alone in the slot.
        CHECK(dbce::overlay::StartD3D11(logLine));
        for (int i = 0; i < 100 && !g_installed; ++i) Sleep(50);
        CHECK(g_installed);
        CHECK(!g_alreadyNoted);
        void *ours = vt[8];
        CHECK(ours != systemPresent);
        // 2. The other overlay jumps from dxgi's Present to its hook and takes the slot's hook (ours) as its original.
        g_otherOrig = (PresentFn)ours;
        otherPatchEntry(systemPresent, true);
        g_otherCalls = 0;
        g_otherMaxDepth = 0;
        dbce::panel::SetOpen(true);
        CHECK(SUCCEEDED(sc->Present(0, 0)));      // ours -> dxgi (jumps) -> other -> ours (loop seen) -> copy of dxgi's first instructions
        CHECK(g_bytesNoted);
        CHECK(g_otherCalls == 1);
        CHECK(memcmp(systemPresent, "\xFF\x25\0\0\0\0", 6) == 0);   // the other overlay's jump is never touched
        g_otherCalls = 0;
        for (int i = 0; i < 3; ++i) CHECK(SUCCEEDED(sc->Present(0, 0)));
        CHECK(g_otherCalls == 0);                 // from then on: game -> ours -> the copy -> the rest of dxgi's Present
        CHECK(!g_skipNoted);

        // 3. Two threads presenting at once through the copy (nothing of the live code is written, so nothing to race).
        HWND w2 = CreateWindowExW(0, wc.lpszClassName, L"", WS_POPUP, 0, 0, 640, 360, nullptr, nullptr, wc.hInstance, nullptr);
        ID3D11Device *dev2 = nullptr;
        ID3D11DeviceContext *ctx2 = nullptr;
        IDXGISwapChain *sc2 = makeChain(w2, &dev2, &ctx2);
        CHECK(sc2 != nullptr);
        static volatile LONG bad;
        struct Run { static DWORD WINAPI Go(LPVOID p) { for (int i = 0; i < 200; ++i) if (FAILED(((IDXGISwapChain *)p)->Present(0, 0))) InterlockedIncrement(&bad); return 0; } };
        HANDLE t = CreateThread(nullptr, 0, Run::Go, sc2, 0, nullptr);
        for (int i = 0; i < 200; ++i) if (FAILED(sc->Present(0, 0))) InterlockedIncrement(&bad);
        WaitForSingleObject(t, 30000);
        CloseHandle(t);
        CHECK(bad == 0);
        CHECK(!g_skipNoted);
        if (sc2) { sc2->Release(); ctx2->Release(); dev2->Release(); }
        DestroyWindow(w2);

        // 4. A longer patch that reaches past the copied instructions: the nested call is skipped, never run half-patched.
        DWORD old;
        BYTE *tail = (BYTE *)systemPresent + 24;
        BYTE keep[4];
        memcpy(keep, tail, 4);
        VirtualProtect(tail, 4, PAGE_EXECUTE_READWRITE, &old);
        memset(tail, 0xCC, 4);
        VirtualProtect(tail, 4, old, &old);
        g_otherCalls = 0;
        g_otherMaxDepth = 0;
        HRESULT hr = sc->Present(0, 0);           // ours -> (copy refused) -> skipped; no overflow, no int3 reached
        CHECK(hr == S_OK);
        CHECK(g_tailNoted);
        CHECK(g_otherMaxDepth <= 1);
        VirtualProtect(tail, 4, PAGE_EXECUTE_READWRITE, &old);
        memcpy(tail, keep, 4);
        VirtualProtect(tail, 4, old, &old);
        FlushInstructionCache(GetCurrentProcess(), tail, 4);
        otherPatchEntry(systemPresent, false);
#else
        printf("  skip: the entry shape is checked on x64 only\n");
#endif
    }
    dbce::panel::SetOpen(false);
    sc->Release(); ctx->Release(); dev->Release();
    DestroyWindow(w);
    const char *shape = entryShape ? "patched entry" : "slot hooked again";
    if (failures) printf("FAIL %d re-hook checks (%s)\n", failures, shape);
    else printf("PASS present hook re-hook check (%s: another overlay taking ours as its original does not loop)\n", shape);
    return failures ? 1 : 0;
}
