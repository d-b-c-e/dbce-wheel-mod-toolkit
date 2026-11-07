// Read-only D3D11 frame logger, installed as d3d11.dll next to a game exe (x86 or x64). It passes every call
// through to the system d3d11.dll and, for a few sampled frames, logs viewports, render targets and
// constant-buffer writes, in order, with the game-code address of each call. That ties a renderer's per-view setup (split-screen
// viewports, camera/projection constants) to the exe's code for static RE. It changes no call argument and draws
// nothing. Log: %LOCALAPPDATA%\dbce\d3d11log\d3d11log-<exe>-<pid>.txt. Sampled frames: the 2nd after any change of
// the per-frame viewport pattern (max 60), every 1800th frame, and 2 frames whenever capture.txt exists in the log
// folder (the file is then deleted). The log stops at 64 MB.
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <intrin.h>
#include <stdio.h>
#include <stdarg.h>
#include <share.h>

#if !defined(_M_IX86)
#error x86 only for now (DiRT 3); an x64 build needs the export jumps in a .asm file
#endif

#pragma comment(lib, "dxguid.lib")

#define PROXIED(X) \
    X(0, CreateDirect3D11DeviceFromDXGIDevice) X(1, CreateDirect3D11SurfaceFromDXGISurface) X(2, D3D11CoreCreateDevice) \
    X(3, D3D11CoreCreateLayeredDevice) X(4, D3D11CoreGetLayeredDeviceSize) X(5, D3D11CoreRegisterLayers) \
    X(6, D3D11CreateDeviceForD3D12) X(7, D3D11On12CreateDevice) X(8, D3DKMTCloseAdapter) X(9, D3DKMTCreateAllocation) \
    X(10, D3DKMTCreateContext) X(11, D3DKMTCreateDevice) X(12, D3DKMTCreateSynchronizationObject) \
    X(13, D3DKMTDestroyAllocation) X(14, D3DKMTDestroyContext) X(15, D3DKMTDestroyDevice) \
    X(16, D3DKMTDestroySynchronizationObject) X(17, D3DKMTEscape) X(18, D3DKMTGetContextSchedulingPriority) \
    X(19, D3DKMTGetDeviceState) X(20, D3DKMTGetDisplayModeList) X(21, D3DKMTGetMultisampleMethodList) \
    X(22, D3DKMTGetRuntimeData) X(23, D3DKMTGetSharedPrimaryHandle) X(24, D3DKMTLock) X(25, D3DKMTOpenAdapterFromHdc) \
    X(26, D3DKMTOpenResource) X(27, D3DKMTPresent) X(28, D3DKMTQueryAdapterInfo) X(29, D3DKMTQueryAllocationResidency) \
    X(30, D3DKMTQueryResourceInfo) X(31, D3DKMTRender) X(32, D3DKMTSetAllocationPriority) \
    X(33, D3DKMTSetContextSchedulingPriority) X(34, D3DKMTSetDisplayMode) X(35, D3DKMTSetDisplayPrivateDriverFormat) \
    X(36, D3DKMTSetGammaRamp) X(37, D3DKMTSetVidPnSourceOwner) X(38, D3DKMTSignalSynchronizationObject) \
    X(39, D3DKMTUnlock) X(40, D3DKMTWaitForSynchronizationObject) X(41, D3DKMTWaitForVerticalBlankEvent) \
    X(42, D3DPerformance_BeginEvent) X(43, D3DPerformance_EndEvent) X(44, D3DPerformance_GetStatus) \
    X(45, D3DPerformance_SetMarker) X(46, EnableFeatureLevelUpgrade) X(47, OpenAdapter10) X(48, OpenAdapter10_2)
#define PROXIED_COUNT 49

static HMODULE g_real;
extern "C" FARPROC g_procs[PROXIED_COUNT] = {};
static const char *g_procNames[PROXIED_COUNT] = {
#define NAME(i, n) #n,
    PROXIED(NAME)
#undef NAME
};

// Jump straight to the system export, so arguments and calling convention never matter.
#define STUB(i, n) extern "C" __declspec(naked) void stub_##n() { __asm { jmp dword ptr [g_procs + (i) * 4] } }
PROXIED(STUB)
#undef STUB

// ---------------------------------------------------------------------------------------------------------------
static CRITICAL_SECTION g_cs;
static FILE *g_log;
static long long g_logBytes;
static const long long kLogLimit = 64ll << 20;
static wchar_t g_dir[MAX_PATH];

static void dlog(const char *fmt, ...)
{
    if (!g_log || g_logBytes > kLogLimit) return;
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    int n = _vsnprintf_s(buf, sizeof buf, _TRUNCATE, fmt, ap);
    va_end(ap);
    if (n < 0) n = (int)strlen(buf);
    EnterCriticalSection(&g_cs);
    fwrite(buf, 1, n, g_log);
    fputc('\n', g_log);
    g_logBytes += n + 1;
    if (g_logBytes > kLogLimit) fputs("log limit reached\n", g_log);
    LeaveCriticalSection(&g_cs);
}

// "module+rva" for a code address.
static void where(void *addr, char *out, size_t cap)
{
    HMODULE mod = nullptr;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCWSTR)addr, &mod) && mod) {
        char path[MAX_PATH];
        DWORD n = GetModuleFileNameA(mod, path, MAX_PATH);
        const char *base = path;
        for (DWORD i = 0; i < n; i++) if (path[i] == '\\' || path[i] == '/') base = path + i + 1;
        sprintf_s(out, cap, "%s+0x%llx", base, (unsigned long long)((char *)addr - (char *)mod));
    } else {
        sprintf_s(out, cap, "0x%p", addr);
    }
}

// Caller of the hooked method, then whatever a frame-pointer walk finds (unreliable under FPO, marked "?").
static void callers(void *ret, char *out, size_t cap)
{
    where(ret, out, cap);
    void *frames[6] = {};
    USHORT n = RtlCaptureStackBackTrace(2, 6, frames, nullptr);
    for (USHORT i = 0; i < n; i++) {
        char w[160];
        where(frames[i], w, sizeof w);
        size_t len = strlen(out);
        if (len + strlen(w) + 3 < cap) sprintf_s(out + len, cap - len, " ?%s", w);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// Vtable patching with one original per (hook, vtable), so two classes sharing a slot index stay correct.
// No Draw* hooks: the D3D11 runtime rewrites those context vtable slots itself (seen in the smoke test).
enum Hook { H_RSVP, H_OMRT, H_MAP, H_UNMAP, H_UPDSUB, H_VSCB, H_PSCB,
            H_PRESENT, H_CREATESC, H_DEFERRED, H_CREATETEX2D, H_COUNT };
struct Slot { void **vt; void *orig; };
static Slot g_slots[H_COUNT][6];

static void *orig(Hook h, void *self)
{
    void **vt = *(void ***)self;
    for (auto &s : g_slots[h]) if (s.vt == vt) return s.orig;
    return g_slots[h][0].orig;
}

static void patch(Hook h, void *obj, int index, void *hook)
{
    if (!obj) return;
    void **vt = *(void ***)obj;
    if (vt[index] == hook) return;
    for (auto &s : g_slots[h]) {
        if (s.vt == vt) return;
        if (!s.vt) {
            DWORD old;
            if (!VirtualProtect(&vt[index], sizeof(void *), PAGE_EXECUTE_READWRITE, &old)) {
                dlog("patch failed: hook %d slot %d", h, index);
                return;
            }
            s.vt = vt;
            s.orig = vt[index];
            vt[index] = hook;
            VirtualProtect(&vt[index], sizeof(void *), old, &old);
            return;
        }
    }
    dlog("patch table full: hook %d", h);
}

// ---------------------------------------------------------------------------------------------------------------
// Frame state. The render thread drives all of it; the counters are plain because only sampled frames are logged.
static long g_frame;
static int g_capture;            // frames left to log, including the current one
static int g_autoLeft = 60;      // distinct stable viewport patterns still to log
static unsigned g_vpHash = 2166136261u, g_prevVpHash, g_watchHash;
static unsigned g_seen[64];      // stable patterns already logged
static int g_seenCount;
static int g_vpCalls, g_seq, g_cbDumps;  // g_seq numbers the logged events of a frame
static ID3D11DeviceContext *g_immediate;
struct PendingMap { ID3D11Resource *res; void *data; UINT size; char where[200]; };
static PendingMap g_maps[16];

static void hashBytes(const void *p, size_t n)
{
    const unsigned char *b = (const unsigned char *)p;
    for (size_t i = 0; i < n; i++) g_vpHash = (g_vpHash ^ b[i]) * 16777619u;
}

static UINT cbSize(ID3D11Resource *res)
{
    D3D11_RESOURCE_DIMENSION dim;
    res->GetType(&dim);
    if (dim != D3D11_RESOURCE_DIMENSION_BUFFER) return 0;
    D3D11_BUFFER_DESC d;
    ((ID3D11Buffer *)res)->GetDesc(&d);
    return (d.BindFlags & D3D11_BIND_CONSTANT_BUFFER) ? d.ByteWidth : 0;
}

static void dumpFloats(const char *tag, ID3D11Resource *res, const void *data, UINT size, const char *at)
{
    if (!data || g_cbDumps >= 800) return;
    g_cbDumps++;
    char line[1800];
    int len = sprintf_s(line, "  %s buf=%p size=%u seq=%d at %s :", tag, (void *)res, size, g_seq++, at);
    const float *f = (const float *)data;
    UINT count = size / 4 < 48 ? size / 4 : 48;
    for (UINT i = 0; i < count && len < (int)sizeof line - 24; i++) len += sprintf_s(line + len, sizeof line - len, " %.5g", f[i]);
    dlog("%s", line);
}

typedef void(STDMETHODCALLTYPE *RSSetViewportsFn)(ID3D11DeviceContext *, UINT, const D3D11_VIEWPORT *);
static void STDMETHODCALLTYPE hkRSSetViewports(ID3D11DeviceContext *c, UINT n, const D3D11_VIEWPORT *v)
{
    if (c == g_immediate && v) { hashBytes(&n, sizeof n); hashBytes(v, n * sizeof *v); g_vpCalls++; }
    if (g_capture && v) {
        char at[600];
        callers(_ReturnAddress(), at, sizeof at);
        char rects[600] = "";
        int len = 0;
        for (UINT i = 0; i < n && i < 8; i++)
            len += sprintf_s(rects + len, sizeof rects - len, " [%g,%g %gx%g z%g-%g]", v[i].TopLeftX, v[i].TopLeftY,
                             v[i].Width, v[i].Height, v[i].MinDepth, v[i].MaxDepth);
        dlog("VP%s n=%u%s seq=%d at %s", c == g_immediate ? "" : "(deferred)", n, rects, g_seq++, at);
    }
    ((RSSetViewportsFn)orig(H_RSVP, c))(c, n, v);
}

typedef void(STDMETHODCALLTYPE *OMSetRenderTargetsFn)(ID3D11DeviceContext *, UINT, ID3D11RenderTargetView *const *, ID3D11DepthStencilView *);
static void STDMETHODCALLTYPE hkOMSetRenderTargets(ID3D11DeviceContext *c, UINT n, ID3D11RenderTargetView *const *rtv, ID3D11DepthStencilView *dsv)
{
    if (g_capture) {
        char at[600];
        callers(_ReturnAddress(), at, sizeof at);
        UINT w = 0, h = 0, fmt = 0;
        void *tex = nullptr;
        if (n && rtv && rtv[0]) {
            ID3D11Resource *r = nullptr;
            rtv[0]->GetResource(&r);
            if (r) {
                D3D11_RESOURCE_DIMENSION dim;
                r->GetType(&dim);
                if (dim == D3D11_RESOURCE_DIMENSION_TEXTURE2D) {
                    D3D11_TEXTURE2D_DESC d;
                    ((ID3D11Texture2D *)r)->GetDesc(&d);
                    w = d.Width; h = d.Height; fmt = d.Format;
                }
                tex = r;
                r->Release();
            }
        }
        dlog("RT n=%u tex=%p %ux%u fmt=%u dsv=%p seq=%d at %s", n, tex, w, h, fmt, (void *)dsv, g_seq++, at);
    }
    ((OMSetRenderTargetsFn)orig(H_OMRT, c))(c, n, rtv, dsv);
}

typedef HRESULT(STDMETHODCALLTYPE *MapFn)(ID3D11DeviceContext *, ID3D11Resource *, UINT, D3D11_MAP, UINT, D3D11_MAPPED_SUBRESOURCE *);
static HRESULT STDMETHODCALLTYPE hkMap(ID3D11DeviceContext *c, ID3D11Resource *r, UINT sub, D3D11_MAP type, UINT flags, D3D11_MAPPED_SUBRESOURCE *m)
{
    HRESULT hr = ((MapFn)orig(H_MAP, c))(c, r, sub, type, flags, m);
    if (g_capture && SUCCEEDED(hr) && r && m && (type == D3D11_MAP_WRITE_DISCARD || type == D3D11_MAP_WRITE || type == D3D11_MAP_WRITE_NO_OVERWRITE)) {
        UINT size = cbSize(r);
        if (size) {
            for (auto &p : g_maps) {
                if (!p.res) {
                    p.res = r; p.data = m->pData; p.size = size;
                    callers(_ReturnAddress(), p.where, sizeof p.where);
                    break;
                }
            }
        }
    }
    return hr;
}

typedef void(STDMETHODCALLTYPE *UnmapFn)(ID3D11DeviceContext *, ID3D11Resource *, UINT);
static void STDMETHODCALLTYPE hkUnmap(ID3D11DeviceContext *c, ID3D11Resource *r, UINT sub)
{
    if (g_capture) {
        for (auto &p : g_maps) {
            if (p.res && p.res == r) {
                dumpFloats("CBmap", r, p.data, p.size, p.where);
                p.res = nullptr;
                break;
            }
        }
    }
    ((UnmapFn)orig(H_UNMAP, c))(c, r, sub);
}

typedef void(STDMETHODCALLTYPE *UpdateSubresourceFn)(ID3D11DeviceContext *, ID3D11Resource *, UINT, const D3D11_BOX *, const void *, UINT, UINT);
static void STDMETHODCALLTYPE hkUpdateSubresource(ID3D11DeviceContext *c, ID3D11Resource *r, UINT sub, const D3D11_BOX *box, const void *src, UINT rp, UINT dp)
{
    if (g_capture && r && src) {
        UINT size = cbSize(r);
        if (size) {
            char at[200];
            callers(_ReturnAddress(), at, sizeof at);
            dumpFloats(box ? "CBupd(box)" : "CBupd", r, src, size, at);
        }
    }
    ((UpdateSubresourceFn)orig(H_UPDSUB, c))(c, r, sub, box, src, rp, dp);
}

typedef void(STDMETHODCALLTYPE *SetCBFn)(ID3D11DeviceContext *, UINT, UINT, ID3D11Buffer *const *);
static void bindLog(const char *stage, UINT start, UINT n, ID3D11Buffer *const *b)
{
    char list[400] = "";
    int len = 0;
    for (UINT i = 0; i < n && i < 8 && b; i++) len += sprintf_s(list + len, sizeof list - len, " %u:%p", start + i, (void *)b[i]);
    dlog("  %sCB%s seq=%d", stage, list, g_seq++);
}
static void STDMETHODCALLTYPE hkVSSetConstantBuffers(ID3D11DeviceContext *c, UINT s, UINT n, ID3D11Buffer *const *b)
{
    if (g_capture) bindLog("VS", s, n, b);
    ((SetCBFn)orig(H_VSCB, c))(c, s, n, b);
}
static void STDMETHODCALLTYPE hkPSSetConstantBuffers(ID3D11DeviceContext *c, UINT s, UINT n, ID3D11Buffer *const *b)
{
    if (g_capture) bindLog("PS", s, n, b);
    ((SetCBFn)orig(H_PSCB, c))(c, s, n, b);
}

static void hookContext(ID3D11DeviceContext *c)
{
    patch(H_VSCB, c, 7, (void *)&hkVSSetConstantBuffers);
    patch(H_MAP, c, 14, (void *)&hkMap);
    patch(H_UNMAP, c, 15, (void *)&hkUnmap);
    patch(H_PSCB, c, 16, (void *)&hkPSSetConstantBuffers);
    patch(H_OMRT, c, 33, (void *)&hkOMSetRenderTargets);
    patch(H_RSVP, c, 44, (void *)&hkRSSetViewports);
    patch(H_UPDSUB, c, 48, (void *)&hkUpdateSubresource);
}

typedef HRESULT(STDMETHODCALLTYPE *PresentFn)(IDXGISwapChain *, UINT, UINT);
static HRESULT STDMETHODCALLTYPE hkPresent(IDXGISwapChain *sc, UINT sync, UINT flags)
{
    if (g_capture) {
        dlog("END frame=%ld viewportCalls=%d seq=%d cbDumps=%d pattern=%08x", g_frame, g_vpCalls, g_seq++, g_cbDumps, g_vpHash);
        g_capture--;
        for (auto &p : g_maps) p.res = nullptr;
        EnterCriticalSection(&g_cs);
        if (g_log) fflush(g_log);
        LeaveCriticalSection(&g_cs);
    }
    // A viewport pattern counts once it repeats on the next frame; each distinct stable pattern is logged once.
    unsigned h = g_vpHash;
    bool stableNew = false;
    if (g_frame > 2 && g_watchHash && h == g_watchHash) {
        stableNew = true;
        for (int i = 0; i < g_seenCount; i++) if (g_seen[i] == h) stableNew = false;
        g_watchHash = 0;
    } else if (h != g_prevVpHash) {
        g_watchHash = h;
    }
    g_prevVpHash = h;
    g_vpHash = 2166136261u;
    g_frame++;
    g_vpCalls = g_seq = g_cbDumps = 0;
    if (!g_capture) {
        const char *why = nullptr;
        wchar_t trigger[MAX_PATH];
        swprintf_s(trigger, L"%s\\capture.txt", g_dir);
        if (stableNew && g_autoLeft > 0) {
            g_autoLeft--;
            if (g_seenCount < 64) g_seen[g_seenCount++] = h;
            g_capture = 1;
            why = "new stable viewport pattern";
        }
        else if (g_frame % 1800 == 0) { g_capture = 1; why = "periodic"; }
        else if ((g_frame & 15) == 0 && GetFileAttributesW(trigger) != INVALID_FILE_ATTRIBUTES) {
            DeleteFileW(trigger);
            g_capture = 2;
            why = "capture.txt";
        }
        if (why) {
            DXGI_SWAP_CHAIN_DESC d = {};
            sc->GetDesc(&d);
            dlog("BEGIN frame=%ld (%s) backbuffer %ux%u windowed=%d", g_frame, why, d.BufferDesc.Width, d.BufferDesc.Height, d.Windowed);
        }
    }
    return ((PresentFn)orig(H_PRESENT, sc))(sc, sync, flags);
}

static void hookSwapChain(IDXGISwapChain *sc)
{
    if (!sc) return;
    DXGI_SWAP_CHAIN_DESC d = {};
    sc->GetDesc(&d);
    dlog("swap chain %p: %ux%u fmt=%u buffers=%u windowed=%d window=%p msaa=%u", (void *)sc, d.BufferDesc.Width,
         d.BufferDesc.Height, d.BufferDesc.Format, d.BufferCount, d.Windowed, (void *)d.OutputWindow, d.SampleDesc.Count);
    patch(H_PRESENT, sc, 8, (void *)&hkPresent);
}

typedef HRESULT(STDMETHODCALLTYPE *CreateSwapChainFn)(IDXGIFactory *, IUnknown *, DXGI_SWAP_CHAIN_DESC *, IDXGISwapChain **);
static HRESULT STDMETHODCALLTYPE hkCreateSwapChain(IDXGIFactory *f, IUnknown *dev, DXGI_SWAP_CHAIN_DESC *desc, IDXGISwapChain **sc)
{
    HRESULT hr = ((CreateSwapChainFn)orig(H_CREATESC, f))(f, dev, desc, sc);
    dlog("IDXGIFactory::CreateSwapChain hr=0x%08lx", (unsigned long)hr);
    if (SUCCEEDED(hr) && sc) hookSwapChain(*sc);
    return hr;
}

typedef HRESULT(STDMETHODCALLTYPE *CreateDeferredContextFn)(ID3D11Device *, UINT, ID3D11DeviceContext **);
static HRESULT STDMETHODCALLTYPE hkCreateDeferredContext(ID3D11Device *d, UINT flags, ID3D11DeviceContext **c)
{
    HRESULT hr = ((CreateDeferredContextFn)orig(H_DEFERRED, d))(d, flags, c);
    char at[200];
    callers(_ReturnAddress(), at, sizeof at);
    dlog("CreateDeferredContext hr=0x%08lx ctx=%p at %s (not hooked)", (unsigned long)hr, c ? (void *)*c : nullptr, at);
    return hr;
}

// Render-target and depth textures are created rarely, so every one is logged with its call chain (this finds the
// code that sizes per-view targets).
typedef HRESULT(STDMETHODCALLTYPE *CreateTexture2DFn)(ID3D11Device *, const D3D11_TEXTURE2D_DESC *, const D3D11_SUBRESOURCE_DATA *, ID3D11Texture2D **);
static HRESULT STDMETHODCALLTYPE hkCreateTexture2D(ID3D11Device *d, const D3D11_TEXTURE2D_DESC *desc, const D3D11_SUBRESOURCE_DATA *init, ID3D11Texture2D **tex)
{
    HRESULT hr = ((CreateTexture2DFn)orig(H_CREATETEX2D, d))(d, desc, init, tex);
    if (desc && (desc->BindFlags & (D3D11_BIND_RENDER_TARGET | D3D11_BIND_DEPTH_STENCIL))) {
        char at[600];
        callers(_ReturnAddress(), at, sizeof at);
        dlog("TEX2D %ux%u fmt=%u bind=0x%x msaa=%u mips=%u hr=0x%08lx tex=%p frame=%ld at %s", desc->Width, desc->Height,
             desc->Format, desc->BindFlags, desc->SampleDesc.Count, desc->MipLevels, (unsigned long)hr,
             tex && SUCCEEDED(hr) ? (void *)*tex : nullptr, g_frame, at);
    }
    return hr;
}

static void hookDevice(ID3D11Device *dev, ID3D11DeviceContext *ctx, char *at)
{
    if (!dev) return;
    dlog("device %p feature level 0x%x at %s", (void *)dev, dev->GetFeatureLevel(), at);
    patch(H_DEFERRED, dev, 27, (void *)&hkCreateDeferredContext);
    patch(H_CREATETEX2D, dev, 5, (void *)&hkCreateTexture2D);
    ID3D11DeviceContext *imm = ctx;
    if (!imm) dev->GetImmediateContext(&imm);
    if (imm) {
        g_immediate = imm;
        hookContext(imm);
        if (!ctx) imm->Release();
    }
    IDXGIDevice *dx = nullptr;
    if (SUCCEEDED(dev->QueryInterface(__uuidof(IDXGIDevice), (void **)&dx)) && dx) {
        IDXGIAdapter *ad = nullptr;
        if (SUCCEEDED(dx->GetAdapter(&ad)) && ad) {
            IDXGIFactory *f = nullptr;
            if (SUCCEEDED(ad->GetParent(__uuidof(IDXGIFactory), (void **)&f)) && f) {
                patch(H_CREATESC, f, 10, (void *)&hkCreateSwapChain);
                f->Release();
            }
            ad->Release();
        }
        dx->Release();
    }
}

// ---------------------------------------------------------------------------------------------------------------
typedef HRESULT(WINAPI *CreateDeviceFn)(IDXGIAdapter *, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL *, UINT, UINT,
                                       ID3D11Device **, D3D_FEATURE_LEVEL *, ID3D11DeviceContext **);
typedef HRESULT(WINAPI *CreateDeviceAndSwapChainFn)(IDXGIAdapter *, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL *, UINT,
                                                   UINT, const DXGI_SWAP_CHAIN_DESC *, IDXGISwapChain **, ID3D11Device **,
                                                   D3D_FEATURE_LEVEL *, ID3D11DeviceContext **);

extern "C" HRESULT WINAPI Proxy_D3D11CreateDevice(IDXGIAdapter *a, D3D_DRIVER_TYPE t, HMODULE sw, UINT flags, const D3D_FEATURE_LEVEL *fl,
                                                 UINT nfl, UINT sdk, ID3D11Device **dev, D3D_FEATURE_LEVEL *got, ID3D11DeviceContext **ctx)
{
    auto real = (CreateDeviceFn)GetProcAddress(g_real, "D3D11CreateDevice");
    if (!real) return E_FAIL;
    HRESULT hr = real(a, t, sw, flags, fl, nfl, sdk, dev, got, ctx);
    char at[200];
    callers(_ReturnAddress(), at, sizeof at);
    dlog("D3D11CreateDevice driver=%d flags=0x%x hr=0x%08lx", t, flags, (unsigned long)hr);
    if (SUCCEEDED(hr) && dev && *dev) hookDevice(*dev, ctx ? *ctx : nullptr, at);
    return hr;
}

extern "C" HRESULT WINAPI Proxy_D3D11CreateDeviceAndSwapChain(IDXGIAdapter *a, D3D_DRIVER_TYPE t, HMODULE sw, UINT flags,
                                                             const D3D_FEATURE_LEVEL *fl, UINT nfl, UINT sdk,
                                                             const DXGI_SWAP_CHAIN_DESC *desc, IDXGISwapChain **sc,
                                                             ID3D11Device **dev, D3D_FEATURE_LEVEL *got, ID3D11DeviceContext **ctx)
{
    auto real = (CreateDeviceAndSwapChainFn)GetProcAddress(g_real, "D3D11CreateDeviceAndSwapChain");
    if (!real) return E_FAIL;
    HRESULT hr = real(a, t, sw, flags, fl, nfl, sdk, desc, sc, dev, got, ctx);
    char at[200];
    callers(_ReturnAddress(), at, sizeof at);
    dlog("D3D11CreateDeviceAndSwapChain driver=%d flags=0x%x hr=0x%08lx", t, flags, (unsigned long)hr);
    if (SUCCEEDED(hr) && dev && *dev) hookDevice(*dev, ctx ? *ctx : nullptr, at);
    if (SUCCEEDED(hr) && sc && *sc) hookSwapChain(*sc);
    return hr;
}

BOOL WINAPI DllMain(HINSTANCE self, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(self);
        InitializeCriticalSection(&g_cs);
        wchar_t sys[MAX_PATH];
        GetSystemDirectoryW(sys, MAX_PATH);  // SysWOW64 for a 32-bit process
        wcscat_s(sys, L"\\d3d11.dll");
        g_real = LoadLibraryW(sys);
        for (int i = 0; i < PROXIED_COUNT; i++) g_procs[i] = g_real ? GetProcAddress(g_real, g_procNames[i]) : nullptr;
        DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", g_dir, MAX_PATH);
        if (n && n < MAX_PATH - 32) {
            wcscat_s(g_dir, L"\\dbce");
            CreateDirectoryW(g_dir, nullptr);
            wcscat_s(g_dir, L"\\d3d11log");
            CreateDirectoryW(g_dir, nullptr);
            wchar_t exe[MAX_PATH], path[MAX_PATH];
            GetModuleFileNameW(nullptr, exe, MAX_PATH);
            const wchar_t *base = wcsrchr(exe, L'\\') ? wcsrchr(exe, L'\\') + 1 : exe;
            swprintf_s(path, L"%s\\d3d11log-%s-%lu.txt", g_dir, base, GetCurrentProcessId());
            g_log = _wfsopen(path, L"w", _SH_DENYWR);
            if (g_log) setvbuf(g_log, nullptr, _IOFBF, 1 << 16);
            HMODULE exeMod = GetModuleHandleW(nullptr);
            dlog("d3d11log 2 in %ls pid %lu, exe base %p, system d3d11 %p", base, GetCurrentProcessId(), (void *)exeMod, (void *)g_real);
        }
    } else if (reason == DLL_PROCESS_DETACH) {
        if (g_log) { dlog("detach at frame %ld", g_frame); fclose(g_log); g_log = nullptr; }
    }
    return TRUE;
}
