// See dbce_overlay_d3d11.h.
#include "dbce_overlay_d3d11.h"
#include "dbce_panel.h"
#include "dbce_panel_input.h"
#include "../third_party/imgui/imgui.h"
#include "../third_party/imgui/backends/imgui_impl_dx11.h"
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <stdarg.h>
#include <stdio.h>

namespace dbce { namespace overlay {

namespace {
void (*g_log)(const char *);
void logf(const char *fmt, ...)
{
    if (!g_log) return;
    char b[256];
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf_s(b, sizeof b, _TRUNCATE, fmt, ap);
    va_end(ap);
    g_log(b);
}

typedef HRESULT(STDMETHODCALLTYPE *PresentFn)(IDXGISwapChain *, UINT, UINT);
typedef HRESULT(STDMETHODCALLTYPE *Present1Fn)(IDXGISwapChain1 *, UINT, UINT, const DXGI_PRESENT_PARAMETERS *);
typedef HRESULT(STDMETHODCALLTYPE *ResizeFn)(IDXGISwapChain *, UINT, UINT, UINT, DXGI_FORMAT, UINT);
PresentFn g_present;
Present1Fn g_present1;
ResizeFn g_resize;

CRITICAL_SECTION g_cs;              // the render state below: Present and ResizeBuffers can come from different threads
IDXGISwapChain *g_chain;            // the swap chain the panel draws on (its window has the input hook); not owned
ID3D11Device *g_device;             // owned reference while ImGui's DX11 backend uses it
ID3D11DeviceContext *g_context;
ID3D11RenderTargetView *g_rtv;
bool g_imgui, g_backend, g_notD3D11Noted;
LARGE_INTEGER g_freq, g_last;

void releaseTarget() { if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; } }

void shutdownBackend()
{
    releaseTarget();
    if (g_backend) { ImGui_ImplDX11_Shutdown(); g_backend = false; }
    if (g_context) { g_context->Release(); g_context = nullptr; }
    if (g_device) { g_device->Release(); g_device = nullptr; }
}

// The game window: the largest swap chain output wins, so a launcher or splash window does not keep the hook.
bool adopt(IDXGISwapChain *sc)
{
    DXGI_SWAP_CHAIN_DESC d;
    if (FAILED(sc->GetDesc(&d)) || !d.OutputWindow) return false;
    if (sc == g_chain) return true;
    RECT r;
    LONG area = GetClientRect(d.OutputWindow, &r) ? (r.right - r.left) * (r.bottom - r.top) : 0;
    LONG current = 0;
    HWND cw = dbce::panel::InputWindow();
    if (g_chain && cw && IsWindow(cw) && GetClientRect(cw, &r)) current = (r.right - r.left) * (r.bottom - r.top);
    if (g_chain && area <= current) return false;
    ID3D11Device *dev = nullptr;
    if (FAILED(sc->GetDevice(__uuidof(ID3D11Device), (void **)&dev)) || !dev) {
        if (!g_notD3D11Noted) { logf("overlay: a swap chain without a D3D11 device (D3D12?); panel not drawn on it"); g_notD3D11Noted = true; }
        return false;
    }
    dev->Release();
    shutdownBackend();
    g_chain = sc;
    bool hooked = dbce::panel::AttachInput(d.OutputWindow);
    logf("overlay: drawing on the swap chain of window %p (%ldx%ld)%s", (void *)d.OutputWindow, (long)d.BufferDesc.Width,
         (long)d.BufferDesc.Height, hooked ? "; F6 input attached" : "; input hook FAILED");
    return true;
}

void render(IDXGISwapChain *sc)
{
    if (sc == g_chain) dbce::panel::PollDev();
    if (sc == g_chain && !dbce::panel::IsOpen()) return;   // closed: nothing per frame on the game's chain
    EnterCriticalSection(&g_cs);
    if (!adopt(sc) || sc != g_chain || !dbce::panel::IsOpen()) { LeaveCriticalSection(&g_cs); return; }
    ID3D11Device *dev = nullptr;
    if (FAILED(sc->GetDevice(__uuidof(ID3D11Device), (void **)&dev)) || !dev) { LeaveCriticalSection(&g_cs); return; }
    if (dev != g_device) {   // first frame, or the game recreated its device
        shutdownBackend();
        g_device = dev;
        g_device->AddRef();
        g_device->GetImmediateContext(&g_context);
        if (!g_imgui) {
            ImGui::CreateContext();
            ImGuiIO &io = ImGui::GetIO();
            io.IniFilename = nullptr;
            io.LogFilename = nullptr;
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NoMouseCursorChange;
            io.MouseDrawCursor = true;   // games hide the system cursor; the panel draws its own while open
            QueryPerformanceFrequency(&g_freq);
            g_imgui = true;
        }
        g_backend = ImGui_ImplDX11_Init(g_device, g_context);
        if (!g_backend) logf("overlay: ImGui DX11 backend could not start");
    }
    dev->Release();
    if (!g_backend) { LeaveCriticalSection(&g_cs); return; }
    ID3D11Texture2D *back = nullptr;
    if (FAILED(sc->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&back)) || !back) { LeaveCriticalSection(&g_cs); return; }
    D3D11_TEXTURE2D_DESC td;
    back->GetDesc(&td);
    if (!g_rtv && FAILED(g_device->CreateRenderTargetView(back, nullptr, &g_rtv))) g_rtv = nullptr;
    back->Release();
    if (!g_rtv) { LeaveCriticalSection(&g_cs); return; }
    const int w = (int)td.Width, h = (int)td.Height;

    if (dbce::panel::Prepare(w, h)) { dbce::panel::BuildFonts(); ImGui_ImplDX11_InvalidateDeviceObjects(); }
    ImGui_ImplDX11_NewFrame();
    ImGuiIO &io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)w, (float)h);
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    io.DeltaTime = g_last.QuadPart ? (float)((double)(now.QuadPart - g_last.QuadPart) / (double)g_freq.QuadPart) : 1.0f / 60.0f;
    if (io.DeltaTime <= 0.0f || io.DeltaTime > 1.0f) io.DeltaTime = 1.0f / 60.0f;
    g_last = now;
    dbce::panel::DrainInput(w, h);
    ImGui::NewFrame();
    dbce::panel::Draw(w, h);
    ImGui::Render();

    // Draw on the back buffer and give the game its render targets back.
    ID3D11RenderTargetView *saved[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
    ID3D11DepthStencilView *savedDepth = nullptr;
    g_context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, saved, &savedDepth);
    g_context->OMSetRenderTargets(1, &g_rtv, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    g_context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, saved, savedDepth);
    for (auto *v : saved) if (v) v->Release();
    if (savedDepth) savedDepth->Release();
    LeaveCriticalSection(&g_cs);
}

HRESULT STDMETHODCALLTYPE hkPresent(IDXGISwapChain *sc, UINT sync, UINT flags)
{
    if (!(flags & DXGI_PRESENT_TEST)) render(sc);
    return g_present(sc, sync, flags);
}
HRESULT STDMETHODCALLTYPE hkPresent1(IDXGISwapChain1 *sc, UINT sync, UINT flags, const DXGI_PRESENT_PARAMETERS *p)
{
    if (!(flags & DXGI_PRESENT_TEST)) render(sc);
    return g_present1(sc, sync, flags, p);
}
HRESULT STDMETHODCALLTYPE hkResize(IDXGISwapChain *sc, UINT n, UINT w, UINT h, DXGI_FORMAT f, UINT flags)
{
    // The panel's reference to the back buffer must go before the buffers can be resized.
    EnterCriticalSection(&g_cs);
    if (sc == g_chain) releaseTarget();
    LeaveCriticalSection(&g_cs);
    return g_resize(sc, n, w, h, f, flags);
}

bool patchSlot(void **vt, int index, void *hook, void **orig)
{
    DWORD old;
    if (!VirtualProtect(&vt[index], sizeof(void *), PAGE_EXECUTE_READWRITE, &old)) return false;
    *orig = vt[index];
    vt[index] = hook;
    VirtualProtect(&vt[index], sizeof(void *), old, &old);
    return true;
}

DWORD WINAPI worker(LPVOID)
{
    HMODULE d3d = LoadLibraryW(L"d3d11.dll");
    auto create = d3d ? (PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN)GetProcAddress(d3d, "D3D11CreateDeviceAndSwapChain") : nullptr;
    if (!create) { logf("overlay: d3d11.dll not available; no panel"); return 0; }
    WNDCLASSEXW wc = {sizeof wc};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"dbce-overlay-probe";
    RegisterClassExW(&wc);
    HWND w = CreateWindowExW(0, wc.lpszClassName, L"", WS_OVERLAPPED, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr);
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = 64;
    sd.BufferDesc.Height = 64;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = w;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    IDXGISwapChain *sc = nullptr;
    ID3D11Device *dev = nullptr;
    ID3D11DeviceContext *ctx = nullptr;
    HRESULT hr = E_FAIL;
    const D3D_DRIVER_TYPE types[2] = {D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP};
    for (D3D_DRIVER_TYPE t : types) {
        hr = create(nullptr, t, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &sd, &sc, &dev, nullptr, &ctx);
        if (SUCCEEDED(hr)) break;
    }
    if (FAILED(hr) || !sc) { logf("overlay: probe swap chain failed (0x%08lx); no panel", (unsigned long)hr); if (w) DestroyWindow(w); return 0; }
    void **vt = *(void ***)sc;
    EnterCriticalSection(&g_cs);
    bool ok = patchSlot(vt, 8, (void *)&hkPresent, (void **)&g_present) && patchSlot(vt, 13, (void *)&hkResize, (void **)&g_resize);
    IDXGISwapChain1 *sc1 = nullptr;
    bool ok1 = false;
    if (ok && SUCCEEDED(sc->QueryInterface(__uuidof(IDXGISwapChain1), (void **)&sc1)) && sc1) {
        void **vt1 = *(void ***)sc1;
        ok1 = vt1 == vt && patchSlot(vt, 22, (void *)&hkPresent1, (void **)&g_present1);
        sc1->Release();
    }
    LeaveCriticalSection(&g_cs);
    sc->Release(); ctx->Release(); dev->Release();
    DestroyWindow(w);
    logf(ok ? "overlay: D3D11 present hook installed%s (F6 opens the panel)" : "overlay: present hook could not be installed%s",
         ok1 ? ", Present1 too" : "");
    return 0;
}
INIT_ONCE g_once = INIT_ONCE_STATIC_INIT;
BOOL CALLBACK initOnce(PINIT_ONCE, PVOID, PVOID *) { InitializeCriticalSection(&g_cs); return TRUE; }
} // namespace

bool StartD3D11(void (*log)(const char *))
{
    InitOnceExecuteOnce(&g_once, initOnce, nullptr, nullptr);
    g_log = log;
    HANDLE t = CreateThread(nullptr, 0, worker, nullptr, 0, nullptr);
    if (!t) { logf("overlay: worker could not start (%lu)", GetLastError()); return false; }
    CloseHandle(t);
    return true;
}

}} // namespace dbce::overlay
