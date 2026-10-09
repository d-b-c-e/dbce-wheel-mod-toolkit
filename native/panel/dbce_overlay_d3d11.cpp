// See dbce_overlay_d3d11.h (and dbce_overlay_d3d12.h for D3D12 swap chains).
#include "dbce_overlay_d3d11.h"
#include "dbce_overlay_internal.h"
#include "dbce_panel.h"
#include "dbce_panel_input.h"
#include "../third_party/imgui/imgui.h"
#include "../third_party/imgui/backends/imgui_impl_dx11.h"
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace dbce { namespace overlay {

namespace detail {
void (*g_log)(const char *);
bool (*render12)(IDXGISwapChain *);
void (*resize12)(IDXGISwapChain *);
void (*probe12)();
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

// One ImGui context for the process, whichever renderer draws it.
static LARGE_INTEGER g_freq, g_last;
void ensureImGui()
{
    if (ImGui::GetCurrentContext()) return;
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NoMouseCursorChange;
    io.MouseDrawCursor = true;   // games hide the system cursor; the panel draws its own while open
    QueryPerformanceFrequency(&g_freq);
}
// After the renderer's NewFrame: size, time, queued input, the panel, Render.
void drawFrame(int w, int h)
{
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
}

static const char *moduleName(const void *p)
{
    static thread_local char name[MAX_PATH];
    HMODULE m = nullptr;
    if (!p || !GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)p, &m) ||
        !GetModuleFileNameA(m, name, MAX_PATH))
        return "memory outside any module";
    const char *slash = strrchr(name, '\\');
    return slash ? slash + 1 : name;
}

static bool executableAt(const IMAGE_NT_HEADERS *nt, ULONG_PTR rva)
{
    const IMAGE_SECTION_HEADER *s = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++s)
        if ((s->Characteristics & IMAGE_SCN_MEM_EXECUTE) && rva >= s->VirtualAddress && rva < s->VirtualAddress + s->Misc.VirtualSize) return true;
    return false;
}

// The slot's value before anyone patched it: the pointer in the module's file on disk (relative to the file's own
// ImageBase), moved to where the module is loaded. Only a value inside one of the module's executable sections counts.
static void *systemSlot(void **vt, int index)
{
    HMODULE mod = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&vt[index], &mod) || !mod) return nullptr;
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(mod, path, MAX_PATH);
    if (!n || n >= MAX_PATH) return nullptr;
    const BYTE *base = (const BYTE *)mod;
    const IMAGE_NT_HEADERS *nt = (const IMAGE_NT_HEADERS *)(base + ((const IMAGE_DOS_HEADER *)base)->e_lfanew);
    const ULONG_PTR rva = (ULONG_PTR)((const BYTE *)&vt[index] - base);
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return nullptr;
    BYTE head[4096];
    DWORD got = 0;
    void *result = nullptr;
    if (ReadFile(f, head, sizeof head, &got, nullptr) && got >= sizeof(IMAGE_DOS_HEADER)) {
        const LONG lfanew = ((const IMAGE_DOS_HEADER *)head)->e_lfanew;
        const IMAGE_NT_HEADERS *fnt = lfanew > 0 && (DWORD)lfanew + sizeof(IMAGE_NT_HEADERS) <= got ? (const IMAGE_NT_HEADERS *)(head + lfanew) : nullptr;
        if (fnt && fnt->Signature == IMAGE_NT_SIGNATURE && fnt->OptionalHeader.Magic == nt->OptionalHeader.Magic) {
            const IMAGE_SECTION_HEADER *s = IMAGE_FIRST_SECTION(fnt);
            const WORD count = fnt->FileHeader.NumberOfSections;
            if ((const BYTE *)(s + count) <= head + got) {
                for (WORD i = 0; i < count; ++i, ++s) {
                    if (rva < s->VirtualAddress || rva + sizeof(void *) > (ULONG_PTR)s->VirtualAddress + s->SizeOfRawData) continue;
                    LARGE_INTEGER at;
                    at.QuadPart = (LONGLONG)s->PointerToRawData + (LONGLONG)(rva - s->VirtualAddress);
                    ULONG_PTR raw = 0;
                    DWORD r = 0;
                    if (SetFilePointerEx(f, at, nullptr, FILE_BEGIN) && ReadFile(f, &raw, sizeof raw, &r, nullptr) && r == sizeof raw) {
                        const ULONG_PTR fileBase = (ULONG_PTR)fnt->OptionalHeader.ImageBase;
                        if (raw >= fileBase && raw - fileBase < nt->OptionalHeader.SizeOfImage && executableAt(nt, raw - fileBase))
                            result = (void *)(base + (raw - fileBase));
                    }
                    break;
                }
            }
        }
    }
    CloseHandle(f);
    // A slot still holding a pointer into its own module is the system's function as loaded; trust it over the file
    // (the file can be newer than the loaded copy after an update that waits for a restart).
    void *cur = vt[index];
    if ((const BYTE *)cur >= base && (ULONG_PTR)((const BYTE *)cur - base) < nt->OptionalHeader.SizeOfImage &&
        executableAt(nt, (ULONG_PTR)((const BYTE *)cur - base)))
        return cur;
    return result;
}

bool PatchSlot(void **vt, int index, void *hook, SlotHook &h)
{
    h.system = systemSlot(vt, index);
    DWORD old;
    if (!VirtualProtect(&vt[index], sizeof(void *), PAGE_EXECUTE_READWRITE, &old)) return false;
    h.next = vt[index];
    vt[index] = hook;
    VirtualProtect(&vt[index], sizeof(void *), old, &old);
    if (h.next != h.system)
        logf("overlay: %s was already hooked by %s; ours runs first, then it%s", h.name, moduleName(h.next),
             h.system ? "" : " (the system's function is unknown: a hook loop could not be broken)");
    return true;
}

void *Reentered(SlotHook &h)
{
    if (!InterlockedExchange(&h.looped, 1))
        logf(h.system ? "overlay: %s was hooked again over ours by %s, which now calls ours while ours called it; ours calls the system's function from now on"
                      : "overlay: %s was hooked again over ours by %s and the system's function is unknown; the looping call is skipped",
             h.name, moduleName(h.next));
    if (h.system) h.next = h.system;
    return h.system;
}
} // namespace detail

namespace {
using detail::logf;
typedef HRESULT(STDMETHODCALLTYPE *PresentFn)(IDXGISwapChain *, UINT, UINT);
typedef HRESULT(STDMETHODCALLTYPE *Present1Fn)(IDXGISwapChain1 *, UINT, UINT, const DXGI_PRESENT_PARAMETERS *);
typedef HRESULT(STDMETHODCALLTYPE *ResizeFn)(IDXGISwapChain *, UINT, UINT, UINT, DXGI_FORMAT, UINT);
detail::SlotHook g_present = {"Present"}, g_present1 = {"Present1"}, g_resize = {"ResizeBuffers"};

CRITICAL_SECTION g_cs;              // the render state below: Present and ResizeBuffers can come from different threads
IDXGISwapChain *g_chain;            // the swap chain the panel draws on (its window has the input hook); not owned
ID3D11Device *g_device;             // owned reference while ImGui's DX11 backend uses it
ID3D11DeviceContext *g_context;
ID3D11RenderTargetView *g_rtv;
bool g_backend, g_otherNoted;

void releaseTarget() { if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; } }

void shutdownBackend()
{
    releaseTarget();
    if (g_backend) { ImGui_ImplDX11_Shutdown(); g_backend = false; }
    if (g_context) { g_context->Release(); g_context = nullptr; }
    if (g_device) { g_device->Release(); g_device = nullptr; }
}

// The game window: the largest swap chain output wins, so a launcher or splash window does not keep the hook. Any API.
bool adopt(IDXGISwapChain *sc)
{
    if (sc == g_chain) return true;
    DXGI_SWAP_CHAIN_DESC d;
    if (FAILED(sc->GetDesc(&d)) || !d.OutputWindow) return false;
    RECT r;
    LONG area = GetClientRect(d.OutputWindow, &r) ? (r.right - r.left) * (r.bottom - r.top) : 0;
    LONG current = 0;
    HWND cw = dbce::panel::InputWindow();
    if (g_chain && cw && IsWindow(cw) && GetClientRect(cw, &r)) current = (r.right - r.left) * (r.bottom - r.top);
    if (g_chain && area <= current) return false;
    shutdownBackend();
    g_chain = sc;
    bool hooked = dbce::panel::AttachInput(d.OutputWindow);
    logf("overlay: drawing on the swap chain of window %p (%ldx%ld)%s", (void *)d.OutputWindow, (long)d.BufferDesc.Width,
         (long)d.BufferDesc.Height, hooked ? "; F6 input attached" : "; input hook FAILED");
    return true;
}

void render11(IDXGISwapChain *sc, ID3D11Device *dev)
{
    if (dev != g_device) {   // first frame, or the game recreated its device
        shutdownBackend();
        g_device = dev;
        g_device->AddRef();
        g_device->GetImmediateContext(&g_context);
        detail::ensureImGui();
        g_backend = ImGui_ImplDX11_Init(g_device, g_context);
        if (!g_backend) logf("overlay: ImGui DX11 backend could not start");
    }
    if (!g_backend) return;
    ID3D11Texture2D *back = nullptr;
    if (FAILED(sc->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&back)) || !back) return;
    D3D11_TEXTURE2D_DESC td;
    back->GetDesc(&td);
    if (!g_rtv && FAILED(g_device->CreateRenderTargetView(back, nullptr, &g_rtv))) g_rtv = nullptr;
    back->Release();
    if (!g_rtv) return;
    const int w = (int)td.Width, h = (int)td.Height;
    if (dbce::panel::Prepare(w, h)) { dbce::panel::BuildFonts(); ImGui_ImplDX11_InvalidateDeviceObjects(); }
    ImGui_ImplDX11_NewFrame();
    detail::drawFrame(w, h);
    // Draw on the back buffer and give the game its render targets back.
    ID3D11RenderTargetView *saved[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
    ID3D11DepthStencilView *savedDepth = nullptr;
    g_context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, saved, &savedDepth);
    g_context->OMSetRenderTargets(1, &g_rtv, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    g_context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, saved, savedDepth);
    for (auto *v : saved) if (v) v->Release();
    if (savedDepth) savedDepth->Release();
}

void render(IDXGISwapChain *sc)
{
    if (sc == g_chain) { dbce::panel::NoteFrame(); dbce::panel::PollDev(); }
    if (sc == g_chain && !dbce::panel::IsOpen()) return;   // closed: nothing per frame on the game's chain
    EnterCriticalSection(&g_cs);
    if (!adopt(sc) || sc != g_chain || !dbce::panel::IsOpen()) { LeaveCriticalSection(&g_cs); return; }
    ID3D11Device *dev = nullptr;
    if (SUCCEEDED(sc->GetDevice(__uuidof(ID3D11Device), (void **)&dev)) && dev) {
        render11(sc, dev);
        dev->Release();
    } else if (!(detail::render12 && detail::render12(sc)) && !g_otherNoted) {
        logf("overlay: the game's swap chain is neither D3D11 nor an enabled D3D12 one; panel not drawn");
        g_otherNoted = true;
    }
    LeaveCriticalSection(&g_cs);
}

// Each hook: entered again on the same thread means another overlay's hook and ours are calling each other (see
// detail::SlotHook); that inner call goes to the system's function instead, and so does every later one.
HRESULT STDMETHODCALLTYPE hkPresent(IDXGISwapChain *sc, UINT sync, UINT flags)
{
    static thread_local int depth;
    if (depth) { auto f = (PresentFn)detail::Reentered(g_present); return f ? f(sc, sync, flags) : S_OK; }
    detail::Depth in(depth);
    if (!(flags & DXGI_PRESENT_TEST)) render(sc);
    return ((PresentFn)g_present.next)(sc, sync, flags);
}
HRESULT STDMETHODCALLTYPE hkPresent1(IDXGISwapChain1 *sc, UINT sync, UINT flags, const DXGI_PRESENT_PARAMETERS *p)
{
    static thread_local int depth;
    if (depth) { auto f = (Present1Fn)detail::Reentered(g_present1); return f ? f(sc, sync, flags, p) : S_OK; }
    detail::Depth in(depth);
    if (!(flags & DXGI_PRESENT_TEST)) render(sc);
    return ((Present1Fn)g_present1.next)(sc, sync, flags, p);
}
HRESULT STDMETHODCALLTYPE hkResize(IDXGISwapChain *sc, UINT n, UINT w, UINT h, DXGI_FORMAT f, UINT flags)
{
    static thread_local int depth;
    if (depth) { auto fn = (ResizeFn)detail::Reentered(g_resize); return fn ? fn(sc, n, w, h, f, flags) : DXGI_ERROR_INVALID_CALL; }
    detail::Depth in(depth);
    // The panel's references to the back buffers must go before the buffers can be resized.
    EnterCriticalSection(&g_cs);
    if (sc == g_chain) { releaseTarget(); if (detail::resize12) detail::resize12(sc); }
    LeaveCriticalSection(&g_cs);
    return ((ResizeFn)g_resize.next)(sc, n, w, h, f, flags);
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
    bool ok = detail::PatchSlot(vt, 8, (void *)&hkPresent, g_present) && detail::PatchSlot(vt, 13, (void *)&hkResize, g_resize);
    IDXGISwapChain1 *sc1 = nullptr;
    bool ok1 = false;
    if (ok && SUCCEEDED(sc->QueryInterface(__uuidof(IDXGISwapChain1), (void **)&sc1)) && sc1) {
        void **vt1 = *(void ***)sc1;
        ok1 = vt1 == vt && detail::PatchSlot(vt, 22, (void *)&hkPresent1, g_present1);
        sc1->Release();
    }
    LeaveCriticalSection(&g_cs);
    sc->Release(); ctx->Release(); dev->Release();
    DestroyWindow(w);
    logf(ok ? "overlay: DXGI present hook installed%s (F6 opens the panel)" : "overlay: present hook could not be installed%s",
         ok1 ? ", Present1 too" : "");
    if (ok && detail::probe12) detail::probe12();
    return 0;
}
INIT_ONCE g_once = INIT_ONCE_STATIC_INIT;
BOOL CALLBACK initOnce(PINIT_ONCE, PVOID, PVOID *) { InitializeCriticalSection(&g_cs); return TRUE; }
} // namespace

bool StartD3D11(void (*log)(const char *))
{
    InitOnceExecuteOnce(&g_once, initOnce, nullptr, nullptr);
    detail::g_log = log;
    HANDLE t = CreateThread(nullptr, 0, worker, nullptr, 0, nullptr);
    if (!t) { logf("overlay: worker could not start (%lu)", GetLastError()); return false; }
    CloseHandle(t);
    return true;
}

}} // namespace dbce::overlay
